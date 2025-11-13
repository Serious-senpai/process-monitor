use alloc::collections::vec_deque::VecDeque;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::ptr;
use core::sync::atomic::{AtomicUsize, Ordering};

use wdk_sys::HANDLE;
use wdk_sys::ntddk::{MmUnmapViewInSystemSpace, ObfDereferenceObject};

use crate::wrappers::mutex::SpinLock;

#[repr(C)]
pub struct MemoryInitialize {
    pub section: HANDLE,
    pub event: HANDLE,
    pub view_size: u64,
}

#[repr(C)]
pub struct SharedMemory {
    pub read: AtomicUsize,
    pub write: AtomicUsize,
    pub buffer: [u8; 4096],
}

impl SharedMemory {
    pub fn read(&self) -> Vec<u8> {
        let read = self.read.load(Ordering::Acquire);
        let write = self.write.load(Ordering::Acquire);
        let cap = self.buffer.len();

        // Empty when read == write
        if read == write {
            return Vec::new();
        }

        let result = if read < write {
            // Contiguous region
            self.buffer[read..write].to_vec()
        } else {
            // Wrapped region: [read..cap) + [0..write)
            let mut out = Vec::with_capacity((cap - read) + write);
            out.extend_from_slice(&self.buffer[read..cap]);
            out.extend_from_slice(&self.buffer[..write]);
            out
        };

        // Consume everything we saw
        self.read.store(write, Ordering::Release);
        result
    }

    pub fn write(&mut self, data: &[u8]) {
        let mut write = self.write.load(Ordering::Acquire);
        let read = self.read.load(Ordering::Acquire);
        let cap = self.buffer.len();

        for &byte in data {
            // Next position if we write one byte
            let next = (write + 1) % cap;
            // Leave one byte empty to distinguish full vs empty
            if next == read {
                break; // buffer full
            }

            self.buffer[write] = byte;
            write = next;
        }

        self.write.store(write, Ordering::Release);
    }
}

#[repr(C)]
pub struct MemoryMap {
    pub section: HANDLE,
    pub event: HANDLE,
    pub mapped_base: *mut SharedMemory,
    pub view_size: u64,
}

impl MemoryMap {
    pub unsafe fn initialize(
        section: HANDLE,
        event: HANDLE,
        mapped_base: *mut SharedMemory,
        view_size: u64,
    ) -> Self {
        unsafe {
            ptr::write_volatile(
                mapped_base,
                SharedMemory {
                    read: AtomicUsize::new(0),
                    write: AtomicUsize::new(0),
                    buffer: [0; 4096],
                },
            );
        }

        Self {
            section,
            event,
            mapped_base,
            view_size,
        }
    }
}

impl Drop for MemoryMap {
    fn drop(&mut self) {
        unsafe {
            let _ = MmUnmapViewInSystemSpace(self.mapped_base as *mut c_void);
            ObfDereferenceObject(self.event);
            ObfDereferenceObject(self.section);
        }
    }
}

#[repr(C)]
pub struct DeviceState {
    pub queue: VecDeque<u8>,
    pub memmap: Option<MemoryMap>,
}

#[repr(C)]
pub struct DeviceExtension {
    pub inner: SpinLock<DeviceState>,
}
