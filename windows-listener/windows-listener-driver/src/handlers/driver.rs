use alloc::boxed::Box;
use alloc::collections::btree_map::BTreeMap;
use core::num::NonZero;
use core::ptr;
use core::sync::atomic::Ordering;

use ffi::MAX_PROCESS_COUNT;
use lru::LruCache;
use wdk::nt_success;
use wdk_sys::ntddk::{IoCreateDevice, IoDeleteDevice, KeQueryPerformanceCounter};
use wdk_sys::{
    DEVICE_OBJECT, DO_BUFFERED_IO, DO_DEVICE_INITIALIZING, DRIVER_OBJECT, FILE_DEVICE_SECURE_OPEN,
    FILE_DEVICE_UNKNOWN, IRP, LARGE_INTEGER,
};
use windows::Wdk::Storage::FileSystem::Minifilters::{
    FltRegisterFilter, FltStartFiltering, FltUnregisterFilter, PFLT_FILTER,
};

use crate::config::{DEVICE_NAME, DOS_NAME};
use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::handlers::minifilter::FILTER_REGISTRATION;
use crate::handlers::process_notify::process_notify;
use crate::log;
use crate::state::DRIVER_STATE;
use crate::wrappers::lock::SpinLock;
use crate::wrappers::safety::{
    add_create_process_notify, create_symbolic_link, delete_symbolic_link,
    remove_create_process_notify,
};
use crate::wrappers::strings::UnicodeString;

struct _CleanupGuard<'a> {
    pub driver: &'a mut DRIVER_OBJECT,
    pub cleanup: bool,
}

impl<'a> Drop for _CleanupGuard<'a> {
    fn drop(&mut self) {
        if self.cleanup {
            let _ = driver_unload(self.driver);
        }
    }
}

pub fn driver_entry(
    driver: &mut DRIVER_OBJECT,
    registry_path: UnicodeString,
    driver_unload: unsafe extern "C" fn(*mut DRIVER_OBJECT),
    irp_handler: unsafe extern "C" fn(*mut DEVICE_OBJECT, *mut IRP) -> i32,
) -> Result<(), RuntimeError> {
    let mut guard = _CleanupGuard {
        driver,
        cleanup: true,
    };
    guard.driver.DriverUnload = Some(driver_unload);
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
            0,
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

    let mut perf_freq = LARGE_INTEGER::default();
    unsafe {
        KeQueryPerformanceCounter(&mut perf_freq);
    }

    DRIVER_STATE
        .ticks_per_ms
        .store(unsafe { perf_freq.QuadPart } / 1000, Ordering::Release);

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

    DRIVER_STATE.minifilter.store(filter.0, Ordering::Release);

    if let Some(device) = unsafe { device.as_mut() } {
        device.Flags |= DO_BUFFERED_IO;
        device.Flags &= !DO_DEVICE_INITIALIZING;
    }

    create_symbolic_link(&DOS_NAME.try_into()?, &DEVICE_NAME.try_into()?).inspect_err(|e| {
        log!("Failed to create symbolic link: {e}");
    })?;

    DRIVER_STATE.driver.store(guard.driver, Ordering::Release);
    DRIVER_STATE.thresholds.store(
        Box::into_raw(Box::new(SpinLock::new(BTreeMap::new()))),
        Ordering::Release,
    );
    DRIVER_STATE.disk_io.store(
        Box::into_raw(Box::new(SpinLock::new(LruCache::new(unsafe {
            NonZero::new_unchecked(MAX_PROCESS_COUNT as _)
        })))),
        Ordering::Release,
    );

    add_create_process_notify(process_notify).inspect_err(|e| {
        log!("Failed to add process notify: {e}");
    })?;

    // add_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to add thread notify: {e}");
    // })?;

    let status = unsafe { FltStartFiltering(filter) }.0;
    if !nt_success(status) {
        log!("Failed to start minifilter: {status}");
        return Err(RuntimeError::Failure(status));
    }

    guard.cleanup = false;
    Ok(())
}

pub fn driver_unload(driver: &mut DRIVER_OBJECT) {
    log!(
        "driver_unload {:?}",
        ForeignDisplayer::Unicode(&driver.DriverName),
    );

    // let _ = remove_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to remove thread notify: {e}");
    // });

    let _ = remove_create_process_notify(process_notify).inspect_err(|e| {
        log!("Failed to remove process notify: {e}");
    });

    let thresholds = DRIVER_STATE.thresholds.load(Ordering::Acquire);
    if !thresholds.is_null() {
        drop(unsafe { Box::from_raw(thresholds) });
    }

    DRIVER_STATE
        .driver
        .store(ptr::null_mut(), Ordering::Release);

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

    let filter = DRIVER_STATE.minifilter.swap(0, Ordering::AcqRel);

    if filter != 0 {
        unsafe {
            FltUnregisterFilter(PFLT_FILTER(filter));
        }
    }

    DRIVER_STATE.ticks_per_ms.store(0, Ordering::Release);

    let device = driver.DeviceObject;
    if !device.is_null() {
        unsafe {
            IoDeleteDevice(device);
        }
    }
}
