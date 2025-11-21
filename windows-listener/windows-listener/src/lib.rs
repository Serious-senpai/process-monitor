use std::ffi::{c_char, c_int, c_void};
use std::fs::OpenOptions;
use std::os::windows::io::IntoRawHandle;
use std::{io, ptr, slice};

use ffi::win32::message::{IOCTL_CLEAR_MONITOR, IOCTL_MEMORY_INITIALIZE, MemoryInitialize};
use ffi::win32::mpsc::DefaultChannel;
use ffi::{Event, Threshold};
use log::{LevelFilter, error};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};
use windows::Win32::Foundation::{HANDLE, INVALID_HANDLE_VALUE, WAIT_OBJECT_0};
use windows::Win32::System::IO::DeviceIoControl;
use windows::Win32::System::Memory::{
    CreateFileMappingW, FILE_MAP_READ, FILE_MAP_WRITE, MEMORY_MAPPED_VIEW_ADDRESS, MapViewOfFile,
    PAGE_READWRITE, UnmapViewOfFile,
};
use windows::Win32::System::Threading::{CreateEventW, WaitForSingleObject};
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

struct _KernelTracer {
    pub hmap: HANDLE,
    pub base: _MappedMemoryGuard,
    pub device: HANDLE,
    pub event: HANDLE,
}

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

const DEVICE_NAME: &str = r"\\.\WinLisDev";

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
    let channel_size = size_of::<DefaultChannel>();
    let channel_size_u32 = match u32::try_from(channel_size) {
        Ok(size) => size,
        Err(e) => {
            error!("DefaultChannel size is too large: {e}");
            return ptr::null_mut();
        }
    };

    let hmap = match unsafe {
        CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            None,
            PAGE_READWRITE,
            0,
            channel_size_u32,
            PCWSTR::from_raw(ptr::null()),
        )
    } {
        Ok(hmap) => hmap,
        Err(e) => {
            error!("CreateFileMappingW failed: {e}");
            return ptr::null_mut();
        }
    };

    // `HANDLE` already has a `Drop` impl that calls `CloseHandle`. I do not really like this implicit behavior though.
    let base = unsafe { MapViewOfFile(hmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, channel_size) };

    if base.Value.is_null() {
        error!("MapViewOfFile failed: {}", io::Error::last_os_error());
        return ptr::null_mut();
    }

    let base = _MappedMemoryGuard(base);
    let event = match unsafe { CreateEventW(None, false, false, PCWSTR::from_raw(ptr::null())) } {
        Ok(event) => event,
        Err(e) => {
            error!("CreateEventW failed: {e}");
            return ptr::null_mut();
        }
    };

    match OpenOptions::new().read(true).write(true).open(DEVICE_NAME) {
        Ok(file) => {
            let device = HANDLE(file.into_raw_handle());
            let message = MemoryInitialize {
                section: hmap.0,
                event: event.0,
            };

            if let Err(e) = unsafe {
                DeviceIoControl(
                    device,
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

            let tracer = Box::new(_KernelTracer {
                hmap,
                base,
                device,
                event,
            });
            Box::into_raw(tracer) as *mut KernelTracerHandle
        }
        Err(e) => {
            error!("Failed to open device {DEVICE_NAME:?}: {e}");
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
        drop(unsafe { Box::from_raw(tracer) });
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
    let tracer = tracer as *mut _KernelTracer;
    match unsafe { tracer.as_ref() } {
        Some(tracer) => unsafe {
            DeviceIoControl(
                tracer.device,
                IOCTL_CLEAR_MONITOR,
                None,
                0,
                None,
                0,
                None,
                None,
            )
        }
        .is_err() as c_int,
        None => 1,
    }
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn next_event(
    tracer: *const KernelTracerHandle,
    timeout_ms: u32,
) -> *mut Event {
    let tracer = tracer as *mut _KernelTracer;
    match unsafe { tracer.as_ref() } {
        Some(tracer) => {
            if unsafe { WaitForSingleObject(tracer.event, timeout_ms) } != WAIT_OBJECT_0 {
                return ptr::null_mut();
            }

            let channel = tracer.base.0.Value as *const DefaultChannel;
            let channel = unsafe { &*channel };

            let mut event = Box::<Event>::new_uninit();
            let mut buffer = unsafe {
                slice::from_raw_parts_mut(event.as_mut_ptr() as *mut u8, size_of::<Event>())
            };
            channel.read(&mut buffer);

            let event = unsafe { event.assume_init() };
            Box::into_raw(event)
        }
        None => ptr::null_mut(),
    }
}

/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`next_event`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drop_event(event: *mut Event) {
    if !event.is_null() {
        drop(unsafe { Box::from_raw(event) });
    }
}
