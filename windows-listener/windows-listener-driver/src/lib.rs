#![no_std]

extern crate alloc;

mod config;
mod displayer;
mod error;
mod handlers;
mod log;
mod mpsc;
mod state;
mod wrappers;

extern crate wdk_panic;

use wdk_alloc::WdkAllocator;
use wdk_sys::ntddk::IofCompleteRequest;
use wdk_sys::{
    IO_NO_INCREMENT, NTSTATUS, PCUNICODE_STRING, PDEVICE_OBJECT, PDRIVER_OBJECT, PIRP,
    STATUS_INVALID_PARAMETER, STATUS_SUCCESS, STATUS_UNSUCCESSFUL,
};

use crate::error::RuntimeError;
use crate::state::DeviceExtension;
use crate::wrappers::bindings::IoGetCurrentIrpStackLocation;
use crate::wrappers::strings::UnicodeString;

#[global_allocator]
static GLOBAL_ALLOCATOR: WdkAllocator = WdkAllocator;

/// # Safety
/// Must be called by the OS.
unsafe extern "C" fn driver_unload(driver: PDRIVER_OBJECT) {
    let driver = match unsafe { driver.as_mut() } {
        Some(d) => d,
        None => {
            log!("driver_unload: PDRIVER_OBJECT is null");
            return;
        }
    };

    if let Err(e) = handlers::driver_unload(driver) {
        log!("Error when unloading driver: {e}");
    }
}

/// # Safety
/// Must be called by the OS. Because IRP handlers may be run concurrently, we provide
/// shared state (e.g. [`DeviceExtension`]) as immutable references.
unsafe extern "C" fn irp_handler(device: PDEVICE_OBJECT, irp: PIRP) -> NTSTATUS {
    let device = match unsafe { device.as_ref() } {
        Some(d) => d,
        None => {
            log!("irp_handler: PDEVICE_OBJECT is null");
            return STATUS_INVALID_PARAMETER;
        }
    };

    let extension = match unsafe {
        let ptr = device.DeviceExtension as *mut DeviceExtension;
        ptr.as_ref()
    } {
        Some(ext) => ext,
        None => {
            log!("irp_handler: DeviceExtension is null");
            return STATUS_INVALID_PARAMETER;
        }
    };

    let irp = match unsafe { irp.as_mut() } {
        Some(i) => i,
        None => {
            log!("irp_handler: PIRP is null");
            return STATUS_INVALID_PARAMETER;
        }
    };

    let irpsp = match unsafe { IoGetCurrentIrpStackLocation(irp).as_mut() } {
        Some(s) => s,
        None => {
            log!("irp_handler: Failed to call IoGetCurrentIrpStackLocation");
            return STATUS_INVALID_PARAMETER;
        }
    };

    log!("Received IRP {}", irpsp.MajorFunction);

    let status = match handlers::irp_handler(device, extension, irp, irpsp) {
        Ok(()) => STATUS_SUCCESS,
        Err(e) => {
            log!("Error when handling IRP: {e}");
            match e {
                RuntimeError::Failure(status) => status,
                _ => STATUS_UNSUCCESSFUL,
            }
        }
    };

    irp.IoStatus.__bindgen_anon_1.Status = status;
    unsafe {
        IofCompleteRequest(irp, IO_NO_INCREMENT as _);
    }

    status
}

/// # Safety
/// Must be called by the OS.
#[unsafe(export_name = "DriverEntry")]
pub unsafe extern "C" fn driver_entry(
    driver: PDRIVER_OBJECT,
    registry_path: PCUNICODE_STRING,
) -> NTSTATUS {
    let driver = match unsafe { driver.as_mut() } {
        Some(d) => d,
        None => {
            log!("driver_entry: PDRIVER_OBJECT is null");
            return STATUS_INVALID_PARAMETER;
        }
    };

    let registry_path = match unsafe { UnicodeString::from_raw(registry_path) } {
        Ok(r) => r,
        Err(e) => {
            log!("driver_entry: failed to parse registry path: {e}");
            return STATUS_INVALID_PARAMETER;
        }
    };

    match handlers::driver_entry(driver, registry_path, driver_unload, irp_handler) {
        Ok(()) => STATUS_SUCCESS,
        Err(e) => {
            log!("Error when loading driver: {e}");
            match e {
                RuntimeError::Failure(status) => status,
                _ => STATUS_UNSUCCESSFUL,
            }
        }
    }
}
