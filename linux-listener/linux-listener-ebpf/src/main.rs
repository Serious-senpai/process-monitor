#![no_std]
#![no_main]

use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::bindings::xdp_action::XDP_PASS;
use aya_ebpf::macros::{kretprobe, xdp};
use aya_ebpf::programs::{RetProbeContext, XdpContext};
use aya_log_ebpf::info;

#[xdp]
pub fn xdp_hook(ctx: XdpContext) -> u32 {
    if ctx.data() < ctx.data_end() {
        let size = ctx.data_end() - ctx.data();
        info!(&ctx, "Process unknown received {} bytes", size);
    }

    XDP_PASS
}

#[kretprobe]
pub fn recv_variants_hook(ctx: RetProbeContext) -> u32 {
    let size = ctx.ret::<i32>();
    if size > 0 {
        info!(&ctx, "Process {} received {} bytes", ctx.pid(), size);
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
