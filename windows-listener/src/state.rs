use core::sync::atomic::AtomicPtr;

use crate::mpsc::UserChannelMap;
use crate::wrappers::user_object::UserEventObject;

pub struct SharedMemory {
    pub queue: UserChannelMap,
    pub event: UserEventObject,
}

pub struct DeviceExtension {
    pub shared_memory: AtomicPtr<SharedMemory>,
}
