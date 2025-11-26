use alloc::string::ToString;
use core::ffi::{CStr, c_void};
use core::ptr;
use core::sync::atomic::Ordering;

use ffi::NewProcess;
use ffi::win32::event::{WindowsEvent, WindowsEventData};
use wdk::nt_success;
use wdk_sys::ntddk::{ObfDereferenceObject, PsLookupProcessByProcessId};
use wdk_sys::{BOOLEAN, HANDLE, PEPROCESS};

use crate::log;
use crate::state::DRIVER_STATE;
use crate::wrappers::bindings::PsGetProcessImageFileName;

struct _DereferenceGuard {
    _process: PEPROCESS,
}

impl _DereferenceGuard {
    fn new() -> Self {
        Self {
            _process: ptr::null_mut(),
        }
    }

    fn ptr(&mut self) -> *mut PEPROCESS {
        &mut self._process
    }
}

impl Drop for _DereferenceGuard {
    fn drop(&mut self) {
        unsafe {
            ObfDereferenceObject(self._process as *mut c_void);
        }
    }
}

/// # Safety
/// Must be called by the OS.
pub unsafe extern "C" fn process_notify(_: HANDLE, process_id: HANDLE, create: BOOLEAN) {
    if create == 0 {
        // We don't care about process exits
        return;
    }

    let shared_memory = DRIVER_STATE.shared_memory.load(Ordering::Acquire);
    if let Some(shared_memory) = unsafe { shared_memory.as_ref() } {
        let mut process = _DereferenceGuard::new();
        if !nt_success(unsafe { PsLookupProcessByProcessId(process_id, process.ptr()) }) {
            return;
        }

        if let Ok(name) =
            unsafe { CStr::from_ptr(PsGetProcessImageFileName(*process.ptr())) }.to_str()
        {
            let event = WindowsEvent {
                pid: process_id as _,
                name: name.to_string(),
                data: WindowsEventData::NewProcess(NewProcess {}),
            };

            match postcard::to_allocvec_cobs(&event) {
                Ok(data) => {
                    if let Err(e) = shared_memory.queue.send(&data) {
                        log!("Failed to write data to shared memory queue: {e}");
                    } else {
                        shared_memory.event.set();
                    }
                }
                Err(e) => {
                    log!("Failed to serialize {event:?}: {e}");
                }
            }
        }
    }
}
