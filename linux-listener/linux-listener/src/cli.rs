use clap::Parser;
use log::LevelFilter;

#[derive(Debug, Parser)]
pub struct Arguments {
    /// The PID to monitor
    pub pid: u32,

    /// Set the logging level
    #[clap(short, long, default_value = "info")]
    pub log_level: LevelFilter,
}
