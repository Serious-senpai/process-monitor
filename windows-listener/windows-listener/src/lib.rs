use std::ffi::{c_char, c_int, c_void};
use std::fs::OpenOptions;
use std::os::windows::io::IntoRawHandle;
use std::{io, ptr};

use ffi::win32::message::{IOCTL_MEMORY_CLEANUP, IOCTL_MEMORY_INITIALIZE, MemoryInitialize};
use ffi::win32::mpsc::DefaultChannel;
use ffi::{Event, Threshold};
use log::{LevelFilter, error};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};
use windows::Win32::Foundation::{HANDLE, INVALID_HANDLE_VALUE};
use windows::Win32::System::IO::DeviceIoControl;
use windows::Win32::System::Memory::{
    CreateFileMappingW, FILE_MAP_READ, FILE_MAP_WRITE, MEMORY_MAPPED_VIEW_ADDRESS, MapViewOfFile,
    PAGE_READWRITE, UnmapViewOfFile,
};
use windows::core::PCWSTR;

#[derive(Debug)]
struct _MappedMemoryGuard(MEMORY_MAPPED_VIEW_ADDRESS);

impl Drop for _MappedMemoryGuard {
    fn drop(&mut self) {
        if !self.0.Value.is_null() {
            unsafe {
                let _ = UnmapViewOfFile(self.0);
            }
        }
    }
}

struct _DeviceGuard(HANDLE);

impl Drop for _DeviceGuard {
    fn drop(&mut self) {
        unsafe {
            let _ = DeviceIoControl(self.0, IOCTL_MEMORY_CLEANUP, None, 0, None, 0, None, None);
        }
    }
}

struct _KernelTracer {
    pub hmap: HANDLE,
    pub base: _MappedMemoryGuard,
    pub device: _DeviceGuard,
}

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

const DEVICE_NAME: &str = r"\\.\LogDrvDev";

/// # Safety
/// This function is just marked as `unsafe` because it is exposed via `extern "C"`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn initialize_logger(level: c_int) -> c_int {
    let filter = match level {
        0 => LevelFilter::Off,
        1 => LevelFilter::Error,
        2 => LevelFilter::Warn,
        3 => LevelFilter::Info,
        4 => LevelFilter::Debug,
        5 => LevelFilter::Trace,
        _ => return 1,
    };
    let cfg = ConfigBuilder::new()
        .set_location_level(filter)
        .set_time_offset_to_local()
        .unwrap_or_else(|e| e)
        .build();

    // Windows will reclaim the leaked memory when our library is unloaded.
    if let Err(e) = TermLogger::init(filter, cfg, TerminalMode::Stderr, ColorChoice::Auto) {
        eprintln!("Failed to initialize logger: {e}");
        return 1;
    }

    0
}

/// # Safety
/// This function is just marked as `unsafe` because it is exposed via `extern "C"`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn new_tracer() -> *mut KernelTracerHandle {
    let view_size = size_of::<MemoryInitialize>();
    match unsafe {
        CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            None,
            PAGE_READWRITE,
            ((view_size >> 32) & 0xFFFFFFFF) as u32,
            (view_size & 0xFFFFFFFF) as u32,
            PCWSTR::from_raw(ptr::null()),
        )
    } {
        Ok(hmap) => {
            // `HANDLE` already has a `Drop` impl that calls `CloseHandle`. I do not really like this implicit behavior though.
            let base =
                unsafe { MapViewOfFile(hmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, view_size) };

            if base.Value.is_null() {
                error!("MapViewOfFile failed: {}", io::Error::last_os_error());
                return ptr::null_mut();
            }

            let base = _MappedMemoryGuard(base);

            match OpenOptions::new()
                .read(false)
                .write(false)
                .open(DEVICE_NAME)
            {
                Ok(file) => {
                    let device = _DeviceGuard(HANDLE(file.into_raw_handle()));
                    let message = MemoryInitialize {
                        section: hmap.0,
                        event: device.0.0,
                    };

                    if let Err(e) = unsafe {
                        DeviceIoControl(
                            device.0,
                            IOCTL_MEMORY_INITIALIZE,
                            Some(&message as *const _ as *const c_void),
                            size_of::<MemoryInitialize>() as _,
                            None,
                            0,
                            None,
                            None,
                        )
                    } {
                        error!("DeviceIoControl failed: {e}");
                        return ptr::null_mut();
                    }

                    let tracer = Box::new(_KernelTracer { hmap, base, device });
                    Box::into_raw(tracer) as *mut KernelTracerHandle
                }
                Err(e) => {
                    error!("Failed to open device {DEVICE_NAME:?}: {e}");
                    ptr::null_mut()
                }
            }
        }
        Err(e) => {
            error!("CreateFileMappingW failed: {e}");
            ptr::null_mut()
        }
    }
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn free_tracer(tracer: *mut KernelTracerHandle) {
    let tracer = tracer as *mut _KernelTracer;
    if !tracer.is_null() {
        let _ = unsafe { Box::from_raw(tracer) };
    }
}

/// # Safety
/// All of the following conditions must be true:
/// - `tracer` must be null or a valid pointer obtained from [`new_tracer`].
/// - `name` must be null or a valid null-terminated string.
/// - `threshold` must be null or a valid pointer to a [`Threshold`] structure.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn set_monitor(
    tracer: *const KernelTracerHandle,
    name: *const c_char,
    threshold: *const Threshold,
) -> c_int {
    1
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn clear_monitor(tracer: *const KernelTracerHandle) -> c_int {
    1
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn next_event(
    tracer: *const KernelTracerHandle,
    timeout_ms: c_int,
) -> *mut Event {
    ptr::null_mut()
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`next_event`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drop_event(event: *mut Event) {}
