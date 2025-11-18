use core::ffi::c_int;

/// # Safety
/// This function is just marked as `unsafe` because it is exposed via `extern "C"`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn initialize_logger(level: c_int) -> c_int {
    level
}
