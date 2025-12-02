use alloc::collections::btree_map::BTreeMap;
use core::ptr;
use core::sync::atomic::{AtomicI64, AtomicIsize, AtomicPtr};

use ffi::{StaticCommandName, Threshold};
use lru::LruCache;
use wdk_sys::DRIVER_OBJECT;

use crate::mpsc::UserChannelMap;
use crate::wrappers::lock::SpinLock;
use crate::wrappers::user_object::UserEventObject;
use crate::wrappers::wfp::WFPTracer;

pub struct SharedMemory {
    pub queue: UserChannelMap,
    pub event: UserEventObject,
}

pub struct DriverState {
    pub driver: AtomicPtr<DRIVER_OBJECT>,
    pub shared_memory: AtomicPtr<SharedMemory>,
    pub minifilter: AtomicIsize,
    pub wfp: AtomicPtr<WFPTracer>,
    pub thresholds: AtomicPtr<SpinLock<BTreeMap<StaticCommandName, Threshold>>>,
    pub ticks_per_ms: AtomicI64,

    /// - **64-bit high:** timestamp of last measurement (in milliseconds)
    /// - **64-bit low:** accumulated transfered bytes
    pub disk_io: AtomicPtr<SpinLock<LruCache<(StaticCommandName, u32), u128>>>,
}

pub static DRIVER_STATE: DriverState = DriverState {
    driver: AtomicPtr::new(ptr::null_mut()),
    shared_memory: AtomicPtr::new(ptr::null_mut()),
    minifilter: AtomicIsize::new(0),
    wfp: AtomicPtr::new(ptr::null_mut()),
    thresholds: AtomicPtr::new(ptr::null_mut()),
    ticks_per_ms: AtomicI64::new(0),
    disk_io: AtomicPtr::new(ptr::null_mut()),
};
