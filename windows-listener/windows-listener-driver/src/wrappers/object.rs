use core::ffi::c_void;
use core::ptr;

use wdk::nt_success;
use wdk_sys::ntddk::{KeSetEvent, ObReferenceObjectByHandle, ObfDereferenceObject};
use wdk_sys::{_OBJECT_TYPE, EVENT_MODIFY_STATE, HANDLE, SYNCHRONIZE};

use crate::error::RuntimeError;

pub struct KernelObject {
    _object: *mut c_void,
}

impl KernelObject {
    pub fn from_handle(
        handle: HANDLE,
        desired_access: u32,
        object_type: *mut _OBJECT_TYPE,
        access_mode: i8,
    ) -> Result<Self, RuntimeError> {
        let mut object = ptr::null_mut();
        let status = unsafe {
            ObReferenceObjectByHandle(
                handle,
                desired_access,
                object_type,
                access_mode,
                &mut object,
                ptr::null_mut(),
            )
        };
        if !nt_success(status) {
            return Err(RuntimeError::Failure(status));
        }

        Ok(Self { _object: object })
    }

    pub fn get(&self) -> *mut c_void {
        self._object
    }
}

impl Drop for KernelObject {
    fn drop(&mut self) {
        unsafe {
            ObfDereferenceObject(self._object);
        }
    }
}

pub struct KernelEvent {
    _object: KernelObject,
}

impl KernelEvent {
    pub fn from_handle(handle: HANDLE, access_mode: i32) -> Result<Self, RuntimeError> {
        let object = KernelObject::from_handle(
            handle,
            EVENT_MODIFY_STATE | SYNCHRONIZE,
            ptr::null_mut(),
            access_mode.try_into()?,
        )?;

        Ok(Self { _object: object })
    }

    pub fn set(&self) {
        unsafe {
            KeSetEvent(self._object.get() as *mut _, 0, 0);
        }
    }
}
