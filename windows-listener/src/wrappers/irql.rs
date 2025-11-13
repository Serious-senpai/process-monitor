use wdk_sys::ntddk::KeGetCurrentIrql;

use crate::error::RuntimeError;

/// Ensure the current IRQL is less than or equal to the specified level.
pub fn irql_requires(irql: impl Into<u64>) -> Result<(), RuntimeError> {
    let current = unsafe { KeGetCurrentIrql() };
    if u64::from(current) > irql.into() {
        return Err(RuntimeError::InvalidIRQL(current));
    }

    Ok(())
}
