use alloc::boxed::Box;
use alloc::collections::btree_map::BTreeMap;
use core::mem::size_of;
use core::ptr;
use core::sync::atomic::{AtomicPtr, Ordering};

use wdk::nt_success;
use wdk_sys::ntddk::{IoCreateDevice, IoDeleteDevice};
use wdk_sys::{
    DEVICE_OBJECT, DO_BUFFERED_IO, DO_DEVICE_INITIALIZING, DRIVER_OBJECT, FILE_DEVICE_SECURE_OPEN,
    FILE_DEVICE_UNKNOWN, IRP,
};
use windows::Wdk::Storage::FileSystem::Minifilters::{FltRegisterFilter, PFLT_FILTER};

use crate::config::{DEVICE_NAME, DOS_NAME, DRIVER};
use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::handlers::minifilter::FILTER_REGISTRATION;
use crate::handlers::process_notify::process_notify;
use crate::log;
use crate::state::DeviceExtension;
use crate::wrappers::lock::SpinLock;
use crate::wrappers::safety::{
    add_create_process_notify, create_symbolic_link, delete_symbolic_link,
    remove_create_process_notify,
};
use crate::wrappers::strings::UnicodeString;

struct _CleanupGuard<'a> {
    pub driver: &'a mut DRIVER_OBJECT,
}

impl<'a> Drop for _CleanupGuard<'a> {
    fn drop(&mut self) {
        let _ = driver_unload(self.driver);
    }
}

pub fn driver_entry(
    driver: &mut DRIVER_OBJECT,
    registry_path: UnicodeString,
    driver_unload: unsafe extern "C" fn(*mut DRIVER_OBJECT),
    irp_handler: unsafe extern "C" fn(*mut DEVICE_OBJECT, *mut IRP) -> i32,
) -> Result<(), RuntimeError> {
    let guard = _CleanupGuard { driver };
    guard.driver.DriverUnload = Some(driver_unload);
    guard.driver.DriverExtension;
    for handler in guard.driver.MajorFunction.iter_mut() {
        *handler = Some(irp_handler);
    }

    log!(
        "driver_entry {:?}, registry_path={registry_path:?}",
        ForeignDisplayer::Unicode(&guard.driver.DriverName),
    );

    let device_name = UnicodeString::try_from(DEVICE_NAME)?;

    let mut device = ptr::null_mut();
    let status = unsafe {
        let mut device_name = device_name.native().into_inner();
        IoCreateDevice(
            guard.driver,
            size_of::<DeviceExtension>().try_into()?,
            &mut device_name,
            FILE_DEVICE_UNKNOWN,
            FILE_DEVICE_SECURE_OPEN,
            0,
            &mut device,
        )
    };
    if !nt_success(status) {
        log!("Failed to create device: {status}");
        return Err(RuntimeError::Failure(status));
    }

    let mut filter = PFLT_FILTER::default();
    let status = unsafe {
        FltRegisterFilter(
            &*guard.driver as *const DRIVER_OBJECT as *const _,
            &FILTER_REGISTRATION,
            &mut filter,
        )
    }
    .0;
    if !nt_success(status) {
        log!("Failed to register minifilter: {status}");
        return Err(RuntimeError::Failure(status));
    }

    if let Some(device) = unsafe { device.as_mut() } {
        device.Flags |= DO_BUFFERED_IO;
        device.Flags &= !DO_DEVICE_INITIALIZING;

        unsafe {
            ptr::write_volatile(
                device.DeviceExtension as *mut DeviceExtension,
                DeviceExtension {
                    shared_memory: AtomicPtr::new(ptr::null_mut()),
                    minifilter: AtomicPtr::new(Box::into_raw(Box::new(filter))),
                    thresholds: SpinLock::new(BTreeMap::new()),
                },
            );
        }
    }

    create_symbolic_link(&DOS_NAME.try_into()?, &DEVICE_NAME.try_into()?).inspect_err(|e| {
        log!("Failed to create symbolic link: {e}");
    })?;

    DRIVER.store(guard.driver, Ordering::SeqCst);

    add_create_process_notify(process_notify).inspect_err(|e| {
        log!("Failed to add process notify: {e}");
    })?;

    // add_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to add thread notify: {e}");
    // })?;

    drop(guard);
    Ok(())
}

pub fn driver_unload(driver: &mut DRIVER_OBJECT) -> Result<(), RuntimeError> {
    log!(
        "driver_unload {:?}",
        ForeignDisplayer::Unicode(&driver.DriverName),
    );

    // remove_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to remove thread notify: {e}");
    // })?;

    remove_create_process_notify(process_notify).inspect_err(|e| {
        log!("Failed to remove process notify: {e}");
    })?;

    match DOS_NAME.try_into() {
        Ok(dos_name) => {
            if let Err(e) = delete_symbolic_link(&dos_name) {
                log!("Failed to remove symlink: {e}");
            }
        }
        Err(e) => {
            log!("Cannot convert {DOS_NAME:?} to UnicodeString: {e}");
        }
    }

    let device = driver.DeviceObject;
    if let Some(device) = unsafe { device.as_mut() } {
        unsafe {
            ptr::drop_in_place(device.DeviceExtension as *mut DeviceExtension);
            IoDeleteDevice(device);
        }
    }

    DRIVER.store(ptr::null_mut(), Ordering::SeqCst);

    Ok(())
}
