#[cfg(feature = "std")]
use aya::Pod;
use aya_ebpf::TASK_COMM_LEN;
use serde::{Deserialize, Serialize};

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Threshold {
    pub thresholds: [u32; 4],
}

#[cfg(feature = "std")]
unsafe impl Pod for Threshold {}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum Metric {
    Cpu,
    Memory,
    Disk,
    Network,
}

#[repr(transparent)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct StaticCommandName(pub [u8; TASK_COMM_LEN]);

#[cfg(feature = "std")]
unsafe impl Pod for StaticCommandName {}

impl From<&str> for StaticCommandName {
    fn from(value: &str) -> Self {
        let mut buffer = [0u8; TASK_COMM_LEN];

        let bytes = value.as_bytes();
        let len = bytes.len().min(TASK_COMM_LEN - 1); // Exclude null terminator
        buffer[..len].copy_from_slice(&bytes[..len]);

        StaticCommandName(buffer)
    }
}

#[repr(C)]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Violation {
    pub pid: u32,
    pub name: StaticCommandName,
    pub metric: Metric,
    pub value: u32,
    pub threshold: u32,
}

impl Violation {
    #[cfg(feature = "std")]
    pub fn command_name(&self) -> String {
        let buffer = &self.name.0;
        let end = buffer.iter().position(|&c| c == 0).unwrap_or(buffer.len());
        String::from_utf8_lossy(&buffer[..end]).to_string()
    }
}
