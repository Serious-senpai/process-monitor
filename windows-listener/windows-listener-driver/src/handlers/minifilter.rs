use alloc::boxed::Box;
use alloc::string::ToString;
use core::ffi::{CStr, c_void};
use core::ptr;
use core::sync::atomic::Ordering;

use ffi::win32::event::{WindowsEvent, WindowsEventData};
use ffi::{Metric, StaticCommandName, Violation};
use wdk_sys::ntddk::{KeQueryPerformanceCounter, PsGetProcessId};
use wdk_sys::{IRP_MJ_READ, IRP_MJ_WRITE, PEPROCESS};
use windows::Wdk::Storage::FileSystem::Minifilters::{
    FLT_CALLBACK_DATA, FLT_OPERATION_REGISTRATION, FLT_POSTOP_CALLBACK_STATUS,
    FLT_POSTOP_FINISHED_PROCESSING, FLT_POSTOP_MORE_PROCESSING_REQUIRED, FLT_REGISTRATION,
    FLT_REGISTRATION_VERSION, FLT_RELATED_OBJECTS, FltDoCompletionProcessingWhenSafe,
    FltGetRequestorProcess, FltUnregisterFilter, IRP_MJ_OPERATION_END, PFLT_FILTER,
};
use windows::Win32::Foundation::{NTSTATUS, STATUS_SUCCESS};

use crate::log;
use crate::state::DRIVER_STATE;
use crate::wrappers::bindings::PsGetProcessImageFileName;

unsafe extern "system" fn _minifilter_safe_postop(
    _: *mut FLT_CALLBACK_DATA,
    _: *const FLT_RELATED_OBJECTS,
    context: *const c_void,
    _: u32,
) -> FLT_POSTOP_CALLBACK_STATUS {
    let event = unsafe { Box::from_raw(context as *mut WindowsEvent) };
    let shared_memory = DRIVER_STATE.shared_memory.load(Ordering::Acquire);

    if let Some(shared_memory) = unsafe { shared_memory.as_ref() } {
        match postcard::to_allocvec_cobs(&event) {
            Ok(data) => {
                if let Err(e) = shared_memory.queue.send(&data) {
                    log!("Failed to write data to shared memory queue: {e}");
                } else {
                    shared_memory.event.set();
                }
            }
            Err(e) => {
                log!("Failed to serialize {:?}: {e}", event);
            }
        }
    }

    FLT_POSTOP_FINISHED_PROCESSING
}

pub const FILTER_REGISTRATION: FLT_REGISTRATION = FLT_REGISTRATION {
    Size: size_of::<FLT_REGISTRATION>() as _,
    Version: FLT_REGISTRATION_VERSION as _,
    Flags: 0,
    ContextRegistration: ptr::null_mut(),
    OperationRegistration: _FILTER_CALLBACKS.as_ptr(),
    FilterUnloadCallback: Some(_minifilter_unload),
    InstanceSetupCallback: None,
    InstanceQueryTeardownCallback: Some(_minifilter_teardown),
    InstanceTeardownStartCallback: None,
    InstanceTeardownCompleteCallback: None,
    GenerateFileNameCallback: None,
    NormalizeNameComponentCallback: None,
    NormalizeContextCleanupCallback: None,
    TransactionNotificationCallback: None,
    NormalizeNameComponentExCallback: None,
    SectionNotificationCallback: None,
};

const _FILTER_CALLBACKS: [FLT_OPERATION_REGISTRATION; 3] = [
    FLT_OPERATION_REGISTRATION {
        MajorFunction: IRP_MJ_READ as _,
        Flags: 0,
        PreOperation: None,
        PostOperation: Some(_minifilter_postop),
        Reserved1: ptr::null_mut(),
    },
    FLT_OPERATION_REGISTRATION {
        MajorFunction: IRP_MJ_WRITE as _,
        Flags: 0,
        PreOperation: None,
        PostOperation: Some(_minifilter_postop),
        Reserved1: ptr::null_mut(),
    },
    FLT_OPERATION_REGISTRATION {
        MajorFunction: IRP_MJ_OPERATION_END as _,
        Flags: 0,
        PreOperation: None,
        PostOperation: None,
        Reserved1: ptr::null_mut(),
    },
];

