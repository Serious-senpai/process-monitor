#![no_std]
#![no_main]

use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::bindings::BPF_F_NO_PREALLOC;
use aya_ebpf::macros::{kretprobe, map};
use aya_ebpf::maps::{HashMap, RingBuf};
use aya_ebpf::programs::RetProbeContext;
use aya_log_ebpf::warn;
use linux_listener_common::types::{Metric, StaticCommandName, Threshold, Violation};

#[map]
static NAMES: HashMap<StaticCommandName, Threshold> =
    HashMap::with_max_entries(512, BPF_F_NO_PREALLOC);

#[map]
static EVENTS: RingBuf = RingBuf::pinned(4096, 0);

// View probable kernel functions using `sudo cat /proc/kallsyms`.

#[kretprobe]
pub fn kretprobe_network_hook(ctx: RetProbeContext) -> u32 {
    let size = ctx.ret::<i32>();
    match ctx.command() {
        Ok(name) => {
            if size > 0
                && let Some(threshold) = unsafe { NAMES.get(name) }
            {
                match EVENTS.reserve::<Violation>(0) {
                    Some(mut entry) => {
                        entry.write(Violation {
                            pid: ctx.pid(),
                            name,
                            metric: Metric::Network,
                            value: size,
                            threshold: threshold.thresholds[Metric::Network as usize],
                        });
                        entry.submit(0);
                    }
                    None => {
                        warn!(&ctx, "[kretprobe] Failed to reserve space in ring buffer");
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
