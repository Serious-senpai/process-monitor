#![no_std]
#![no_main]

use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::macros::{kretprobe, map};
use aya_ebpf::maps::RingBuf;
use aya_ebpf::programs::RetProbeContext;
// use aya_log_ebpf::{info, warn};

#[map]
static EVENTS: RingBuf = RingBuf::pinned(4096, 0);

// View probable kernel functions using `sudo cat /proc/kallsyms`.

#[kretprobe]
pub fn kretprobe_send_hook(ctx: RetProbeContext) -> u32 {
    let size = ctx.ret::<i32>();
    if ctx.pid() == 17270 && size > 0 {
        match EVENTS.reserve::<(u32, i32)>(0) {
            Some(mut entry) => {
                entry.write((ctx.pid(), size));
                entry.submit(0);
            }
            None => {
                // warn!(&ctx, "[kretprobe] Failed to reserve space in ring buffer");
            }
        }

        // info!(
        //     &ctx,
        //     "[kretprobe] Process {} sent {} bytes",
        //     ctx.pid(),
        //     size
        // );
    }

    0
}

#[kretprobe]
pub fn kretprobe_receive_hook(ctx: RetProbeContext) -> u32 {
    let size = ctx.ret::<i32>();
    if ctx.pid() == 17270 && size > 0 {
        match EVENTS.reserve::<(u32, i32)>(0) {
            Some(mut entry) => {
                entry.write((ctx.pid(), size));
                entry.submit(0);
            }
            None => {
                // warn!(&ctx, "[kretprobe] Failed to reserve space in ring buffer");
            }
        }

        // info!(
        //     &ctx,
        //     "[kretprobe] Process {} received {} bytes",
        //     ctx.pid(),
        //     size
        // );
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
