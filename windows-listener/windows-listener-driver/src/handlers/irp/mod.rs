mod cleanup;
mod close;
mod create;
mod ioctl;

use cleanup::CleanupHandler;
use close::CloseHandler;
use create::CreateHandler;
use ioctl::DeviceControlHandler;
use wdk_sys::{DEVICE_OBJECT, IO_STACK_LOCATION, IRP, STATUS_INVALID_DEVICE_REQUEST};

use crate::error::RuntimeError;
use crate::state::DeviceExtension;

/// Trait for handling IRP requests.
///
/// Each time an IRP request is received, an instance of a type implementing this trait
/// will be created to handle the request.
pub trait IrpHandler<'a> {
    /// The IRP major function code this handler is responsible for.
    const CODE: u32;

    /// Create a new instance of the handler.
    ///
    /// Implementations should use this method to populate necessary fields of their own
    /// (note that [`IrpHandler::handle()`] does not take any arguments).
    fn new(
        device: &'a DEVICE_OBJECT,
        extension: &'a DeviceExtension,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError>
    where
        Self: Sized;

    /// Handle the IRP request.
    ///
    /// Implementations should set the appropriate fields in the [`IRP::IoStatus`] structure, except
    /// `Status`, as that field will be set automatically by the framework afterwards.
    fn handle(&mut self) -> Result<(), RuntimeError>;
}

macro_rules! _irp_handle {
    ($device:expr, $extension:expr, $irp:expr, $irpsp:expr, $($Handler:tt,)*) => {
        match $irpsp.MajorFunction.into() {
            $(
                $Handler::CODE => {
                    let mut handler = $Handler::new(
                        $device,
                        $extension,
                        $irp,
                        $irpsp,
                    )?;
                    handler.handle()
                },
            )*
            _ => Err(RuntimeError::Failure(STATUS_INVALID_DEVICE_REQUEST)),
        }
    };
}

pub fn irp_handler(
    device: &DEVICE_OBJECT,
    extension: &DeviceExtension,
    irp: &mut IRP,
    irpsp: &mut IO_STACK_LOCATION,
) -> Result<(), RuntimeError> {
    _irp_handle!(
        device,
        extension,
        irp,
        irpsp,
        CleanupHandler,
        CloseHandler,
        CreateHandler,
        DeviceControlHandler,
    )
}
