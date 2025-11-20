use wdk_sys::{PEPROCESS, PIO_STACK_LOCATION, PIRP};

unsafe extern "C" {
    pub unsafe fn PsGetProcessImageFileName(Process: PEPROCESS) -> *const u8;
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
