use core::cell::UnsafeCell;
#[cfg(feature = "win32-user")]
use core::cmp;
use core::hint;
use core::sync::atomic::{AtomicUsize, Ordering, fence};

pub struct Channel<const N: usize> {
    _read: AtomicUsize,
    _write_commit: AtomicUsize,
    _write_reserve: AtomicUsize,
    _buffer: UnsafeCell<[u8; N]>,
}

impl<const N: usize> Channel<N> {
    #[cfg(feature = "win32-user")]
    pub fn new() -> Result<Self, usize> {
        if N == 0 && N.count_ones() != 1 {
            return Err(N);
        }

        Ok(Self {
            _read: AtomicUsize::new(0),
            _write_commit: AtomicUsize::new(0),
            _write_reserve: AtomicUsize::new(0),
            _buffer: UnsafeCell::new([0; N]),
        })
    }

    #[cfg(feature = "win32-user")]
    pub fn read(&self, buffer: &mut [u8]) -> usize {
        let write_commit = self._write_commit.load(Ordering::Acquire);
        let read = self._read.load(Ordering::Relaxed);
        let our_buffer = unsafe { &*self._buffer.get() };

        let length = if write_commit < read {
            let length = cmp::min(buffer.len(), N - read + write_commit);
            if read + length < N {
                buffer[..length].copy_from_slice(&our_buffer[read..read + length]);
            } else {
                let tail = N - read;
                buffer[..tail].copy_from_slice(&our_buffer[read..]);
                buffer[tail..length].copy_from_slice(&our_buffer[..length - tail]);
            }

            length
        } else {
            let length = cmp::min(buffer.len(), write_commit - read);
            buffer[..length].copy_from_slice(&our_buffer[read..read + length]);
            length
        };

        self._read.store((read + length) & N, Ordering::Release);
        length
    }

    pub fn write(&self, buffer: &[u8]) -> Result<(), usize> {
        let our_buffer = unsafe { &mut *self._buffer.get() };

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

            let new_write_reserve = (write_reserve + buffer.len()) & N;
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
                if write_reserve < read {
                    // No wrap because write_reserve < read < N
                    our_buffer[write_reserve..new_write_reserve].copy_from_slice(buffer);
                } else {
                    let tail = N - write_reserve;
                    if buffer.len() > tail {
                        // Wrap around
                        our_buffer[write_reserve..].copy_from_slice(&buffer[..tail]);
                        our_buffer[..new_write_reserve].copy_from_slice(&buffer[tail..]);
                    } else {
                        // No wrap
                        our_buffer[write_reserve..new_write_reserve].copy_from_slice(buffer);
                    }
                }

                fence(Ordering::Release);

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
