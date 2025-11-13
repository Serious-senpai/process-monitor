use alloc::collections::VecDeque;
use core::mem::size_of;
use core::ptr::{null_mut, write};
use core::sync::atomic::Ordering;

use wdk_sys::ntddk::IoCreateDevice;
use wdk_sys::{
    DEVICE_OBJECT, DO_BUFFERED_IO, DO_DEVICE_INITIALIZING, DRIVER_OBJECT, FILE_DEVICE_SECURE_OPEN,
    FILE_DEVICE_UNKNOWN, IRP, NT_SUCCESS,
};

use crate::config::{DEVICE_NAME, DOS_NAME, DRIVER, QUEUE_CAPACITY};
use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::handlers::delete_device;
use crate::log;
use crate::state::{DeviceExtension, DeviceState};
use crate::wrappers::mutex::SpinLock;
use crate::wrappers::safety::create_symbolic_link;
use crate::wrappers::strings::UnicodeString;

pub fn driver_entry(
    driver: &mut DRIVER_OBJECT,
    registry_path: UnicodeString,
    driver_unload: unsafe extern "C" fn(*mut DRIVER_OBJECT),
    irp_handler: unsafe extern "C" fn(*mut DEVICE_OBJECT, *mut IRP) -> i32,
) -> Result<(), RuntimeError> {
    driver.DriverUnload = Some(driver_unload);
    for handler in driver.MajorFunction.iter_mut() {
        *handler = Some(irp_handler);
    }

    log!(
        "driver_entry {:?}, registry_path={registry_path:?}",
        ForeignDisplayer::Unicode(&driver.DriverName),
    );

    let device_name = UnicodeString::try_from(DEVICE_NAME)?;

    let mut device = null_mut();
    let status = unsafe {
        let mut device_name = device_name.native().into_inner();
        IoCreateDevice(
            driver,
            size_of::<DeviceExtension>().try_into()?,
            &mut device_name,
            FILE_DEVICE_UNKNOWN,
            FILE_DEVICE_SECURE_OPEN,
            0,
            &mut device,
        )
    };
    if !NT_SUCCESS(status) {
        log!("Failed to create device: {status}");
        return Err(RuntimeError::Failure(status));
    }

    if let Some(device) = unsafe { device.as_mut() } {
        device.Flags |= DO_BUFFERED_IO;
        device.Flags &= !DO_DEVICE_INITIALIZING;

        unsafe {
            write(
                device.DeviceExtension as *mut DeviceExtension,
                DeviceExtension {
                    inner: SpinLock::new(DeviceState {
                        queue: VecDeque::with_capacity(QUEUE_CAPACITY),
                        memmap: None,
                    }),
                },
            );
        }
    }

    create_symbolic_link(&DOS_NAME.try_into()?, &DEVICE_NAME.try_into()?).inspect_err(|e| {
        log!("Failed to create symbolic link: {e}");
        delete_device(driver);
    })?;

    DRIVER.store(driver, Ordering::SeqCst);

    // add_create_process_notify(process_notify).inspect_err(|e| {
    //     log!("Failed to add process notify: {e}");
    // })?;

    // add_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to add thread notify: {e}");
    // })?;

    Ok(())
}
