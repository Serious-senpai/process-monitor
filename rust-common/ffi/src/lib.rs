#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "linux-kernel")]
pub mod linux;

use core::{fmt, str};

#[cfg(feature = "linux-kernel")]
pub const COMMAND_LENGTH: usize = 16; // aya_ebpf::TASK_COMM_LEN

#[cfg(not(feature = "linux-kernel"))]
pub const COMMAND_LENGTH: usize = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Threshold {
    pub thresholds: [u32; 4],
}

#[cfg(feature = "linux-user")]
unsafe impl aya::Pod for Threshold {}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Metric {
    Cpu = 0,
    Memory = 1,
    Disk = 2,
    Network = 3,
}

#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct StaticCommandName(pub [u8; COMMAND_LENGTH]);

#[cfg(feature = "linux-user")]
unsafe impl aya::Pod for StaticCommandName {}

impl From<&str> for StaticCommandName {
    fn from(value: &str) -> Self {
        let mut buffer = [0u8; COMMAND_LENGTH];

        let bytes = value.as_bytes();
        let len = bytes.len().min(COMMAND_LENGTH - 1); // Exclude null terminator
        buffer[..len].copy_from_slice(&bytes[..len]);

        Self(buffer)
    }
}

impl fmt::Display for StaticCommandName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let buffer = &self.0;
        let end = buffer.iter().position(|&c| c == 0).unwrap_or(buffer.len());
        let s = str::from_utf8(&buffer[..end]).map_err(|_| fmt::Error)?;
        write!(f, "{s}")
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Violation {
    pub metric: Metric,
    pub value: u32,
    pub threshold: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct NewProcess;

#[repr(C)]
#[derive(Clone, Copy)]
pub union EventData {
    pub violation: Violation,
    pub new_process: NewProcess,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum EventType {
    Violation = 0,
    NewProcess = 1,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Event {
    pub pid: u32,
    pub name: StaticCommandName,
    pub variant: EventType,
    pub data: EventData,
}

impl Event {
    #[cfg(feature = "std")]
    pub fn command_name(&self) -> String {
        self.name.to_string()
    }
}
