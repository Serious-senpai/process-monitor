use core::sync::atomic::Ordering;

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
        let event =
            alloc::format!("Process {{ pid: {process_id}, ppid: {parent_id}, create: {create} }}"); // TODO: replace with proper struct

        match postcard::to_allocvec_cobs(&event) {
            Ok(data) => {
                if let Err(e) = inner.queue.send(&data) {
                    log!("Failed to write data to shared memory queue: {e}");
                } else {
                    inner.event.set();
                }
            }
            Err(e) => {
                log!("Failed to serialize process: {e}");
            }
        }
    }
}
