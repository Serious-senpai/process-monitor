use alloc::boxed::Box;
use alloc::vec;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::fmt::{Debug, Display};
use core::{fmt, slice};

use wdk::nt_success;
use wdk_sys::ntddk::{RtlAnsiStringToUnicodeString, RtlInitAnsiString};
use wdk_sys::{PASSIVE_LEVEL, PCUNICODE_STRING, PUNICODE_STRING, STRING, UNICODE_STRING};

use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::wrappers::irql::irql_requires;

/// Wrapper around an owned [`UNICODE_STRING`] structure.
pub struct UnicodeString {
    /// The `UNICODE_STRING.Buffer` field points to `buffer.as_ptr()`.
    _native: UNICODE_STRING,
    _buffer: Box<Vec<u16>>,
}

impl UnicodeString {
    pub fn into_raw_parts(self) -> (UNICODE_STRING, Box<Vec<u16>>) {
        (self._native, self._buffer)
    }

    fn _clone_to_native(&self) -> Result<(UNICODE_STRING, Box<Vec<u16>>), RuntimeError> {
        let buffer = self._buffer.clone();

        let bytes_count = u16::try_from(buffer.len().saturating_mul(2))?;
        let native = UNICODE_STRING {
            Length: bytes_count.saturating_sub(2),
            MaximumLength: bytes_count,
            Buffer: buffer.as_ptr() as *mut u16,
        };

        Ok((native, buffer))
    }

    pub fn with_cloned_native<F, R>(&self, f: F) -> Result<R, RuntimeError>
    where
        F: FnOnce(PUNICODE_STRING) -> R,
    {
        let (mut native, buffer) = self._clone_to_native()?;
        let result = f(&mut native);
        drop(buffer);

        Ok(result)
    }

    /// Clone a Unicode string from a raw pointer to a [`UNICODE_STRING`] structure.
    ///
    /// # Safety
    /// The pointer must point to a valid [`UNICODE_STRING`] structure or be null.
    pub unsafe fn from_raw(value: PCUNICODE_STRING) -> Result<Self, RuntimeError> {
        let new = Box::new(match unsafe { value.as_ref() } {
            Some(s) => {
                let buf = unsafe { slice::from_raw_parts(s.Buffer, usize::from(s.Length / 2)) };
                let mut buf = buf.to_vec();
                buf.push(0);
                buf
            }
            None => vec![0],
        });

        let bytes_count = u16::try_from(new.len().saturating_mul(2))?;
        Ok(Self {
            _native: UNICODE_STRING {
                Length: bytes_count.saturating_sub(2),
                MaximumLength: bytes_count,
                Buffer: new.as_ptr() as *mut u16,
            },
            _buffer: new,
        })
    }
}

impl TryFrom<&AnsiString> for UnicodeString {
    type Error = RuntimeError;

    /// # Complexity
    /// Linear in the length of the string.
    fn try_from(value: &AnsiString) -> Result<Self, Self::Error> {
        irql_requires(PASSIVE_LEVEL)?;

        let mut buffer = vec![0; value._buffer.len()];
        let mut native = UNICODE_STRING {
            Length: 0,
            MaximumLength: u16::try_from(buffer.len().saturating_mul(2))?,
            Buffer: buffer.as_mut_ptr(),
        };

        // `RtlAnsiStringToUnicodeString` converts the string to UTF-16 (a deep copy but no allocation is performed)
        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlansistringtounicodestring
        let mut ansi_copied = value._native;
        let status = unsafe { RtlAnsiStringToUnicodeString(&mut native, &mut ansi_copied, 0) };
        if !nt_success(status) {
            return Err(RuntimeError::Failure(status));
        }

        let moved = Box::new(buffer);
        native.Buffer = moved.as_ptr() as *mut u16;
        Ok(Self {
            _native: native,
            _buffer: moved,
        })
    }
}

impl TryFrom<&CStr> for UnicodeString {
    type Error = RuntimeError;

    /// # Complexity
    /// Linear in the length of the string.
    fn try_from(value: &CStr) -> Result<Self, Self::Error> {
        let ansi = AnsiString::from(value);
        Self::try_from(&ansi)
    }
}

impl Display for UnicodeString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&ForeignDisplayer::Unicode(&self._native), f)
    }
}

impl Debug for UnicodeString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(&ForeignDisplayer::Unicode(&self._native), f)
    }
}

/// Wrapper around an owned [`STRING`] structure.
pub struct AnsiString {
    /// The `UNICODE_STRING.Buffer` field points to `buffer.as_ptr()`.
    _native: STRING,
    _buffer: Box<Vec<u8>>,
}

impl AnsiString {
    // fn _clone_to_native(&self) -> Result<(STRING, Vec<u8>), RuntimeError> {
    //     let buffer = self._buffer.clone();

    //     let bytes_count = u16::try_from(buffer.len())?;
    //     let native = STRING {
    //         Length: bytes_count.saturating_sub(1),
    //         MaximumLength: bytes_count,
    //         Buffer: buffer.as_ptr() as *mut i8,
    //     };

    //     Ok((native, buffer))
    // }
}

impl From<&CStr> for AnsiString {
    /// # Complexity
    /// Linear in the length of the string.
    fn from(value: &CStr) -> Self {
        let mut native = STRING::default();
        let buffer = Box::new(value.to_bytes_with_nul().to_vec());

        // `RtlInitAnsiString` only copies the pointer
        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlinitansistring
        unsafe {
            RtlInitAnsiString(&mut native, buffer.as_ptr() as *const i8);
        }
        Self {
            _native: native,
            _buffer: buffer,
        }
    }
}
