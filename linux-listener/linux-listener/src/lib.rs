pub mod epoll;

use std::ffi::{CStr, c_char, c_int, c_short};
use std::os::fd::{AsFd, AsRawFd};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use std::{ptr, thread};

use aya::maps::{HashMap, MapData, RingBuf};
use aya::programs::{FExit, KProbe};
use aya::{Btf, Ebpf};
use aya_log::EbpfLogger;
use ffi::{Event, StaticCommandName, Threshold};
use log::{LevelFilter, debug, error, warn};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};

macro_rules! try_attach {
    ($hook:expr) => {
        $hook
            .attach()
            .inspect_err(|e| warn!("Unable to hook: {e}"))
            .is_ok()
    };
    ($hook:expr, $target:expr, $($args:expr),* $(,)?) => {
        $hook
            .attach($target, $($args),+)
            .inspect_err(|e| warn!("Unable to hook to {:?}: {e}", $target))
            .is_ok()
    };
}

// fn attach_kretprobe_network(hook: &mut KProbe) -> anyhow::Result<()> {
//     hook.load()?;

//     let _ = try_attach!(hook, "sock_sendmsg", 0);
//     let _ = try_attach!(hook, "sock_recvmsg", 0);

//     Ok(())
// }

fn attach_fexit_network_send(btf: &Btf, hook: &mut FExit) -> anyhow::Result<()> {
    hook.load("sock_sendmsg", &btf)?;

    let _ = try_attach!(hook);

    Ok(())
}

fn attach_fexit_network_recv(btf: &Btf, hook: &mut FExit) -> anyhow::Result<()> {
    hook.load("sock_recvmsg", &btf)?;

    let _ = try_attach!(hook);

    Ok(())
}

// This is also another option, but per-process data is hard to collect here.
// fn attach_classifier_network(hook: &mut SchedClassifier) -> anyhow::Result<()> {
//     let _ = tc::qdisc_add_clsact("enp0s3");
//     hook.load()?;

//     let _ = try_attach!(hook, "enp0s3", tc::TcAttachType::Ingress);
//     let _ = try_attach!(hook, "enp0s3", tc::TcAttachType::Egress);

//     Ok(())
// }

// fn attach_tracepoint_disk(hook: &mut TracePoint) -> anyhow::Result<()> {
//     hook.load()?;

//     let _ = try_attach!(hook, "block", "block_rq_complete");

//     Ok(())
// }

// fn attach_lsm_task_alloc(btf: &Btf, hook: &mut Lsm) -> anyhow::Result<()> {
//     hook.load("task_alloc", btf)?;

//     let _ = try_attach!(hook);

//     Ok(())
// }

fn attach_kretprobe_process_creation(hook: &mut KProbe) -> anyhow::Result<()> {
    hook.load()?;

    let _ = try_attach!(hook, "__x64_sys_execve", 0);
    let _ = try_attach!(hook, "__x64_sys_execveat", 0);

    Ok(())
}

pub struct KernelTracer {
    pub ebpf: Mutex<Ebpf>,
    pub names: Mutex<HashMap<MapData, StaticCommandName, Threshold>>,
    pub events: Mutex<RingBuf<MapData>>,

