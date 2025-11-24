use ffi::win32::message::IOCTL_CLEAR_MONITOR;
use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP};

use crate::error::RuntimeError;
use crate::handlers::DeviceExtension;
use crate::handlers::irp::ioctl::IoctlHandler;

pub struct ClearMonitorHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _extension: &'a DeviceExtension,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
    _input_length: usize,
}

impl<'a> IoctlHandler<'a> for ClearMonitorHandler<'a> {
    const CODE: u32 = IOCTL_CLEAR_MONITOR;

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
        let mut threshold = self._extension.thresholds.acquire();
        threshold.clear();

        Ok(())
    }
}
