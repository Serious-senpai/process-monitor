use alloc::boxed::Box;
use core::sync::atomic::Ordering;

use wdk_sys::{DEVICE_OBJECT, HANDLE, IO_STACK_LOCATION, IRP, STATUS_INVALID_PARAMETER};

use crate::error::RuntimeError;
use crate::handlers::DeviceExtension;
use crate::handlers::irp::ioctl::IoctlHandler;
use crate::mpsc::UserChannelMap;
use crate::state::SharedMemory;
use crate::wrappers::user_object::UserEventObject;

#[repr(C)]
pub struct MemoryInitializeMessage {
    pub section: HANDLE,
    pub event: HANDLE,
}

pub struct MemoryInitializeHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _extension: &'a DeviceExtension,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
    _input_length: usize,
}

impl<'a> IoctlHandler<'a> for MemoryInitializeHandler<'a> {
    const CODE: u32 = super::IOCTL_MEMORY_INITIALIZE;

    fn new(
        device: &'a DEVICE_OBJECT,
        extension: &'a DeviceExtension,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        let input_length = unsafe { irpsp.Parameters.DeviceIoControl.InputBufferLength };
        Ok(Self {
            _device: device,
            _extension: extension,
            _irp: irp,
            _irpsp: irpsp,
            _input_length: input_length.try_into()?,
        })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        if self._input_length != size_of::<MemoryInitializeMessage>() {
            return Err(RuntimeError::Failure(STATUS_INVALID_PARAMETER));
        }

        let input = match unsafe {
            let ptr = self._irp.AssociatedIrp.SystemBuffer as *const MemoryInitializeMessage;
            ptr.as_ref()
        } {
            Some(input) => input,
            None => return Err(RuntimeError::Failure(STATUS_INVALID_PARAMETER)),
        };

        let shared_memory = Box::into_raw(Box::new(SharedMemory {
            queue: UserChannelMap::from_userspace(input.section)?,
            event: UserEventObject::from_userspace(input.event)?,
        }));

        let old = self
            ._extension
            .shared_memory
            .swap(shared_memory, Ordering::SeqCst);

        if !old.is_null() {
            unsafe {
                let _ = Box::from_raw(old);
            }
        }

        Ok(())
    }
}
