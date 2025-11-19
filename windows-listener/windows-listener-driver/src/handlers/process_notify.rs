use core::slice;
use core::sync::atomic::Ordering;

use ffi::{Event, EventData, EventType, NewProcess, StaticCommandName};
use wdk_sys::{BOOLEAN, HANDLE};

use crate::config::DRIVER;
use crate::log;
use crate::state::DeviceExtension;

/// # Safety
/// Must be called by the OS.
pub unsafe extern "C" fn process_notify(parent_id: HANDLE, process_id: HANDLE, create: BOOLEAN) {
    let parent_id = parent_id as usize;
    let process_id = process_id as usize;
    let driver = DRIVER.load(Ordering::SeqCst);
    if let Some(driver) = unsafe { driver.as_ref() }
        && let Some(device) = unsafe { driver.DeviceObject.as_ref() }
        && let Some(inner) = unsafe {
            let extension = device.DeviceExtension as *const DeviceExtension;
            extension.as_ref().map(|e| &e.shared_memory)
        }
        && let Some(inner) = unsafe { inner.load(Ordering::SeqCst).as_ref() }
    {
        let event = Event {
            pid: process_id.try_into().unwrap_or(u32::MAX),
            name: StaticCommandName::from("dummy"), // TODO: get process name
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
