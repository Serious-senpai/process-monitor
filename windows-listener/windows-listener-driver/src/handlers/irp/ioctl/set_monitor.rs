use ffi::win32::message::{IOCTL_SET_MONITOR, SetMonitor};
use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, STATUS_INVALID_PARAMETER};

use crate::DeviceExtension;
use crate::error::RuntimeError;
use crate::handlers::irp::ioctl::IoctlHandler;

pub struct SetMonitorHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _extension: &'a DeviceExtension,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
    _input_length: usize,
}

impl<'a> IoctlHandler<'a> for SetMonitorHandler<'a> {
    const CODE: u32 = IOCTL_SET_MONITOR;

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
        if self._input_length != size_of::<SetMonitor>() {
            return Err(RuntimeError::Failure(STATUS_INVALID_PARAMETER));
        }

        let input = match unsafe {
            let ptr = self._irp.AssociatedIrp.SystemBuffer as *const SetMonitor;
            ptr.as_ref()
        } {
            Some(input) => input,
            None => return Err(RuntimeError::Failure(STATUS_INVALID_PARAMETER)),
        };

        let mut threshold = self._extension.thresholds.acquire();
        threshold.insert(input.name, input.threshold);

        Ok(())
    }
}
