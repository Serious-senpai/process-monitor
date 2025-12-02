#[macro_export]
macro_rules! log {
    () => {
        wdk::println!("[Windows Listener]");
    };
    ($($arg:tt)*) => {
        wdk::println!("[Windows Listener] {}", format_args!($($arg)*));
    };
}
