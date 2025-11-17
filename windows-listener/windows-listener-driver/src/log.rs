#[macro_export]
macro_rules! log {
    () => {
        wdk::println!("[WindowsListener]");
    };
    ($($arg:tt)*) => {
        wdk::println!("[WindowsListener] {}", format_args!($($arg)*));
    };
}
