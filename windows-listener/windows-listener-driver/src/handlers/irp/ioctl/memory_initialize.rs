use alloc::boxed::Box;
use alloc::format;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::ptr;
use core::sync::atomic::{AtomicU64, Ordering};

use ffi::win32::message::{IOCTL_MEMORY_INITIALIZE, MemoryInitialize};
use ffi::win32::mpsc::DefaultChannel;
use wdk_sys::{
    DEVICE_OBJECT, IO_STACK_LOCATION, IRP, OBJ_KERNEL_HANDLE, OBJECT_ATTRIBUTES, PUNICODE_STRING,
    STATUS_BUFFER_TOO_SMALL, STATUS_INVALID_PARAMETER,
};

use crate::error::RuntimeError;
use crate::handlers::irp::ioctl::IoctlHandler;
use crate::mpsc::SharedMemorySection;
use crate::state::{DRIVER_STATE, SharedMemory};
use crate::wrappers::bindings::InitializeObjectAttributes;
use crate::wrappers::event::Event;
use crate::wrappers::strings::UnicodeString;

pub struct MemoryInitializeHandler<'a> {
    _device: &'a DEVICE_OBJECT,
    _irp: &'a mut IRP,
    _irpsp: &'a mut IO_STACK_LOCATION,
    _output_length: u32,
}

static _ID_COUNTER: AtomicU64 = AtomicU64::new(0);

fn _named_attribute(
    name: &str,
) -> Result<(PUNICODE_STRING, Box<Vec<u16>>, OBJECT_ATTRIBUTES), RuntimeError> {
    let name =
        UnicodeString::try_from(unsafe { CStr::from_bytes_with_nul_unchecked(name.as_bytes()) })?;

    let (native, buffer) = name.into_raw_parts();
    let native = Box::into_raw(Box::new(native));

    let mut attr = OBJECT_ATTRIBUTES::default();
    unsafe {
        InitializeObjectAttributes(
            &mut attr,
            native,
            OBJ_KERNEL_HANDLE,
            ptr::null_mut(),
            ptr::null_mut(),
        )
    };

    Ok((native, buffer, attr))
}

impl<'a> IoctlHandler<'a> for MemoryInitializeHandler<'a> {
    const CODE: u32 = IOCTL_MEMORY_INITIALIZE;

    fn new(
        device: &'a DEVICE_OBJECT,
        irp: &'a mut IRP,
        irpsp: &'a mut IO_STACK_LOCATION,
    ) -> Result<Self, RuntimeError> {
        let output_length = unsafe { irpsp.Parameters.DeviceIoControl.OutputBufferLength };
        Ok(Self {
            _device: device,
            _irp: irp,
            _irpsp: irpsp,
            _output_length: output_length,
        })
    }

    fn handle(&mut self) -> Result<(), RuntimeError> {
        self._irp.IoStatus.Information = size_of::<MemoryInitialize>().try_into()?;
        if self._output_length < size_of::<MemoryInitialize>().try_into()? {
            return Err(RuntimeError::Failure(STATUS_BUFFER_TOO_SMALL));
        }

        let mut section_attr = _named_attribute(&format!(
            "\\BaseNamedObjects\\Global\\ProcessMonitor_Section_{}\0",
            _ID_COUNTER.fetch_add(1, Ordering::AcqRel)
        ))?;
        let mut event_attr = _named_attribute(&format!(
            "\\BaseNamedObjects\\Global\\ProcessMonitor_Event_{}\0",
            _ID_COUNTER.fetch_add(1, Ordering::AcqRel)
        ))?;
        let skip_chars = "\\BaseNamedObjects\\".len();

        let mut message = MemoryInitialize {
            section: [0; 64],
            event: [0; 64],
            size: size_of::<DefaultChannel>(),
        };

        let user_section_name = &section_attr.1[skip_chars..];
        let user_event_name = &event_attr.1[skip_chars..];

        // Do the strings fit inside the buffers (they have already included a null terminator)?
        if user_section_name.len() > message.section.len()
            || user_event_name.len() > message.event.len()
        {
            return Err(RuntimeError::Failure(STATUS_INVALID_PARAMETER));
        }

        message.section[..user_section_name.len()].copy_from_slice(user_section_name);
        message.event[..user_event_name.len()].copy_from_slice(user_event_name);

        let shared_memory = SharedMemory::new(
            SharedMemorySection::new(&mut section_attr.2)?,
            Event::new(&mut event_attr.2)?,
        )?;
        let old = DRIVER_STATE
            .shared_memory
            .swap(Box::into_raw(Box::new(shared_memory)), Ordering::AcqRel);
        if !old.is_null() {
            unsafe {
                drop(Box::from_raw(old));
            }
        }

        unsafe {
            ptr::write(
                self._irp.AssociatedIrp.SystemBuffer as *mut MemoryInitialize,
                message,
            );
            drop(Box::from_raw(section_attr.0));
            drop(Box::from_raw(event_attr.0));
        }

        Ok(())
    }
}
