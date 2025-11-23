use core::cell::UnsafeCell;
#[cfg(feature = "win32-user")]
use core::cmp;
use core::sync::atomic::{AtomicUsize, Ordering};
use core::{hint, ptr};

pub struct Channel<const N: usize> {
    _read: AtomicUsize,
    _write_commit: AtomicUsize,
    _write_reserve: AtomicUsize,
    _buffer: UnsafeCell<[u8; N]>,
}

impl<const N: usize> Channel<N> {
    const _SIZE_MASK: usize = N - 1;

    pub const fn new() -> Channel<N> {
        assert!(N > 0);
        assert!(N.is_power_of_two());

        Self {
            _read: AtomicUsize::new(0),
            _write_commit: AtomicUsize::new(0),
            _write_reserve: AtomicUsize::new(0),
            _buffer: UnsafeCell::new([0; N]),
        }
    }

    /// # Safety
    /// There must be at most one reader at a time.
    #[cfg(feature = "win32-user")]
    pub unsafe fn read(&self, buffer: &mut [u8]) -> usize {
        let write_commit = self._write_commit.load(Ordering::Acquire);
        let read = self._read.load(Ordering::Relaxed);

        // Avoid violating the aliasing rules
        // let our_buffer = unsafe { &*self._buffer.get() };
        let base_ptr = self._buffer.get() as *const u8;

        let length = if write_commit < read {
            let length = cmp::min(buffer.len(), N - read + write_commit);
            unsafe {
                if read + length < N {
                    // buffer[..length].copy_from_slice(&our_buffer[read..read + length]);
                    ptr::copy_nonoverlapping(base_ptr.add(read), buffer.as_mut_ptr(), length);
                } else {
                    let tail = N - read;
                    // buffer[..tail].copy_from_slice(&our_buffer[read..]);
                    ptr::copy_nonoverlapping(base_ptr.add(read), buffer.as_mut_ptr(), tail);
                    // buffer[tail..length].copy_from_slice(&our_buffer[..length - tail]);
                    ptr::copy_nonoverlapping(
                        base_ptr,
                        buffer.as_mut_ptr().add(tail),
                        length - tail,
                    );
                }
            }

            length
        } else {
            let length = cmp::min(buffer.len(), write_commit - read);
            // buffer[..length].copy_from_slice(&our_buffer[read..read + length]);
            unsafe {
                ptr::copy_nonoverlapping(base_ptr.add(read), buffer.as_mut_ptr(), length);
            }
            length
        };

        self._read
            .store((read + length) & Self::_SIZE_MASK, Ordering::Release);
        length
    }

    pub fn write(&self, buffer: &[u8]) -> Result<(), usize> {
        // Avoid violating the aliasing rules
        // let our_buffer = unsafe { &mut *self._buffer.get() };
        let base_ptr = self._buffer.get() as *mut u8;

        loop {
            let write_reserve = self._write_reserve.load(Ordering::Acquire);
            let read = self._read.load(Ordering::Acquire);
            let available = if write_reserve < read {
                read - write_reserve - 1
            } else {
                N + read - write_reserve - 1
            };

            if available < buffer.len() {
                return Err(available);
            }

            let new_write_reserve = (write_reserve + buffer.len()) & Self::_SIZE_MASK;
            if self
                ._write_reserve
                .compare_exchange(
                    write_reserve,
                    new_write_reserve,
                    Ordering::AcqRel,
                    Ordering::Acquire,
                )
                .is_ok()
            {
                unsafe {
                    if write_reserve < read {
                        // No wrap because write_reserve < read < N
                        // our_buffer[write_reserve..new_write_reserve].copy_from_slice(buffer);
                        ptr::copy_nonoverlapping(
                            buffer.as_ptr(),
                            base_ptr.add(write_reserve),
                            buffer.len(),
                        );
                    } else {
                        let tail = N - write_reserve;
                        if buffer.len() > tail {
                            // Wrap around
                            // our_buffer[write_reserve..].copy_from_slice(&buffer[..tail]);
                            ptr::copy_nonoverlapping(
                                buffer.as_ptr(),
                                base_ptr.add(write_reserve),
                                tail,
                            );
                            // our_buffer[..new_write_reserve].copy_from_slice(&buffer[tail..]);
                            ptr::copy_nonoverlapping(
                                buffer.as_ptr().add(tail),
                                base_ptr,
                                new_write_reserve,
                            );
                        } else {
                            // No wrap
                            // our_buffer[write_reserve..new_write_reserve].copy_from_slice(buffer);
                            ptr::copy_nonoverlapping(
                                buffer.as_ptr(),
                                base_ptr.add(write_reserve),
                                buffer.len(),
                            );
                        }
                    }
                }

                loop {
                    // Wait for our turn to commit
                    let current_commit = self._write_commit.load(Ordering::Acquire);
                    if current_commit == write_reserve {
                        self._write_commit
                            .store(new_write_reserve, Ordering::Release);

                        return Ok(());
                    }

                    hint::spin_loop();
                }
            }
        }
    }
}

pub type DefaultChannel = Channel<4096>;
