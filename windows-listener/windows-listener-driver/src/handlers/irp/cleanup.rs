use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, IRP_MJ_CLEANUP};

use crate::error::RuntimeError;
use crate::handlers::irp::IrpHandler;

pub struct CleanupHandler<'a> {
    _irp: &'a mut IRP,
}

impl<'a> IrpHandler<'a> for CleanupHandler<'a> {
    const CODE: u32 = IRP_MJ_CLEANUP;

    fn new(
        _: &'a DEVICE_OBJECT,
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
