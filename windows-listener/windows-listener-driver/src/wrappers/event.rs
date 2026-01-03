use wdk::nt_success;
use wdk_sys::_EVENT_TYPE::SynchronizationEvent;
use wdk_sys::_MODE::KernelMode;
use wdk_sys::ntddk::{ZwClose, ZwCreateEvent};
use wdk_sys::{EVENT_ALL_ACCESS, HANDLE, POBJECT_ATTRIBUTES};

use crate::error::RuntimeError;
use crate::log;
use crate::wrappers::object::KernelEvent;

pub struct Event {
    _handle: HANDLE,
}

impl Event {
    pub fn new(attributes: POBJECT_ATTRIBUTES) -> Result<Self, RuntimeError> {
        let mut result = Self {
            _handle: HANDLE::default(),
        };

        let status = unsafe {
            ZwCreateEvent(
                &mut result._handle,
                EVENT_ALL_ACCESS,
                attributes,
                SynchronizationEvent,
                0,
            )
        };
        if !nt_success(status) {
            log!("Failed to call ZwCreateEvent: {status}");
            return Err(RuntimeError::Failure(status));
        }

        Ok(result)
    }

    pub fn get_object(&self) -> Result<KernelEvent, RuntimeError> {
        KernelEvent::from_handle(self._handle, KernelMode)
    }
}

impl Drop for Event {
    fn drop(&mut self) {
        unsafe {
            if self._handle != HANDLE::default() {
                let _ = ZwClose(self._handle);
            }
        }
    }
}
