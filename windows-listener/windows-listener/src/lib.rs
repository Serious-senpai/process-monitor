use std::collections::VecDeque;
use std::ffi::{CStr, c_char, c_int, c_void};
use std::fs::OpenOptions;
use std::os::windows::io::IntoRawHandle;
use std::sync::Mutex;
use std::{io, ptr};

use ffi::win32::event::WindowsEvent;
use ffi::win32::message::{
    IOCTL_CLEAR_MONITOR, IOCTL_MEMORY_INITIALIZE, IOCTL_SET_MONITOR, MemoryInitialize, SetMonitor,
};
use ffi::win32::mpsc::{DEFAULT_CHANNEL_SIZE, DefaultChannel};
use ffi::{Event, StaticCommandName, Threshold};
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
    pub reading: Mutex<VecDeque<u8>>,
}

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

const DEVICE_NAME: &str = r"\\.\WinLisDev";

/// Initialize the logger with the specified minimum verbose log level.
/// Valid values for `level` are: 0 (Off), 1 (Error), 2 (Warn), 3 (Info), 4 (Debug), 5 (Trace).
///
/// # Returns
/// - 0 on success
/// - 1 on failure
///
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

/// Create a new kernel tracer instance.
///
/// # Returns
/// A pointer to the created `KernelTracerHandle` instance, or null on failure.
///
/// # Safety
/// This function is just marked as `unsafe` because it is exposed via `extern "C"`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn new_tracer() -> *mut KernelTracerHandle {
    const CHANNEL_SIZE: usize = size_of::<DefaultChannel>();
    let channel_size_u32 = match u32::try_from(CHANNEL_SIZE) {
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
    let base = unsafe { MapViewOfFile(hmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, CHANNEL_SIZE) };

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
                mapping: hmap.0,
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
                reading: Mutex::new(VecDeque::new()),
            });
            Box::into_raw(tracer) as *mut KernelTracerHandle
        }
        Err(e) => {
            error!("Failed to open device {DEVICE_NAME:?}: {e}");
            ptr::null_mut()
        }
    }
}

/// Free the kernel tracer instance.
///
/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn free_tracer(tracer: *mut KernelTracerHandle) {
    let tracer = tracer as *mut _KernelTracer;
    if !tracer.is_null() {
        drop(unsafe { Box::from_raw(tracer) });
    }
}

/// Add a monitor target to the provided kernel tracer (for example: "curl.exe")
///
/// # Returns
/// - 0 on success
/// - 1 on failure
///
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
    let tracer = tracer as *const _KernelTracer;
    if name.is_null() {
        return 1;
    }

    if let Some(tracer) = unsafe { tracer.as_ref() }
        && let Ok(name) = unsafe { CStr::from_ptr(name) }.to_str()
        && let Some(threshold) = unsafe { threshold.as_ref() }
    {
        let message = SetMonitor {
            name: StaticCommandName::from(name),
            threshold: *threshold,
        };

        if let Err(e) = unsafe {
            DeviceIoControl(
                tracer.device,
                IOCTL_SET_MONITOR,
                Some(&message as *const _ as *const c_void),
                size_of::<SetMonitor>() as _,
                None,
                0,
                None,
                None,
            )
        } {
            error!("Failed to set monitor for {name:?}: {e}");
            1
        } else {
            0
        }
    } else {
        1
    }
}

/// Clear all monitor targets from the provided kernel tracer.
///
/// # Returns
/// - 0 on success
/// - 1 on failure
///
/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn clear_monitor(tracer: *const KernelTracerHandle) -> c_int {
    let tracer = tracer as *const _KernelTracer;
    match unsafe { tracer.as_ref() } {
        Some(tracer) => {
            if let Err(e) = unsafe {
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
            } {
                error!("Failed to clear monitor: {e}");
                1
            } else {
                0
            }
        }

        None => 1,
    }
}

/// Return the next event from the kernel tracer.
/// In Windows, the following types of event may be returned:
/// - Process creation event
/// - Violation of disk I/O usage threshold
/// - Violation of network I/O usage threshold
///
/// # Returns
/// A pointer to the obtained `Event` instance, or null on timeout or failure. Note
/// that the returned pointer must be freed via [`drop_event`].
///
/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`new_tracer`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn next_event(
    tracer: *const KernelTracerHandle,
    timeout_ms: u32,
) -> *mut Event {
    let tracer = tracer as *const _KernelTracer;
    match unsafe { tracer.as_ref() } {
        Some(tracer) => {
            let mut buffer = [0u8; DEFAULT_CHANNEL_SIZE / 4];
            loop {
                match tracer.reading.lock() {
                    Ok(mut reading) => {
                        if unsafe { WaitForSingleObject(tracer.event, timeout_ms) } != WAIT_OBJECT_0
                        {
                            return ptr::null_mut();
                        }

                        let channel = tracer.base.0.Value as *const DefaultChannel;
                        let channel = unsafe { &*channel };

                        let size = unsafe {
                            // Safety: The `tracer.reading` mutex above ensures that there is only one reader at a time.
                            channel.read(&mut buffer)
                        };

                        let base_index = reading.len();
                        let mut complete_index = usize::MAX;
                        for (index, &byte) in buffer[..size].iter().enumerate() {
                            reading.push_back(byte);
                            if byte == 0 {
                                complete_index = base_index + index;
                            }
                        }

                        if complete_index != usize::MAX {
                            let mut data = reading.drain(..=complete_index).collect::<Vec<u8>>();
                            match postcard::from_bytes_cobs::<WindowsEvent>(&mut data) {
                                Ok(event) => {
                                    let boxed = Box::new(event.into());
                                    return Box::into_raw(boxed);
                                }
                                Err(e) => {
                                    error!("Failed to deserialize event: {e}");
                                }
                            }
                        }
                    }
                    Err(e) => {
                        error!("Poisoned mutex: {e}");
                        return ptr::null_mut();
                    }
                }
            }
        }
        None => ptr::null_mut(),
    }
}

/// Free the event obtained from [`next_event`].
///
/// # Safety
/// The provided pointer must be null or a valid pointer obtained from [`next_event`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn drop_event(event: *mut Event) {
    if !event.is_null() {
        drop(unsafe { Box::from_raw(event) });
    }
}
