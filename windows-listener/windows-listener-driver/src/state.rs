use alloc::collections::btree_map::BTreeMap;
use core::sync::atomic::AtomicPtr;

use ffi::{StaticCommandName, Threshold};
use windows::Wdk::Storage::FileSystem::Minifilters::PFLT_FILTER;

use crate::mpsc::UserChannelMap;
use crate::wrappers::lock::SpinLock;
use crate::wrappers::user_object::UserEventObject;

pub struct SharedMemory {
    pub queue: UserChannelMap,
    pub event: UserEventObject,
}

pub struct DeviceExtension {
    pub shared_memory: AtomicPtr<SharedMemory>,
    pub minifilter: AtomicPtr<PFLT_FILTER>,
    pub thresholds: SpinLock<BTreeMap<StaticCommandName, Threshold>>,
}
