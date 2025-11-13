use alloc::vec::Vec;
use core::ptr;

use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, IRP_MJ_READ};

use crate::error::RuntimeError;
use crate::handlers::irp::IrpHandler;
use crate::state::DeviceExtension;

pub struct ReadHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _extension: &'a DeviceExtension,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
    _length: usize,
}

impl<'a> IrpHandler<'a> for ReadHandler<'a> {
    const CODE: u32 = IRP_MJ_READ;

    fn new(
        device: &'a DEVICE_OBJECT,
        extension: &'a DeviceExtension,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        let length = unsafe { irpsp.Parameters.Read.Length };
        Ok(Self {
            _device: device,
            _extension: extension,
            _irp: irp,
            _irpsp: irpsp,
            _length: length.try_into()?,
        })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        let mut inner = self._extension.inner.acquire();
        let requested = self._length.min(inner.queue.len());

        let src = inner.queue.drain(..requested).collect::<Vec<u8>>();
        unsafe {
            let dst = self._irp.AssociatedIrp.SystemBuffer as *mut u8;
            ptr::copy_nonoverlapping(src.as_ptr(), dst, src.len());
        }

        self._irp.IoStatus.Information = src.len().try_into()?;

        Ok(())
    }
}
