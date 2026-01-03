use core::ffi::{c_char, c_void};
use core::ptr;

use wdk_sys::_MEMORY_CACHING_TYPE::MmCached;
use wdk_sys::_MODE::KernelMode;
use wdk_sys::ntddk::MmMapLockedPagesSpecifyCache;
use wdk_sys::{
    HANDLE, MDL, MDL_MAPPED_TO_SYSTEM_VA, MDL_SOURCE_IS_NONPAGED_POOL, OBJECT_ATTRIBUTES,
    PEPROCESS, PIO_STACK_LOCATION, PIRP, POBJECT_ATTRIBUTES, PSECURITY_DESCRIPTOR, PUNICODE_STRING,
    ULONG,
};

unsafe extern "C" {
    /// See also: https://blog.csdn.net/fearhacker/article/details/152052624 (Chinese)
    pub unsafe fn PsGetProcessImageFileName(Process: PEPROCESS) -> *const c_char;
}

/// # Safety
/// Binding to [`IoGetCurrentIrpStackLocation`](https://codemachine.com/downloads/win71/wdm.h)
#[allow(non_snake_case)]
pub unsafe fn IoGetCurrentIrpStackLocation(irp: PIRP) -> PIO_STACK_LOCATION {
    unsafe {
        debug_assert!((*irp).CurrentLocation <= (*irp).StackCount + 1);
        (*irp)
            .Tail
            .Overlay
            .__bindgen_anon_2
            .__bindgen_anon_1
            .CurrentStackLocation
    }
}

/// Binding to [`MmGetSystemAddressForMdlSafe`](https://codemachine.com/downloads/win71/wdm.h)
#[allow(non_snake_case)]
pub unsafe fn MmGetSystemAddressForMdlSafe(mdl: *mut MDL, priority: ULONG) -> *mut c_void {
    unsafe {
        if (*mdl).MdlFlags as u32 & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL) != 0 {
            (*mdl).MappedSystemVa
        } else {
            MmMapLockedPagesSpecifyCache(
                mdl,
                KernelMode as _,
                MmCached,
                ptr::null_mut(),
                0,
                priority,
            )
        }
    }
}

#[allow(non_snake_case)]
pub unsafe fn InitializeObjectAttributes(
    p: POBJECT_ATTRIBUTES,
    n: PUNICODE_STRING,
    a: ULONG,
    r: HANDLE,
    s: PSECURITY_DESCRIPTOR,
) {
    unsafe {
        (*p).Length = size_of::<OBJECT_ATTRIBUTES>() as u32;
        (*p).RootDirectory = r;
        (*p).Attributes = a;
        (*p).ObjectName = n;
        (*p).SecurityDescriptor = s;
        (*p).SecurityQualityOfService = ptr::null_mut();
    }
}

#[allow(non_snake_case)]
pub const fn ZwCurrentProcess() -> HANDLE {
    usize::MAX as HANDLE
}
