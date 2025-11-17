use core::ffi::c_void;
use core::ptr;

use wdk::nt_success;
use wdk_sys::_MODE::UserMode;
use wdk_sys::ntddk::{KeSetEvent, ObReferenceObjectByHandle, ObfDereferenceObject};
use wdk_sys::{_OBJECT_TYPE, EVENT_MODIFY_STATE, HANDLE, SYNCHRONIZE};

use crate::error::RuntimeError;

pub struct UserObjectMap {
    _object: *mut c_void,
}

impl UserObjectMap {
    pub fn from_userspace(
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

impl Drop for UserObjectMap {
    fn drop(&mut self) {
        unsafe {
            ObfDereferenceObject(self._object);
        }
    }
}

pub struct UserEventObject {
    _object: UserObjectMap,
}

impl UserEventObject {
    pub fn from_userspace(handle: HANDLE) -> Result<Self, RuntimeError> {
        let object = UserObjectMap::from_userspace(
            handle,
            EVENT_MODIFY_STATE | SYNCHRONIZE,
            ptr::null_mut(),
            UserMode.try_into()?,
        )?;

        Ok(Self { _object: object })
    }

    pub fn set(&self) {
        unsafe {
            KeSetEvent(self._object.get() as *mut _, 0, 0);
        }
    }
}
