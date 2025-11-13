use core::error::Error;
use core::fmt;
use core::num::TryFromIntError;

use wdk_sys::{KIRQL, NTSTATUS};

#[derive(Debug)]
pub enum RuntimeError {
    Failure(NTSTATUS),
    InvalidIRQL(KIRQL),
    ConversionError(TryFromIntError),
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Failure(status) => write!(f, "Operation failed with status {status}"),
            Self::InvalidIRQL(irql) => write!(f, "Invalid IRQL {irql}"),
            Self::ConversionError(error) => write!(f, "Conversion error: {error}"),
        }
    }
}

impl Error for RuntimeError {}

impl From<TryFromIntError> for RuntimeError {
    fn from(value: TryFromIntError) -> Self {
        Self::ConversionError(value)
    }
}
