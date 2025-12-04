use alloc::string::ToString;
use core::ffi::CStr;
use core::sync::atomic::Ordering;

use ffi::NewProcess;
use ffi::win32::event::{WindowsEvent, WindowsEventData};
use wdk_sys::{BOOLEAN, HANDLE};

use crate::log;
use crate::state::DRIVER_STATE;
use crate::wrappers::bindings::PsGetProcessImageFileName;
use crate::wrappers::safety::lookup_process_by_id;

/// # Safety
/// Must be called by the OS.
pub unsafe extern "C" fn process_notify(_: HANDLE, process_id: HANDLE, create: BOOLEAN) {
    if create == 0 {
        // We don't care about process exits
        return;
    }

    let shared_memory = DRIVER_STATE.shared_memory.load(Ordering::Acquire);
    if let Some(shared_memory) = unsafe { shared_memory.as_ref() } {
        let process = match lookup_process_by_id(process_id) {
            Some(process) => process,
            None => return,
        };

        if let Ok(name) =
            unsafe { CStr::from_ptr(PsGetProcessImageFileName(*process.as_ptr())) }.to_str()
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
