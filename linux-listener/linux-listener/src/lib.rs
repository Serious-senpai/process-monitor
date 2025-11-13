use std::ffi::{CStr, c_char, c_int};
use std::os::fd::{AsFd, AsRawFd};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use std::{ptr, thread};

use aya::Ebpf;
use aya::maps::{HashMap, MapData, RingBuf};
use aya::programs::{KProbe, TracePoint};
use aya_log::EbpfLogger;
use linux_listener_common::{StaticCommandName, Threshold, Violation};
use log::{LevelFilter, debug, error, warn};
use simplelog::{ColorChoice, ConfigBuilder, TermLogger, TerminalMode};

macro_rules! _try_hook {
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

fn attach_kretprobe_network(hook: &mut KProbe) -> anyhow::Result<()> {
    hook.load()?;

    let _ = _try_hook!(hook, "inet_sendmsg", 0);
    let _ = _try_hook!(hook, "inet_recvmsg", 0);

    Ok(())
}

// This is also another option, but per-process data is hard to collect here.
// fn attach_classifier_network(hook: &mut SchedClassifier) -> anyhow::Result<()> {
//     let _ = tc::qdisc_add_clsact("enp0s3");
//     hook.load()?;

//     let _ = _try_hook!(hook, "enp0s3", tc::TcAttachType::Ingress);
//     let _ = _try_hook!(hook, "enp0s3", tc::TcAttachType::Egress);

//     Ok(())
// }

fn attach_tracepoint_disk(hook: &mut TracePoint) -> anyhow::Result<()> {
    hook.load()?;

    let _ = _try_hook!(hook, "block", "block_rq_complete");

    Ok(())
}

// fn attach_lsm_task_alloc(btf: &Btf, hook: &mut Lsm) -> anyhow::Result<()> {
//     hook.load("task_alloc", btf)?;

//     let _ = _try_hook!(hook);

//     Ok(())
// }

fn attach_kretprobe_process_creation(hook: &mut KProbe) -> anyhow::Result<()> {
    hook.load()?;

    let _ = _try_hook!(hook, "__x64_sys_execve", 0);
    let _ = _try_hook!(hook, "__x64_sys_execveat", 0);

    Ok(())
}

pub struct KernelTracer {
    pub ebpf: Mutex<Ebpf>,
    pub names: Mutex<HashMap<MapData, StaticCommandName, Threshold>>,
    pub violations: Mutex<RingBuf<MapData>>,
    pub new_processes: Mutex<RingBuf<MapData>>,

    pub stopped: Arc<AtomicBool>,
    pub logger_thread: Option<thread::JoinHandle<()>>,
}

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

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
        .set_location_level(LevelFilter::Off)
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

        // let btf = Btf::from_sys_fs()?;
        attach_kretprobe_network(
            ebpf.program_mut("kretprobe_network_hook")
                .expect("Check the eBPF program again for kretprobe_network_hook")
                .try_into()?,
        )?;
        // attach_classifier_network(
        //     ebpf.program_mut("classifier_network_hook")
        //         .expect("Check the eBPF program again for classifier_network_hook")
        //         .try_into()?,
        // )?;
        attach_tracepoint_disk(
            ebpf.program_mut("tracepoint_disk_hook")
                .expect("Check the eBPF program again for tracepoint_disk_hook")
                .try_into()?,
        )?;
        // attach_lsm_task_alloc(
        //     &btf,
        //     ebpf.program_mut("lsm_task_alloc_hook")
        //         .expect("Check the eBPF program again for lsm_task_alloc_hook")
        //         .try_into()?,
        // )?;
        attach_kretprobe_process_creation(
            ebpf.program_mut("kretprobe_process_creation")
                .expect("Check the eBPF program again for kretprobe_process_creation")
                .try_into()?,
        )?;

        let names = HashMap::<MapData, StaticCommandName, Threshold>::try_from(
            ebpf.take_map("NAMES").ok_or(anyhow::format_err!(
                "Check the eBPF program again for NAMES"
            ))?,
        )?;
        let violations = RingBuf::try_from(ebpf.take_map("VIOLATIONS").ok_or(
            anyhow::format_err!("Check the eBPF program again for VIOLATIONS"),
        )?)?;
        let new_processes = RingBuf::try_from(ebpf.take_map("NEW_PROCESSES").ok_or(
            anyhow::format_err!("Check the eBPF program again for NEW_PROCESSES"),
        )?)?;

        debug!("Completed loading eBPF program");
        Ok(KernelTracer {
            ebpf: Mutex::new(ebpf),
            names: Mutex::new(names),
            violations: Mutex::new(violations),
            new_processes: Mutex::new(new_processes),
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

#[unsafe(no_mangle)]
pub unsafe extern "C" fn free_tracer(tracer: *mut KernelTracerHandle) {
    if !tracer.is_null() {
        let tracer = unsafe { Box::from_raw(tracer as *mut KernelTracer) };
        tracer.stopped.store(true, Ordering::Relaxed);
        if let Some(logger_thread) = tracer.logger_thread {
            let _ = logger_thread.join();
        }
    }
}

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
                if let Err(e) = names.insert(&name.into(), threshold, 0) {
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

#[unsafe(no_mangle)]
pub unsafe extern "C" fn next_event(
    tracer: *const KernelTracerHandle,
    timeout_ms: c_int,
) -> *mut Violation {
    let tracer = tracer as *const KernelTracer;
    if let Some(tracer) = unsafe { tracer.as_ref() }
        && let Ok(mut violations) = tracer.violations.lock()
    {
        let mut poll_fd = libc::pollfd {
            fd: violations.as_fd().as_raw_fd(),
            events: libc::POLLIN,
            revents: 0,
        };

        if unsafe { libc::poll(&mut poll_fd as *mut _, 1, timeout_ms) } > 0 {
            if let Some(item) = violations.next() {
                let violation = unsafe { ptr::read_unaligned(item.as_ptr() as *const _) };
                return Box::into_raw(Box::new(violation));
            }
        }
    }

    ptr::null_mut()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn drop_event(event: *mut Violation) {
    if !event.is_null() {
        let _ = unsafe { Box::from_raw(event) };
    }
}
