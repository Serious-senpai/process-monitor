mod memory_cleanup;
mod memory_initialize;

use memory_cleanup::MemoryCleanupHandler;
use memory_initialize::MemoryInitializeHandler;
use wdk_sys::{
    DEVICE_OBJECT, IO_STACK_LOCATION, IRP, IRP_MJ_DEVICE_CONTROL, STATUS_INVALID_DEVICE_REQUEST,
};

use crate::error::RuntimeError;
use crate::handlers::irp::IrpHandler;
use crate::state::DeviceExtension;

/// Trait for handling IOCTL requests.
///
/// Each time an IOCTL request is received, an instance of a type implementing this trait
/// will be created to handle the request.
pub trait IoctlHandler<'a> {
    /// The IOCTL code this handler is responsible for.
    const CODE: u32;

    /// Create a new instance of the handler.
    ///
    /// Implementations should use this method to populate necessary fields of their own
    /// (note that [`IoctlHandler::handle()`] does not take any arguments).
    fn new(
        device: &'a DEVICE_OBJECT,
        extension: &'a DeviceExtension,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError>
    where
        Self: Sized;

    /// Handle the IOCTL request. In case of failure, [`IoctlHandler::on_failure()`] will be called.
    fn handle(&mut self) -> Result<(), RuntimeError>;

    /// Clean up in case [`IoctlHandler::handle()`] fails.
    ///
    /// The default implementation does nothing.
    fn on_failure(&mut self) {}
}

macro_rules! _ioctl_handle {
    ($device:expr, $extension:expr, $irp:expr, $irpsp:expr, $($Handler:tt,)*) => {
        match unsafe { $irpsp.Parameters.DeviceIoControl.IoControlCode } {
            $(
                $Handler::CODE => {
                    let mut handler = $Handler::new(
                        $device,
                        $extension,
                        $irp,
                        $irpsp,
                    )?;
                    let result = handler.handle();
                    if result.is_err() {
                        handler.on_failure();
                    }

                    result
                },
            )*
            _ => Err(RuntimeError::Failure(STATUS_INVALID_DEVICE_REQUEST)),
        }
    };
}

pub struct DeviceControlHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _extension: &'a DeviceExtension,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
}

impl<'a> IrpHandler<'a> for DeviceControlHandler<'a> {
    const CODE: u32 = IRP_MJ_DEVICE_CONTROL;

    fn new(
        device: &'a DEVICE_OBJECT,
        extension: &'a DeviceExtension,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        Ok(Self {
            _device: device,
            _extension: extension,
            _irp: irp,
            _irpsp: irpsp,
        })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        _ioctl_handle!(
            self._device,
            self._extension,
            self._irp,
            self._irpsp,
            MemoryCleanupHandler,
            MemoryInitializeHandler,
        )
    }
}
