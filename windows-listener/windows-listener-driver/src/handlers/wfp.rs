use crate::log;

pub unsafe extern "C" fn wfp_callback(pid: u64, size: usize) {
    log!("WFP callback: pid = {pid}, size = {size}");
}
