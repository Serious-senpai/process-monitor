use core::ffi::{CStr, c_void};
use core::ptr;
use core::sync::atomic::Ordering;

use wdk_sys::ntddk::PsGetProcessId;
use wdk_sys::{IRP_MJ_READ, IRP_MJ_WRITE, PEPROCESS};
use windows::Wdk::Storage::FileSystem::Minifilters::{
    FLT_CALLBACK_DATA, FLT_OPERATION_REGISTRATION, FLT_POSTOP_CALLBACK_STATUS,
    FLT_POSTOP_FINISHED_PROCESSING, FLT_REGISTRATION, FLT_REGISTRATION_VERSION,
    FLT_RELATED_OBJECTS, FltGetRequestorProcess, FltUnregisterFilter, IRP_MJ_OPERATION_END,
    PFLT_FILTER,
};
use windows::Win32::Foundation::{NTSTATUS, STATUS_SUCCESS};

use crate::config::DRIVER;
use crate::log;
use crate::state::DeviceExtension;
use crate::wrappers::bindings::PsGetProcessImageFileName;

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
    _: *const FLT_RELATED_OBJECTS,
    _: *const c_void,
    _: u32,
) -> FLT_POSTOP_CALLBACK_STATUS {
    let process = unsafe { FltGetRequestorProcess(data) }.0 as PEPROCESS;
    if let Ok(name) = unsafe { CStr::from_ptr(PsGetProcessImageFileName(process)) }.to_str() {
        let pid = unsafe { PsGetProcessId(process) } as usize;

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

        log!(
            "_minifilter_postop pid={pid:#x} name={:?} size={}",
            name,
            size
        );
    }

    FLT_POSTOP_FINISHED_PROCESSING
}

unsafe extern "system" fn _minifilter_unload(_: u32) -> NTSTATUS {
    let driver = DRIVER.load(Ordering::SeqCst);
    if let Some(driver) = unsafe { driver.as_ref() }
        && let Some(device) = unsafe { driver.DeviceObject.as_ref() }
        && let Some(filter) = unsafe {
            let extension = device.DeviceExtension as *const DeviceExtension;
            extension
                .as_ref()
                .map(|e| e.minifilter.swap(ptr::null_mut(), Ordering::SeqCst))
        }
    {
        unsafe {
            FltUnregisterFilter(PFLT_FILTER(filter as _));
        }
    }

    STATUS_SUCCESS
}

unsafe extern "system" fn _minifilter_teardown(_: *const FLT_RELATED_OBJECTS, _: u32) -> NTSTATUS {
    STATUS_SUCCESS
}
