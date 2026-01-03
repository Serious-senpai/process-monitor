use alloc::boxed::Box;
use core::ptr;
use core::sync::atomic::Ordering;

use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, IRP_MJ_CLOSE};

use crate::error::RuntimeError;
use crate::handlers::irp::IrpHandler;
use crate::state::DRIVER_STATE;

pub struct CloseHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
}

impl<'a> IrpHandler<'a> for CloseHandler<'a> {
    const CODE: u32 = IRP_MJ_CLOSE;

    fn new(
        device: &'a DEVICE_OBJECT,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        Ok(Self {
            _device: device,
            _irp: irp,
            _irpsp: irpsp,
        })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        self._irp.IoStatus.Information = 0;

        let old = DRIVER_STATE
            .shared_memory
            .swap(ptr::null_mut(), Ordering::AcqRel);
        if !old.is_null() {
            unsafe {
                drop(Box::from_raw(old));
            }
        }

        Ok(())
    }
}
