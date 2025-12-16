use alloc::string::ToString;
use core::ffi::CStr;
use core::ptr;
use core::sync::atomic::Ordering;

use ffi::win32::event::{WindowsEvent, WindowsEventData};
use ffi::{Metric, StaticCommandName, Violation};
use wdk_sys::ntddk::KeQueryPerformanceCounter;
use wdk_sys::{HANDLE, PDEVICE_OBJECT};

use crate::log;
use crate::state::DRIVER_STATE;
use crate::wrappers::bindings::PsGetProcessImageFileName;
use crate::wrappers::safety::lookup_process_by_id;

/// # Safety
/// This callback is typically executed at DISPATCH_LEVEL
pub unsafe extern "C" fn wfp_callback(_: PDEVICE_OBJECT, pid: u64, size: usize) {
    let pid = match u32::try_from(pid) {
        Ok(p) => p,
        Err(_) => return,
    };

    let thresholds = DRIVER_STATE.thresholds.load(Ordering::Acquire);
    let ticks_per_ms = DRIVER_STATE.ticks_per_ms.load(Ordering::Acquire);
    let network_io = DRIVER_STATE.network_io.load(Ordering::Acquire);
    let shared_memory = DRIVER_STATE.shared_memory.load(Ordering::Acquire);

    if let Some(thresholds) = unsafe { thresholds.as_ref() }
        && ticks_per_ms > 0
        && let Some(network_io) = unsafe { network_io.as_ref() }
        && let Some(shared_memory) = unsafe { shared_memory.as_ref() }
    {
        let process = match lookup_process_by_id(pid as HANDLE) {
            Some(process) => process,
            None => return,
        };

        if let Ok(name) =
            unsafe { CStr::from_ptr(PsGetProcessImageFileName(*process.as_ptr())) }.to_str()
        {
            let static_name = StaticCommandName::from(name);

            if let Some(threshold) = {
                let thresholds = thresholds.acquire();
                thresholds
                    .get(&static_name)
                    .map(|t| t.thresholds[Metric::Network as usize])
            } {
                let timestamp_ms = (unsafe { KeQueryPerformanceCounter(ptr::null_mut()).QuadPart }
                    / ticks_per_ms) as u64;

                let mut network_io = network_io.acquire();
                match network_io.get_mut(&(static_name, pid)) {
                    Some(packed) => {
                        *packed += size as u128;

                        let dt = timestamp_ms.saturating_sub((*packed >> 64) as u64);
                        if dt >= 1000 {
                            let old = *packed;
                            *packed = u128::from(timestamp_ms) << 64;

                            let accumulated = old & u128::from(u64::MAX);
                            log!(
                                "Received network I/O metric event from PID {pid}: size = {size}, accumulated = {accumulated}, dt = {dt} ms, threshold = {threshold}, timestamp_ms = {timestamp_ms}"
                            );

                            let rate = 1000 * accumulated / u128::from(dt);
                            if rate >= u128::from(threshold) {
                                let event = WindowsEvent {
                                    pid,
                                    name: name.to_string(),
                                    data: WindowsEventData::Violation(Violation {
                                        metric: Metric::Network,
                                        value: rate as u32,
                                        threshold,
                                    }),
                                };

                                match postcard::to_allocvec_cobs(&event) {
                                    Ok(data) => {
                                        if let Err(e) = shared_memory.queue.send(&data) {
                                            log!(
                                                "Failed to write data to shared memory queue: {e}"
                                            );
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
                    None => {
                        network_io.put(
                            (static_name, pid),
                            u128::from(timestamp_ms) << 64 | (size as u128),
                        );
                    }
                }
            }
        }
    }
}
