use alloc::vec;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::fmt::{Debug, Display};
use core::{fmt, slice};

use ouroboros::self_referencing;
use wdk_sys::ntddk::{RtlAnsiStringToUnicodeString, RtlInitAnsiString};
use wdk_sys::{NT_SUCCESS, PASSIVE_LEVEL, PCUNICODE_STRING, STRING, UNICODE_STRING};

use crate::displayer::ForeignDisplayer;
use crate::error::RuntimeError;
use crate::wrappers::irql::irql_requires;
use crate::wrappers::phantom::Lifetime;

/// Wrapper around an owned [`UNICODE_STRING`] structure.
#[self_referencing]
pub struct UnicodeString {
    /// The `UNICODE_STRING.Buffer` field points to `buffer.as_ptr()`.
    native: UNICODE_STRING,
    buffer: Vec<u16>,

    /// By using this field, we statically guarantee that `buffer` never moves.
    #[borrows(buffer)]
    buffer_assert_pin: &'this [u16],
}

impl UnicodeString {
    pub fn native(&self) -> Lifetime<'_, UNICODE_STRING> {
        Lifetime::new(*self.borrow_native())
    }

    /// Clone a Unicode string from a raw pointer to a [`UNICODE_STRING`] structure.
    ///
    /// # Safety
    /// The pointer must point to a valid [`UNICODE_STRING`] structure or be null.
    pub unsafe fn from_raw(value: PCUNICODE_STRING) -> Result<Self, RuntimeError> {
        let new = match unsafe { value.as_ref() } {
            Some(s) => {
                let buf = unsafe { slice::from_raw_parts(s.Buffer, usize::from(s.Length / 2)) };
                let mut buf = buf.to_vec();
                buf.push(0);
                buf
            }
            None => vec![0],
        };

        let bytes_count = 2 * u16::try_from(new.len())?;
        Ok(Self::new(
            UNICODE_STRING {
                Length: bytes_count - 2,
                MaximumLength: bytes_count,
                Buffer: new.as_ptr() as *mut u16,
            },
            new,
            |buf| buf.as_slice(),
        ))
    }
}

impl TryFrom<&AnsiString> for UnicodeString {
    type Error = RuntimeError;

    /// # Complexity
    /// Linear in the length of the string.
    fn try_from(value: &AnsiString) -> Result<Self, Self::Error> {
        irql_requires(PASSIVE_LEVEL)?;

        let mut buffer = vec![0; value.borrow_buffer().len()];
        let mut native = UNICODE_STRING {
            Length: 0,
            MaximumLength: 2 * u16::try_from(buffer.len())?,
            Buffer: buffer.as_mut_ptr(),
        };

        // `RtlAnsiStringToUnicodeString` converts the string to UTF-16 (a deep copy but no allocation is performed)
        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlansistringtounicodestring
        let status = unsafe {
            let value = value.borrow_native() as *const STRING as *mut STRING;
            RtlAnsiStringToUnicodeString(&mut native, value, 0)
        };

        if !NT_SUCCESS(status) {
            return Err(RuntimeError::Failure(status));
        }

        Ok(Self::new(native, buffer, |buf| buf.as_slice()))
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
        Display::fmt(&ForeignDisplayer::Unicode(self.borrow_native()), f)
    }
}

impl Debug for UnicodeString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(&ForeignDisplayer::Unicode(self.borrow_native()), f)
    }
}

/// Wrapper around an owned [`STRING`] structure.
#[self_referencing]
pub struct AnsiString {
    /// The `UNICODE_STRING.Buffer` field points to `buffer.as_ptr()`.
    native: STRING,
    buffer: Vec<u8>,

    /// By using this field, we statically guarantee that `buffer` never moves.
    #[borrows(buffer)]
    buffer_assert_pin: &'this [u8],
}

impl AnsiString {
    // pub fn native(&self) -> Lifetime<'_, STRING> {
    //     Lifetime::new(*self.borrow_native())
    // }
}

impl From<&CStr> for AnsiString {
    /// # Complexity
    /// Linear in the length of the string.
    fn from(value: &CStr) -> Self {
        let mut native = STRING::default();
        let buffer = value.to_bytes_with_nul().to_vec();

        // `RtlInitAnsiString` only copies the pointer
        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlinitansistring
        unsafe { RtlInitAnsiString(&mut native, buffer.as_ptr() as *const i8) }
        Self::new(native, buffer, |buf| buf.as_slice())
    }
}
