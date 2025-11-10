#![no_std]
#![no_main]

use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::bindings::{BPF_F_NO_PREALLOC, BPF_NOEXIST};
use aya_ebpf::helpers::generated::bpf_ktime_get_ns;
use aya_ebpf::macros::{kretprobe, map};
use aya_ebpf::maps::{HashMap, PerCpuHashMap, RingBuf};
use aya_ebpf::programs::RetProbeContext;
use aya_log_ebpf::warn;
use linux_listener_common::config::{MAX_PROCESS_COUNT, RING_BUFFER_SIZE};
use linux_listener_common::types::{Metric, StaticCommandName, Threshold, Violation};

struct _NetworkIo {
    pub transfered: u64,
    pub timestamp: u64,
}

#[map]
static NAMES: HashMap<StaticCommandName, Threshold> =
    HashMap::with_max_entries(MAX_PROCESS_COUNT, BPF_F_NO_PREALLOC);

#[map]
static NETWORK_IO: PerCpuHashMap<(StaticCommandName, u32), _NetworkIo> =
    PerCpuHashMap::with_max_entries(MAX_PROCESS_COUNT, BPF_F_NO_PREALLOC);

#[map]
static EVENTS: RingBuf = RingBuf::pinned(RING_BUFFER_SIZE, 0);

// View probable kernel functions using `sudo cat /proc/kallsyms`.

#[kretprobe]
pub fn kretprobe_network_hook(ctx: RetProbeContext) -> u32 {
    let size = ctx.ret::<i32>() as u64;
    match ctx.command() {
        Ok(name) => {
            let name = StaticCommandName(name);
            if size > 0
                && let Some(threshold) = unsafe { NAMES.get(&name) }
            {
                let pid = ctx.pid();
                let threshold = threshold.thresholds[Metric::Network as usize];
                let timestamp = unsafe { bpf_ktime_get_ns() };

                match NETWORK_IO.get_ptr_mut(&(name, pid)) {
                    Some(old) => {
                        let old = unsafe { &mut *old };
                        old.transfered += size;

                        let dt = timestamp - old.timestamp;
                        old.timestamp = timestamp;

                        if dt >= 1_000_000_000 {
                            let transfered = old.transfered;
                            old.transfered = 0;

                            if 1_000_000_000 * transfered / dt >= threshold {
                                match EVENTS.reserve::<Violation>(0) {
                                    Some(mut entry) => {
                                        entry.write(Violation {
                                            pid,
                                            name,
                                            metric: Metric::Network,
                                            value: transfered,
                                            threshold,
                                        });
                                        entry.submit(0);
                                    }
                                    None => {
                                        warn!(
                                            &ctx,
                                            "[kretprobe] Failed to reserve space in ring buffer"
                                        );
                                    }
                                }
                            }
                        }
                    }
                    None => {
                        let _ = NETWORK_IO.insert(
                            &(name, pid),
                            _NetworkIo {
                                transfered: size,
                                timestamp,
                            },
                            BPF_NOEXIST as _,
                        );
                    }
                }
            }
        }
        Err(e) => {
            warn!(
                &ctx,
                "[kretprobe] Failed to call bpf_get_current_comm: {}", e
            );
        }
    }

    0
}

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
#[unsafe(link_section = "license")]
static LICENSE: [u8; 13] = *b"Dual MIT/GPL\0";
