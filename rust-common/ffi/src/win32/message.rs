use wdk_sys::{FILE_ANY_ACCESS, FILE_DEVICE_UNKNOWN, HANDLE, METHOD_BUFFERED};

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

pub struct MemoryInitialize {
    pub section: HANDLE,
    pub event: HANDLE,
}
