use core::ffi::CStr;
use core::ptr::null_mut;
use core::sync::atomic::AtomicPtr;

use wdk_sys::DRIVER_OBJECT;

pub const DOS_NAME: &CStr = c"\\DosDevices\\WinLisDev";
pub const DEVICE_NAME: &CStr = c"\\Device\\WinLisDev";
pub static DRIVER: AtomicPtr<DRIVER_OBJECT> = AtomicPtr::new(null_mut());
