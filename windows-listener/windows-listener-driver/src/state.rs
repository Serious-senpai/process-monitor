use alloc::collections::btree_map::BTreeMap;
use core::ptr;
use core::sync::atomic::{AtomicIsize, AtomicPtr};

use ffi::{StaticCommandName, Threshold};
use wdk_sys::DRIVER_OBJECT;

use crate::mpsc::UserChannelMap;
use crate::wrappers::lock::SpinLock;
use crate::wrappers::user_object::UserEventObject;

pub struct SharedMemory {
    pub queue: UserChannelMap,
    pub event: UserEventObject,
}

pub struct DriverState {
    pub driver: AtomicPtr<DRIVER_OBJECT>,
    pub shared_memory: AtomicPtr<SharedMemory>,
    pub minifilter: AtomicIsize,
    pub thresholds: AtomicPtr<SpinLock<BTreeMap<StaticCommandName, Threshold>>>,
}

pub static DRIVER_STATE: DriverState = DriverState {
    driver: AtomicPtr::new(ptr::null_mut()),
    shared_memory: AtomicPtr::new(ptr::null_mut()),
    minifilter: AtomicIsize::new(0),
    thresholds: AtomicPtr::new(ptr::null_mut()),
};
