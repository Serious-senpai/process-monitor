use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, IRP_MJ_CREATE};

use crate::error::RuntimeError;
use crate::handlers::irp::IrpHandler;
use crate::state::DeviceExtension;

pub struct CreateHandler<'a> {
    _irp: &'a mut IRP,
}

impl<'a> IrpHandler<'a> for CreateHandler<'a> {
    const CODE: u32 = IRP_MJ_CREATE;

    fn new(
        _: &'a DEVICE_OBJECT,
        _: &'a DeviceExtension,
        irp: &'a mut IRP,
        _: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        Ok(Self { _irp: irp })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        self._irp.IoStatus.Information = 0;
        Ok(())
    }
}
