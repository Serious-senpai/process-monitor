use core::ffi::c_void;
use core::ptr;

use ffi::win32::mpsc::DefaultChannel;
use wdk::nt_success;
use wdk_sys::_MODE::UserMode;
use wdk_sys::ntddk::{MmMapViewInSystemSpace, MmUnmapViewInSystemSpace};
use wdk_sys::{HANDLE, SECTION_MAP_READ, SECTION_MAP_WRITE};

use crate::error::RuntimeError;
use crate::wrappers::user_object::UserObjectMap;

pub struct UserChannelMap {
    _object: UserObjectMap,
    _channel: *const DefaultChannel,
}

impl UserChannelMap {
    pub fn from_userspace(section: HANDLE) -> Result<Self, RuntimeError> {
        let object = UserObjectMap::from_userspace(
            section,
            SECTION_MAP_READ | SECTION_MAP_WRITE,
            ptr::null_mut(),
            UserMode.try_into()?,
        )?;

        let mut view_size = size_of::<DefaultChannel>() as _;
        let mut channel = ptr::null_mut();

        let status = unsafe { MmMapViewInSystemSpace(object.get(), &mut channel, &mut view_size) };
        if !nt_success(status) {
            return Err(RuntimeError::Failure(status));
        }

        // Initialize the channel memory ourselves. Do not trust memory from userspace.
        unsafe {
            ptr::write_volatile(channel as *mut DefaultChannel, DefaultChannel::new());
        }

        Ok(Self {
            _object: object,
            _channel: channel as *const _,
        })
    }

    pub fn send(&self, data: &[u8]) -> Result<(), usize> {
        unsafe {
            let channel = &*self._channel;
            channel.write(data)
        }
    }
}

impl Drop for UserChannelMap {
    fn drop(&mut self) {
        unsafe {
            let _ = MmUnmapViewInSystemSpace(self._channel as *mut c_void);
        }
    }
}
