use core::ffi::c_void;
use core::ptr;

use ffi::win32::mpsc::DefaultChannel;
use wdk::nt_success;
use wdk_sys::_LOCK_OPERATION::IoModifyAccess;
use wdk_sys::_MM_PAGE_PRIORITY::NormalPagePriority;
use wdk_sys::_MODE::KernelMode;
use wdk_sys::_SECTION_INHERIT::ViewUnmap;
use wdk_sys::ntddk::{
    IoAllocateMdl, IoFreeMdl, MmProbeAndLockPages, MmUnlockPages, ZwClose, ZwCreateSection,
    ZwMapViewOfSection, ZwUnmapViewOfSection,
};
use wdk_sys::{
    HANDLE, LARGE_INTEGER, MDL, PAGE_READWRITE, PASSIVE_LEVEL, POBJECT_ATTRIBUTES, SEC_COMMIT,
    SECTION_MAP_READ, SECTION_MAP_WRITE,
};

use crate::error::RuntimeError;
use crate::log;
use crate::wrappers::bindings::{MmGetSystemAddressForMdlSafe, ZwCurrentProcess};
use crate::wrappers::irql::irql_requires;

pub struct SharedMemorySection {
    _section: HANDLE,
    _base_address: *mut c_void,
    _mdl: *mut MDL,
    _channel_locked: *const DefaultChannel,
}

impl SharedMemorySection {
    pub fn new(attributes: POBJECT_ATTRIBUTES) -> Result<Self, RuntimeError> {
        irql_requires(PASSIVE_LEVEL)?;

        let mut result = Self {
            _section: HANDLE::default(),
            _base_address: ptr::null_mut(),
            _mdl: ptr::null_mut(),
            _channel_locked: ptr::null(),
        };

        let mut section_size = LARGE_INTEGER::default();
        let status = unsafe {
            section_size.QuadPart = size_of::<DefaultChannel>().try_into()?;
            ZwCreateSection(
                &mut result._section,
                SECTION_MAP_READ | SECTION_MAP_WRITE,
                attributes,
                &mut section_size,
                PAGE_READWRITE,
                SEC_COMMIT,
                HANDLE::default(),
            )
        };
        if !nt_success(status) {
            log!("Failed to call ZwCreateSection: {status}");
            return Err(RuntimeError::Failure(status));
        }

        let mut size = 0;
        let status = unsafe {
            ZwMapViewOfSection(
                result._section,
                ZwCurrentProcess(),
                &mut result._base_address,
                0,
                size_of::<DefaultChannel>().try_into()?,
                ptr::null_mut(),
                &mut size,
                ViewUnmap,
                0,
                PAGE_READWRITE,
            )
        };
        if !nt_success(status) {
            log!("Failed to call ZwMapViewOfSection: {status}");
            return Err(RuntimeError::Failure(status));
        }

        result._mdl = unsafe {
            IoAllocateMdl(
                result._base_address,
                size_of::<DefaultChannel>() as _,
                0,
                0,
                ptr::null_mut(),
            )
        };
        if result._mdl.is_null() {
            return Err(RuntimeError::Custom("IoAllocateMdl failed"));
        }

        let kernel_mode = KernelMode.try_into()?;
        let normal_page_priority = NormalPagePriority.try_into()?;
        result._channel_locked = unsafe {
            // FIXME: Handle exception from `MmProbeAndLockPages`?
            MmProbeAndLockPages(result._mdl, kernel_mode, IoModifyAccess);
            MmGetSystemAddressForMdlSafe(result._mdl, normal_page_priority)
        } as *const DefaultChannel;

        if result._channel_locked.is_null() {
            return Err(RuntimeError::Custom("MmGetSystemAddressForMdlSafe failed"));
        }

        unsafe {
            ptr::write(
                result._channel_locked as *mut DefaultChannel,
                DefaultChannel::new(),
            );
        }

        Ok(result)
    }

    pub fn send(&self, data: &[u8]) -> Result<(), usize> {
        unsafe { &*self._channel_locked }.write(data)
    }
}

impl Drop for SharedMemorySection {
    fn drop(&mut self) {
        unsafe {
            if !self._mdl.is_null() {
                MmUnlockPages(self._mdl);
                IoFreeMdl(self._mdl);
            }

            if !self._base_address.is_null() {
                let _ = ZwUnmapViewOfSection(ZwCurrentProcess(), self._base_address);
            }

            if self._section != HANDLE::default() {
                let _ = ZwClose(self._section);
            }
        }
    }
}
