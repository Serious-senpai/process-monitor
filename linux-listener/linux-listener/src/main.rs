use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;

use aya::Ebpf;
use aya::programs::{KProbe, Xdp};
use aya_log::EbpfLogger;
use clap::Parser;
use log::LevelFilter;
use log::{info, warn};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};
use tokio::signal;

#[derive(Debug, Parser)]
struct Arguments {
    #[clap(short, long, default_value = "eth0")]
    pub interface: String,
}

fn initialize_logger(level: LevelFilter) -> Box<TermLogger> {
    let cfg = ConfigBuilder::new()
        .set_location_level(level)
        .set_time_offset_to_local()
        .unwrap_or_else(|e| e)
        .build();

    TermLogger::new(level, cfg, TerminalMode::Stderr, ColorChoice::Auto)
}

fn attach_recv(hook: &mut KProbe) -> anyhow::Result<()> {
    hook.load()?;
    hook.attach("__sys_recvmsg", 0)?;
    hook.attach("__sys_recvfrom", 0)?;
    Ok(())
}

fn attach_xdp(hook: &mut Xdp, interface: &str) -> anyhow::Result<()> {
    hook.load()?;
    hook.attach(interface, aya::programs::XdpFlags::default())?;
    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let arguments = Arguments::parse();
    let _ = log::set_boxed_logger(initialize_logger(LevelFilter::Info))
        .inspect_err(|e| warn!("Unable to set default logger: {e}"));

    let mut ebpf = Ebpf::load(aya::include_bytes_aligned!(concat!(
        env!("OUT_DIR"),
        "/linux-listener"
    )))?;

    let stopped = Arc::new(AtomicBool::new(false));
    let handle = match EbpfLogger::init(&mut ebpf) {
        Ok(mut logger) => {
            info!("eBPF logger initialized");

            let stopped = stopped.clone();
            Some(thread::spawn(move || {
                while !stopped.load(Ordering::Relaxed) {
                    logger.flush();
                    thread::sleep(Duration::from_secs(1));
                }
            }))
        }
        Err(e) => {
            warn!("Failed to init eBPF logger: {e}");
            None
        }
    };

    attach_recv(
        ebpf.program_mut("recv_variants_hook")
            .expect("Check the eBPF program again")
            .try_into()?,
    )?;
    attach_xdp(
        ebpf.program_mut("xdp_hook")
            .expect("Check the eBPF program again")
            .try_into()?,
        &arguments.interface,
    )?;

    info!("Listening for packets... Press Ctrl+C to exit.");
    signal::ctrl_c().await.expect("Unable to listen for Ctrl-C");

    info!("Received Ctrl-C. Exiting...");
    stopped.store(true, Ordering::Relaxed);
    if let Some(handle) = handle {
        handle.join().unwrap();
    }

    Ok(())
}
