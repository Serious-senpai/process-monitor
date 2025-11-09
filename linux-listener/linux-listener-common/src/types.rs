#[cfg(feature = "std")]
use aya::Pod;
use aya_ebpf::TASK_COMM_LEN;
use serde::{Deserialize, Serialize};

#[derive(Copy, Debug, Clone, Serialize, Deserialize)]
pub struct Threshold {
    pub thresholds: [i32; 4],
}

#[cfg(feature = "std")]
unsafe impl Pod for Threshold {}

#[derive(Copy, Debug, Clone, Serialize, Deserialize)]
pub enum Metric {
    Cpu,
    Memory,
    Disk,
    Network,
}

pub type StaticCommandName = [u8; TASK_COMM_LEN];

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Violation {
    pub pid: u32,
    pub name: StaticCommandName,
    pub metric: Metric,
    pub value: i32,
    pub threshold: i32,
}

impl Violation {
    #[cfg(feature = "std")]
    pub fn command_name(&self) -> String {
        let end = self
            .name
            .iter()
            .position(|&c| c == 0)
            .unwrap_or(self.name.len());
        String::from_utf8_lossy(&self.name[..end]).to_string()
    }
}
