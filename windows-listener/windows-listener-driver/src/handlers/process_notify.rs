use core::ffi::{CStr, c_void};
use core::sync::atomic::Ordering;
use core::{ptr, slice};

use ffi::{Event, EventData, EventType, NewProcess, StaticCommandName};
use wdk::nt_success;
use wdk_sys::ntddk::{ObfDereferenceObject, PsLookupProcessByProcessId};
use wdk_sys::{BOOLEAN, HANDLE, PEPROCESS};

use crate::config::DRIVER;
use crate::log;
use crate::state::DeviceExtension;
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

    let driver = DRIVER.load(Ordering::SeqCst);
    if let Some(driver) = unsafe { driver.as_ref() }
        && let Some(device) = unsafe { driver.DeviceObject.as_ref() }
        && let Some(inner) = unsafe {
            let extension = device.DeviceExtension as *const DeviceExtension;
            extension.as_ref().map(|e| &e.shared_memory)
        }
        && let Some(inner) = unsafe { inner.load(Ordering::SeqCst).as_ref() }
    {
        let mut process = _DereferenceGuard::new();
        if !nt_success(unsafe { PsLookupProcessByProcessId(process_id, process.ptr()) }) {
            return;
        }

        if let Ok(name) =
            unsafe { CStr::from_ptr(PsGetProcessImageFileName(*process.ptr()) as *const _) }
                .to_str()
        {
            let event = Event {
                pid: process_id as _,
                name: StaticCommandName::from(name),
                variant: EventType::NewProcess,
                data: EventData {
                    new_process: NewProcess {},
                },
            };

            if let Err(e) = inner.queue.send(unsafe {
                slice::from_raw_parts(&event as *const _ as *const u8, size_of::<Event>())
            }) {
                log!("Failed to write data to shared memory queue: {e}");
            } else {
                inner.event.set();
            }
        }
    }
}
