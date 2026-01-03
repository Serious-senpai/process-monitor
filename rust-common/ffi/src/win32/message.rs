use wdk_sys::{FILE_ANY_ACCESS, FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, WCHAR};

use crate::{StaticCommandName, Threshold};

/// Port of the [`CTL_CODE`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d4drvif/nf-d4drvif-ctl_code) macro.
///
/// See also: <https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/defining-i-o-control-codes>
const fn _ctl_code(device_type: u32, function: u32, method: u32, access: u32) -> u32 {
    (device_type << 16) | (access << 14) | (function << 2) | method
}

pub const IOCTL_MEMORY_INITIALIZE: u32 =
    _ctl_code(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
pub const IOCTL_CLEAR_MONITOR: u32 =
    _ctl_code(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);
pub const IOCTL_SET_MONITOR: u32 =
    _ctl_code(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS);

/// Message structure to be sent during [`IOCTL_MEMORY_INITIALIZE`]
#[derive(Debug)]
pub struct MemoryInitialize {
    /// Name of the section created via `ZwCreateSection`
    pub section: [WCHAR; 64],

    /// Name of the event created via `ZwCreateEvent`
    pub event: [WCHAR; 64],

    /// Size of `section` in bytes
    pub size: usize,
}

/// Message structure to be sent during [`IOCTL_SET_MONITOR`]
#[derive(Debug)]
pub struct SetMonitor {
    /// Name of the process to be monitored
    pub name: StaticCommandName,

    /// Threshold for the process
    pub threshold: Threshold,
}
