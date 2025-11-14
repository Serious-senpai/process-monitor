use std::ffi::c_int;

pub struct Epoll {
    pub fd: c_int,
}

impl Drop for Epoll {
    fn drop(&mut self) {
        if self.fd >= 0 {
            unsafe {
                libc::close(self.fd);
            }
        }
    }
}
