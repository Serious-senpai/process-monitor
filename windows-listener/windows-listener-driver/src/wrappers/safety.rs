use core::ffi::c_void;
use core::ptr;

use wdk::nt_success;
use wdk_sys::ntddk::{
    IoCreateSymbolicLink, IoDeleteSymbolicLink, ObfDereferenceObject, PsLookupProcessByProcessId,
    PsSetCreateProcessNotifyRoutine,
};
use wdk_sys::{BOOLEAN, HANDLE, PASSIVE_LEVEL, PEPROCESS};

use crate::error::RuntimeError;
use crate::wrappers::irql::irql_requires;
use crate::wrappers::strings::UnicodeString;

/// Removes a symbolic link from the system.
///
/// See also: [`IoDeleteSymbolicLink`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iodeletesymboliclink)
pub fn delete_symbolic_link(name: &UnicodeString) -> Result<(), RuntimeError> {
    irql_requires(PASSIVE_LEVEL)?;

    let status = unsafe {
        let mut value = name.native().into_inner();

        // This implementation relies on the fact that `IoDeleteSymbolicLink` does not modify the string
        IoDeleteSymbolicLink(&mut value)
    };
    if !nt_success(status) {
        return Err(RuntimeError::Failure(status));
    }

    Ok(())
}

/// Sets up a symbolic link between a device object name and a user-visible name for the device.
///
/// See also: [`IoCreateSymbolicLink`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocreatesymboliclink)
pub fn create_symbolic_link(
    symbolic_link_name: &UnicodeString,
    device_name: &UnicodeString,
) -> Result<(), RuntimeError> {
    irql_requires(PASSIVE_LEVEL)?;

    let status = unsafe {
        let mut sym_name = symbolic_link_name.native().into_inner();
        let mut dev_name = device_name.native().into_inner();

        // This implementation relies on the fact that `IoCreateSymbolicLink` does not modify the string
        IoCreateSymbolicLink(&mut sym_name, &mut dev_name)
    };
    if !nt_success(status) {
        return Err(RuntimeError::Failure(status));
    }

    Ok(())
}

fn _set_create_process_notify<const REMOVE: u8>(
    handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
) -> Result<(), RuntimeError> {
    irql_requires(PASSIVE_LEVEL)?;

    let status = unsafe { PsSetCreateProcessNotifyRoutine(Some(handler), REMOVE) };
    if !nt_success(status) {
        return Err(RuntimeError::Failure(status));
    }

    Ok(())
}

pub fn add_create_process_notify(
    handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
) -> Result<(), RuntimeError> {
    _set_create_process_notify::<0>(handler)
}

pub fn remove_create_process_notify(
    handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
) -> Result<(), RuntimeError> {
    _set_create_process_notify::<1>(handler)
}

// fn _set_create_thread_notify<const REMOVE: bool>(
//     handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
// ) -> Result<(), RuntimeError> {
//     let status = unsafe {
//         if REMOVE {
//             PsRemoveCreateThreadNotifyRoutine(Some(handler))
//         } else {
//             PsSetCreateThreadNotifyRoutine(Some(handler))
//         }
//     };
//     if !nt_success(status) {
//         return Err(RuntimeError::Failure(status));
//     }

//     Ok(())
// }

// pub fn add_create_thread_notify(
//     handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
// ) -> Result<(), RuntimeError> {
//     irql_requires(PASSIVE_LEVEL)?;
//     _set_create_thread_notify::<false>(handler)
// }

// pub fn remove_create_thread_notify(
//     handler: unsafe extern "C" fn(HANDLE, HANDLE, BOOLEAN),
// ) -> Result<(), RuntimeError> {
//     irql_requires(APC_LEVEL)?;
//     _set_create_thread_notify::<true>(handler)
// }

pub struct ProcessInfo {
    _process: PEPROCESS,
}

impl ProcessInfo {
    fn new() -> Self {
        Self {
            _process: ptr::null_mut(),
        }
    }

    pub fn as_ptr(&self) -> *const PEPROCESS {
        &self._process
    }

    pub fn as_mut_ptr(&mut self) -> *mut PEPROCESS {
        &mut self._process
    }
}

impl Drop for ProcessInfo {
    fn drop(&mut self) {
        unsafe {
            ObfDereferenceObject(self._process as *mut c_void);
        }
    }
}

pub fn lookup_process_by_id(process_id: HANDLE) -> Option<ProcessInfo> {
    let mut process = ProcessInfo::new();
    if !nt_success(unsafe { PsLookupProcessByProcessId(process_id, process.as_mut_ptr()) }) {
        return None;
    }

    Some(process)
}
