use std::sync::Arc;
use std::time::Duration;
use std::{ptr, thread};

use aya::Ebpf;
use aya::maps::{HashMap, MapData, RingBuf};
use aya::programs::KProbe;
use aya_log::EbpfLogger;
use clap::Parser;
use linux_listener::cli::Arguments;
use linux_listener_common::types::{StaticCommandName, Threshold, Violation};
use log::{LevelFilter, SetLoggerError, debug, info, warn};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};
use tokio::io::unix::AsyncFd;
use tokio::signal;
use tokio::sync::SetOnce;

macro_rules! try_hook {
    ($hook:expr, $target:expr, $($args:expr),* $(,)?) => {
        $hook
            .attach($target, $($args),+)
            .inspect_err(|e| warn!("Unable to hook to {}: {e}", $target))
            .is_ok()
    };
}

fn initialize_logger(level: LevelFilter) -> Result<(), SetLoggerError> {
    let cfg = ConfigBuilder::new()
        .set_location_level(level)
        .set_time_offset_to_local()
        .unwrap_or_else(|e| e)
        .build();

    TermLogger::init(level, cfg, TerminalMode::Stderr, ColorChoice::Auto)
}

fn attach_kretprobe_network(hook: &mut KProbe) -> anyhow::Result<()> {
    hook.load()?;

    if !try_hook!(hook, "__sys_sendmsg", 0) && !try_hook!(hook, "__x64_sys_sendmsg", 0) {
        warn!("Unable to hook to sendmsg");
    }

    if !try_hook!(hook, "__sys_write", 0) && !try_hook!(hook, "__x64_sys_write", 0) {
        warn!("Unable to hook to write");
    }

    if !try_hook!(hook, "__sys_sendfile", 0) && !try_hook!(hook, "__x64_sys_sendfile", 0) {
        warn!("Unable to hook to sendfile");
    }

    if !try_hook!(hook, "__sys_recvmsg", 0) && !try_hook!(hook, "__x64_sys_recvmsg", 0) {
        warn!("Unable to hook to recvmsg");
    }

    if !try_hook!(hook, "__sys_read", 0) && !try_hook!(hook, "__x64_sys_read", 0) {
        warn!("Unable to hook to read");
    }

    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let arguments = Arguments::parse(); // We will use this someday, but not now.

    let _ = initialize_logger(arguments.log_level)
        .inspect_err(|e| eprintln!("Unable to set default logger: {e}"));

    debug!("Loading eBPF program...");
    let mut ebpf = Ebpf::load(aya::include_bytes_aligned!(concat!(
        env!("OUT_DIR"),
        "/linux-listener"
    )))?;
    info!("eBPF program loaded");

    let stopped = Arc::new(SetOnce::new());
    let handle = match EbpfLogger::init(&mut ebpf) {
        Ok(mut logger) => {
            info!("eBPF logger initialized");

            let stopped = stopped.clone();
            Some(thread::spawn(move || {
                while !stopped.initialized() {
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

    let mut hashmap = HashMap::<MapData, StaticCommandName, Threshold>::try_from(
        ebpf.take_map("NAMES")
            .expect("Check the eBPF program again"),
    )?;

    let mut name = [0u8; 16];
    name[..4].copy_from_slice(b"curl");
    hashmap.insert(
        &name,
        Threshold {
            thresholds: [0, 0, 0, 0],
        },
        0,
    )?;

    let ring_buf = RingBuf::try_from(
        ebpf.take_map("EVENTS")
            .expect("Check the eBPF program again"),
    )?;
    let mut ring_buf_fd = AsyncFd::new(ring_buf)?;

    attach_kretprobe_network(
        ebpf.program_mut("kretprobe_network_hook")
            .expect("Check the eBPF program again")
            .try_into()?,
    )?;

    let communicate = {
        let stopped = stopped.clone();
        tokio::spawn(async move {
            info!("Started communication task");
            while !stopped.initialized() {
                let mut guard = tokio::select! {
                    _ = stopped.wait() => break,
                    guard = ring_buf_fd.readable_mut() => guard.unwrap(),
                };

                let ring_buf = guard.get_mut();
                while let Some(item) = ring_buf.get_mut().next()
                    && !stopped.initialized()
                {
                    let violation =
                        unsafe { ptr::read_unaligned(item.as_ptr() as *const Violation) };
                    info!(
                        "Received data: {violation:?}, command {:?}",
                        violation.command_name(),
                    );
                }

                guard.clear_ready();
            }
        })
    };

    info!("Listening for packets... Press Ctrl+C to exit.");
    signal::ctrl_c().await.expect("Unable to listen for Ctrl-C");

    info!("Received Ctrl-C. Exiting...");
    stopped.set(())?;

    if let Some(handle) = handle {
        handle.join().unwrap();
    }

    communicate.await?;

    Ok(())
}
