use core::ffi::c_void;
use core::ptr;

use ffi::win32::mpsc::DefaultChannel;
use wdk::nt_success;
use wdk_sys::_LOCK_OPERATION::IoModifyAccess;
use wdk_sys::_MM_PAGE_PRIORITY::NormalPagePriority;
use wdk_sys::_MODE::{KernelMode, UserMode};
use wdk_sys::ntddk::{
    IoAllocateMdl, IoFreeMdl, MmMapViewInSystemSpace, MmProbeAndLockPages, MmUnlockPages,
    MmUnmapViewInSystemSpace,
};
use wdk_sys::{HANDLE, MDL, PASSIVE_LEVEL, SECTION_MAP_READ, SECTION_MAP_WRITE};

use crate::error::RuntimeError;
use crate::wrappers::bindings::MmGetSystemAddressForMdlSafe;
use crate::wrappers::irql::irql_requires;
use crate::wrappers::user_object::UserObjectMap;

pub struct UserChannelMap {
    _object: UserObjectMap,
    _channel: *const DefaultChannel,
    _channel_locked: *const DefaultChannel,
    _mdl: *mut MDL,
}

impl UserChannelMap {
    pub fn from_userspace(section: HANDLE) -> Result<Self, RuntimeError> {
        irql_requires(PASSIVE_LEVEL)?;
        let user_mode = UserMode.try_into()?;
        let kernel_mode = KernelMode.try_into()?;
        let normal_page_priority = NormalPagePriority.try_into()?;

        let object = UserObjectMap::from_userspace(
            section,
            SECTION_MAP_READ | SECTION_MAP_WRITE,
            ptr::null_mut(),
            user_mode,
        )?;

        let mut view_size = size_of::<DefaultChannel>() as _;
        let mut channel = ptr::null_mut();

        let status = unsafe { MmMapViewInSystemSpace(object.get(), &mut channel, &mut view_size) };
        if !nt_success(status) {
            return Err(RuntimeError::Failure(status));
        }

        let mdl = unsafe { IoAllocateMdl(channel, view_size as _, 0, 0, ptr::null_mut()) };
        if mdl.is_null() {
            let _ = unsafe { MmUnmapViewInSystemSpace(channel) };
            return Err(RuntimeError::Custom("IoAllocateMdl failed"));
        }

        let channel_locked = unsafe {
            // FIXME: Handle exception from `MmProbeAndLockPages`?
            MmProbeAndLockPages(mdl, kernel_mode, IoModifyAccess);
            MmGetSystemAddressForMdlSafe(mdl, normal_page_priority) as *const DefaultChannel
        };

        if channel_locked.is_null() {
            unsafe {
                MmUnlockPages(mdl);
                IoFreeMdl(mdl);
                let _ = MmUnmapViewInSystemSpace(channel);
            }
            return Err(RuntimeError::Custom("MmGetSystemAddressForMdlSafe failed"));
        }

        unsafe {
            ptr::write(channel_locked as *mut DefaultChannel, DefaultChannel::new());
        }

        Ok(Self {
            _object: object,
            _channel: channel as *const _,
            _channel_locked: channel_locked,
            _mdl: mdl,
        })
    }

    pub fn send(&self, data: &[u8]) -> Result<(), usize> {
        unsafe { &*self._channel_locked }.write(data)
    }
}

impl Drop for UserChannelMap {
    fn drop(&mut self) {
        unsafe {
            MmUnlockPages(self._mdl);
            IoFreeMdl(self._mdl);
            let _ = MmUnmapViewInSystemSpace(self._channel as *mut c_void);
        }
    }
}
