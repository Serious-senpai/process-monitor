#![no_std]
#![no_main]
#![feature(core_intrinsics)]

use core::intrinsics;
use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::bindings::{BPF_F_NO_PREALLOC, BPF_NOEXIST};
use aya_ebpf::helpers::bpf_ktime_get_ns;
use aya_ebpf::macros::{kretprobe, map};
use aya_ebpf::maps::{HashMap, LruHashMap, RingBuf};
use aya_ebpf::programs::RetProbeContext;
use aya_log_ebpf::warn;
use linux_listener_common::config::{MAX_PROCESS_COUNT, RING_BUFFER_SIZE};
use linux_listener_common::types::{Metric, StaticCommandName, Threshold, Violation};

#[map]
static NAMES: HashMap<StaticCommandName, Threshold> =
    HashMap::with_max_entries(MAX_PROCESS_COUNT, BPF_F_NO_PREALLOC);

/// - **32-bit high:** timestamp of last measurement (in milliseconds)
/// - **32-bit low:** accumulated transfered bytes
#[map]
static NETWORK_IO: LruHashMap<(StaticCommandName, u32), u64> =
    LruHashMap::with_max_entries(MAX_PROCESS_COUNT, 0);

#[map]
static EVENTS: RingBuf = RingBuf::with_byte_size(RING_BUFFER_SIZE, 0);

// View probable kernel functions using `sudo cat /proc/kallsyms`.

#[kretprobe]
pub fn kretprobe_network_hook(ctx: RetProbeContext) -> u32 {
    let size = (ctx.ret::<i32>() as u64) & 0xFFFFFFFF;
    if size == 0 {
        return 0;
    }

    match ctx.command() {
        Ok(name) => {
            let name = StaticCommandName(name);
            if let Some(threshold) = unsafe { NAMES.get(&name) } {
                let pid = ctx.pid();
                let threshold = threshold.thresholds[Metric::Network as usize];
                let timestamp_ms = unsafe { bpf_ktime_get_ns() / 1_000_000 } & 0xFFFFFFFF;

                match NETWORK_IO.get_ptr_mut(&(name, pid)) {
                    Some(packed) => {
                        unsafe {
                            intrinsics::atomic_xadd::<
                                u64,
                                u64,
                                { intrinsics::AtomicOrdering::SeqCst },
                            >(packed, size);
                        }

                        let dt = timestamp_ms - (unsafe { *packed } >> 32);
                        if dt >= 1_000 {
                            let old = unsafe {
                                intrinsics::atomic_xchg::<u64, { intrinsics::AtomicOrdering::SeqCst }>(
                                    packed,
                                    timestamp_ms << 32,
                                )
                            };
                            let transfered = old & 0xFFFFFFFF;
                            let rate = (1_000 * transfered / dt) as u32;

                            if rate >= threshold {
                                match EVENTS.reserve::<Violation>(0) {
                                    Some(mut entry) => {
                                        entry.write(Violation {
                                            pid,
                                            name,
                                            metric: Metric::Network,
                                            value: rate,
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
                            (timestamp_ms << 32) | (size & 0xFFFFFFFF),
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

// #[classifier]
// pub fn classifier_network_hook(ctx: TcContext) -> i32 {
//     TC_ACT_OK
// }

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
#[unsafe(link_section = "license")]
static LICENSE: [u8; 13] = *b"Dual MIT/GPL\0";
