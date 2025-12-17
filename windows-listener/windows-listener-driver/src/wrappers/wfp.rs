use wdk_sys::{PDEVICE_OBJECT, WCHAR};

#[repr(C)]
pub struct WFPTracer {
    _private: [u8; 0],
}

pub type WFPTracerHandle = *mut WFPTracer;

unsafe extern "C" {
    pub unsafe fn free_wfp_tracer(tracer: WFPTracerHandle);
    pub unsafe fn new_wfp_tracer(
        device: PDEVICE_OBJECT,
        callback: unsafe extern "C" fn(
            device: PDEVICE_OBJECT,
            pid: u64,
            process_name: *mut WCHAR,
            size: usize,
        ),
    ) -> WFPTracerHandle;
}
