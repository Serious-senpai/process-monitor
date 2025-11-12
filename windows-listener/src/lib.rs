use std::ffi::{c_char, c_int, c_void};

// Placeholder
type Threshold = c_void;
type Violation = c_void;

pub struct KernelTracerHandle {
    _private: [u8; 0],
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn initialize_logger() -> c_int {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn set_log_level(level: c_int) -> c_int {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn new_tracer() -> *mut KernelTracerHandle {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn free_tracer(tracer: *mut KernelTracerHandle) {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn set_monitor(
    tracer: *const KernelTracerHandle,
    name: *const c_char,
    threshold: *const Threshold,
) -> c_int {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn clear_monitor(tracer: *const KernelTracerHandle) -> c_int {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn next_event(
    tracer: *const KernelTracerHandle,
    timeout_ms: c_int,
) -> *mut Violation {
    todo!()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn drop_event(event: *mut Violation) {
    todo!()
}