    pub stopped: Arc<AtomicBool>,
    pub logger_thread: Option<thread::JoinHandle<()>>,
}

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

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

    // Linux will reclaim the leaked memory when our library is unloaded.
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
    fn _new_tracer_inner() -> Result<KernelTracer, anyhow::Error> {
        debug!("Loading eBPF program");

        let stopped = Arc::new(AtomicBool::new(false));
        let mut ebpf = Ebpf::load(aya::include_bytes_aligned!(concat!(
            env!("OUT_DIR"),
            "/linux-listener"
        )))?;

        let logger_thread = match EbpfLogger::init(&mut ebpf) {
            Ok(mut logger) => {
                debug!("eBPF logger initialized");

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

        let btf = Btf::from_sys_fs()?;
        // attach_kretprobe_network(
        //     ebpf.program_mut("kretprobe_network_hook")
        //         .expect("Check the eBPF program again")
        //         .try_into()?,
        // )?;
        attach_fexit_network_send(
            &btf,
            ebpf.program_mut("fexit_sock_sendmsg_hook")
                .expect("Check the eBPF program again")
                .try_into()?,
        )?;
        attach_fexit_network_recv(
            &btf,
            ebpf.program_mut("fexit_sock_recvmsg_hook")
                .expect("Check the eBPF program again")
                .try_into()?,
        )?;
        // attach_classifier_network(
        //     ebpf.program_mut("classifier_network_hook")
        //         .expect("Check the eBPF program again")
        //         .try_into()?,
        // )?;
        // attach_tracepoint_disk(
        //     ebpf.program_mut("tracepoint_disk_hook")
        //         .expect("Check the eBPF program again")
        //         .try_into()?,
        // )?;
        // attach_lsm_task_alloc(
        //     &btf,
        //     ebpf.program_mut("lsm_task_alloc_hook")
        //         .expect("Check the eBPF program again")
        //         .try_into()?,
        // )?;
        attach_kretprobe_process_creation(
            ebpf.program_mut("kretprobe_process_creation")
                .expect("Check the eBPF program again")
                .try_into()?,
        )?;

        let names = HashMap::<MapData, StaticCommandName, Threshold>::try_from(
            ebpf.take_map("NAMES")
                .ok_or(anyhow::format_err!("Check the eBPF program again"))?,
        )?;
        let events = RingBuf::try_from(
            ebpf.take_map("EVENTS")
                .ok_or(anyhow::format_err!("Check the eBPF program again"))?,
        )?;

        debug!("Completed loading eBPF program");
        Ok(KernelTracer {
            ebpf: Mutex::new(ebpf),
            names: Mutex::new(names),
            events: Mutex::new(events),
            stopped,
            logger_thread,
        })
    }

    match _new_tracer_inner() {
        Ok(result) => Box::into_raw(Box::new(result)) as *mut KernelTracerHandle,
        Err(e) => {
            error!("Failed to load eBPF program: {e}");
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
    let tracer = tracer as *mut KernelTracer;
    if !tracer.is_null() {
        let tracer = unsafe { Box::from_raw(tracer) };
        tracer.stopped.store(true, Ordering::Relaxed);
        if let Some(logger_thread) = tracer.logger_thread {
            let _ = logger_thread.join();
        }
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
    let tracer = tracer as *const KernelTracer;
    if name.is_null() {
        return 1;
    }

    if let Some(tracer) = unsafe { tracer.as_ref() }
        && let Ok(name) = unsafe { CStr::from_ptr(name) }.to_str()
        && let Some(threshold) = unsafe { threshold.as_ref() }
    {
        match tracer.names.lock() {
            Ok(mut names) => {
                if let Err(e) = names.insert(StaticCommandName::from(name), threshold, 0) {
                    error!("Failed to insert key {name:?}: {e}");
                    1
                } else {
                    0
                }
            }
            Err(e) => {
                error!("NAMES is poisoned: {e}");
                1
            }
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
    let tracer = tracer as *const KernelTracer;
    if let Some(tracer) = unsafe { tracer.as_ref() } {
        match tracer.names.lock() {
            Ok(mut names) => {
                let mut current_keys = vec![];
                for item in names.iter() {
                    let Ok((key, _)) = item else {
                        error!("Failed to iterate existing keys");
                        return 1;
                    };

                    current_keys.push(key);
                }

                for key in current_keys {
                    if let Err(e) = names.remove(&key) {
                        error!("Failed to remove existing key {key:?}: {e}");
                        return 1;
                    }
                }

                0
            }
            Err(e) => {
                error!("NAMES is poisoned: {e}");
                1
            }
        }
    } else {
        1
    }
}

/// Return the next event from the kernel tracer.
/// In Linux, the following types of event may be returned:
/// - Process creation event
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
    timeout_ms: c_int,
) -> *mut Event {
    let tracer = tracer as *const KernelTracer;
    if let Some(tracer) = unsafe { tracer.as_ref() }
        && let Ok(mut events) = tracer.events.lock()
    {
        let mut poll_fd = libc::pollfd {
            fd: events.as_fd().as_raw_fd(),
            events: libc::POLLIN,
            revents: 0,
        };

        if unsafe { libc::poll(&mut poll_fd, 1, timeout_ms) } > 0 {
            const ERROR_CONDITION: c_short = libc::POLLERR | libc::POLLHUP | libc::POLLNVAL;
            if (poll_fd.revents & ERROR_CONDITION) == 0
                && (poll_fd.revents & libc::POLLIN) != 0
                && let Some(item) = events.next()
            {
                let event = unsafe { ptr::read_unaligned(item.as_ptr() as *const Event) };
                return Box::into_raw(Box::new(event));
            }
        }
    }

    ptr::null_mut()
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
