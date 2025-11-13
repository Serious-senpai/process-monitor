use core::ptr::null_mut;
use core::sync::atomic::Ordering;

use wdk_sys::DRIVER_OBJECT;

use crate::config::DRIVER;
use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::handlers::delete_device;
use crate::log;

pub fn driver_unload(driver: &mut DRIVER_OBJECT) -> Result<(), RuntimeError> {
    log!(
        "driver_unload {:?}",
        ForeignDisplayer::Unicode(&driver.DriverName),
    );

    // remove_create_thread_notify(thread_notify).inspect_err(|e| {
    //     log!("Failed to remove thread notify: {e}");
    // })?;

    // remove_create_process_notify(process_notify).inspect_err(|e| {
    //     log!("Failed to remove process notify: {e}");
    // })?;

    delete_device(driver);
    DRIVER.store(null_mut(), Ordering::SeqCst);

    Ok(())
}
