use clap::Parser;
use log::LevelFilter;

#[derive(Debug, Parser)]
pub struct Arguments {
    /// Set the logging level
    #[clap(short, long, default_value = "info")]
    pub log_level: LevelFilter,
}