unsafe extern "system" fn _minifilter_postop(
    data: *mut FLT_CALLBACK_DATA,
    flt_objects: *const FLT_RELATED_OBJECTS,
    _: *const c_void,
    _: u32,
) -> FLT_POSTOP_CALLBACK_STATUS {
    let thresholds = DRIVER_STATE.thresholds.load(Ordering::Acquire);
    let ticks_per_ms = DRIVER_STATE.ticks_per_ms.load(Ordering::Acquire);
    let disk_io = DRIVER_STATE.disk_io.load(Ordering::Acquire);

    if let Some(thresholds) = unsafe { thresholds.as_ref() }
        && ticks_per_ms > 0
        && let Some(disk_io) = unsafe { disk_io.as_ref() }
    {
        let process = unsafe { FltGetRequestorProcess(data) }.0 as PEPROCESS;

        if let Ok(name) = unsafe { CStr::from_ptr(PsGetProcessImageFileName(process)) }.to_str() {
            let static_name = StaticCommandName::from(name);

            if let Some(threshold) = {
                let thresholds = thresholds.acquire();
                thresholds
                    .get(&static_name)
                    .map(|t| t.thresholds[Metric::Disk as usize])
            } {
                let pid = unsafe { PsGetProcessId(process) } as u32;
                let timestamp_ms = (unsafe { KeQueryPerformanceCounter(ptr::null_mut()).QuadPart }
                    / ticks_per_ms) as u64;

                let size = if let Some(data) = unsafe { data.as_mut() }
                    && let Some(io) = unsafe { data.Iopb.as_ref() }
                {
                    match io.MajorFunction.into() {
                        IRP_MJ_READ => unsafe { io.Parameters.Read.Length },
                        IRP_MJ_WRITE => unsafe { io.Parameters.Write.Length },
                        _ => 0,
                    }
                } else {
                    0
                };

                let mut disk_io = disk_io.acquire();
                match disk_io.get_mut(&(static_name, pid)) {
                    Some(packed) => {
                        *packed += u128::from(size);

                        let dt = timestamp_ms.saturating_sub((*packed >> 64) as u64);
                        if dt >= 1000 {
                            let old = *packed;
                            *packed = u128::from(timestamp_ms) << 64;

                            let accumulated = old & u128::from(u64::MAX);
                            log!(
                                "Received disk I/O metric event from PID {pid}: size = {size}, accumulated = {accumulated}, dt = {dt} ms, threshold = {threshold}, timestamp_ms = {timestamp_ms}"
                            );

                            let rate = 1000 * accumulated / u128::from(dt);
                            if rate >= u128::from(threshold) {
                                let context = Box::new(WindowsEvent {
                                    pid,
                                    name: name.to_string(),
                                    data: WindowsEventData::Violation(Violation {
                                        metric: Metric::Disk,
                                        value: rate as u32,
                                        threshold,
                                    }),
                                });

                                let mut status = FLT_POSTOP_FINISHED_PROCESSING;
                                let synchronous = unsafe {
                                    FltDoCompletionProcessingWhenSafe(
                                        data,
                                        flt_objects,
                                        Some(Box::into_raw(context) as *const c_void),
                                        0,
                                        Some(_minifilter_safe_postop),
                                        &mut status,
                                    )
                                };

                                if synchronous {
                                    return status;
                                } else {
                                    return FLT_POSTOP_MORE_PROCESSING_REQUIRED;
                                }
                            }
                        }
                    }
                    None => {
                        disk_io.put(
                            (static_name, pid),
                            u128::from(timestamp_ms) << 64 | u128::from(size),
                        );
                    }
                }
            }
        }
    }

    FLT_POSTOP_FINISHED_PROCESSING
}

unsafe extern "system" fn _minifilter_unload(_: u32) -> NTSTATUS {
    let filter = DRIVER_STATE.minifilter.swap(0, Ordering::AcqRel);
    if filter != 0 {
        unsafe {
            FltUnregisterFilter(PFLT_FILTER(filter));
        }
    }

    STATUS_SUCCESS
}

unsafe extern "system" fn _minifilter_teardown(_: *const FLT_RELATED_OBJECTS, _: u32) -> NTSTATUS {
    STATUS_SUCCESS
}
