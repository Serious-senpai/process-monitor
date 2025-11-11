#![no_std]
#![no_main]
#![feature(adt_const_params)]
#![feature(core_intrinsics)]

use core::intrinsics;
use core::ops::Deref;
use core::panic::PanicInfo;

use aya_ebpf::EbpfContext;
use aya_ebpf::bindings::{BPF_F_NO_PREALLOC, BPF_NOEXIST};
use aya_ebpf::helpers::bpf_ktime_get_ns;
use aya_ebpf::macros::{kretprobe, map, tracepoint};
use aya_ebpf::maps::{HashMap, LruHashMap, RingBuf};
use aya_ebpf::programs::{RetProbeContext, TracePointContext};
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

/// - **32-bit high:** timestamp of last measurement (in milliseconds)
/// - **32-bit low:** accumulated transfered bytes
#[map]
static DISK_IO: LruHashMap<(StaticCommandName, u32), u64> =
    LruHashMap::with_max_entries(MAX_PROCESS_COUNT, 0);

#[map]
static EVENTS: RingBuf = RingBuf::with_byte_size(RING_BUFFER_SIZE, 0);

struct _Atomic<T: Copy> {
    _ptr: *mut T,
}

impl<T: Copy> _Atomic<T> {
    pub fn new(ptr: *mut T) -> Self {
        Self { _ptr: ptr }
    }

    // For some reasons, LLVM fails to compile atomic_load in eBPF?
    // pub fn load<const ORD: intrinsics::AtomicOrdering>(&self) -> T {
    //     unsafe { intrinsics::atomic_load::<T, ORD>(self._ptr) }
    // }

    pub fn fetch_add<const ORD: intrinsics::AtomicOrdering>(&self, val: T) -> T {
        unsafe { intrinsics::atomic_xadd::<T, T, ORD>(self._ptr, val) }
    }

    pub fn swap<const ORD: intrinsics::AtomicOrdering>(&self, val: T) -> T {
        unsafe { intrinsics::atomic_xchg::<T, ORD>(self._ptr, val) }
    }
}

impl<T: Copy> Deref for _Atomic<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self._ptr }
    }
}

fn _update_io_usage<T: EbpfContext>(
    ctx: T,
    map: &LruHashMap<(StaticCommandName, u32), u64>,
    size: u64,
    metric: Metric,
) {
    match ctx.command() {
        Ok(name) => {
            let name = StaticCommandName(name);
            if let Some(threshold) = unsafe { NAMES.get(&name) } {
                let pid = ctx.pid();
                let threshold = threshold.thresholds[metric as usize];
                let timestamp_ms = unsafe { bpf_ktime_get_ns() / 1_000_000 } & 0xFFFFFFFF;

                match map.get_ptr_mut(&(name, pid)) {
                    Some(packed) => {
                        let atomic = _Atomic::new(packed);
                        atomic.fetch_add::<{ intrinsics::AtomicOrdering::Release }>(size);

                        let dt = timestamp_ms.saturating_sub(*atomic >> 32);
                        if dt >= 1_000 {
                            let old = atomic.swap::<{ intrinsics::AtomicOrdering::Release }>(
                                timestamp_ms << 32,
                            );
                            let transfered = old & 0xFFFFFFFF;
                            let rate = (1_000 * transfered / dt) as u32;

                            if rate >= threshold {
                                match EVENTS.reserve::<Violation>(0) {
                                    Some(mut entry) => {
                                        entry.write(Violation {
                                            pid,
                                            name,
                                            metric,
                                            value: rate,
                                            threshold,
                                        });
                                        entry.submit(0);
                                    }
                                    None => {
                                        warn!(&ctx, "Failed to reserve space in ring buffer");
                                    }
                                }
                            }
                        }
                    }
                    None => {
                        let _ = map.insert(
                            &(name, pid),
                            (timestamp_ms << 32) | (size & 0xFFFFFFFF),
                            BPF_NOEXIST as _,
                        );
                    }
                }
            }
        }
        Err(e) => {
            warn!(&ctx, "Failed to call bpf_get_current_comm: {}", e);
        }
    }
}

// View probable kernel functions using `sudo cat /proc/kallsyms`.

#[kretprobe]
pub fn kretprobe_network_hook(ctx: RetProbeContext) -> u32 {
    let size = (ctx.ret::<i32>() as u64) & 0xFFFFFFFF;
    if size == 0 {
        return 0;
    }

    _update_io_usage(ctx, &NETWORK_IO, size, Metric::Network);

    0
}

// #[classifier]
// pub fn classifier_network_hook(ctx: TcContext) -> i32 {
//     TC_ACT_OK
// }

// View tracepoints using `sudo ls /sys/kernel/debug/tracing/events`.

#[tracepoint]
pub fn tracepoint_disk_hook(ctx: TracePointContext) -> u32 {
    let nr_sector = unsafe { ctx.read_at::<u64>(16) }.unwrap_or_default();
    if nr_sector == 0 {
        return 0;
    }

    _update_io_usage(ctx, &DISK_IO, nr_sector * 512, Metric::Disk);

    0
}

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
#[unsafe(link_section = "license")]
static LICENSE: [u8; 13] = *b"Dual MIT/GPL\0";
