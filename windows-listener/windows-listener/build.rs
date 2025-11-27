use std::env;
use std::path::PathBuf;

fn main() -> anyhow::Result<()> {
    let windows_listener_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")?);
    let headers_dir = windows_listener_dir
        .join("..")
        .join("..")
        .join("process-monitor")
        .join("generated");

    cbindgen_base::default(&windows_listener_dir)
        .with_include("types.hpp")
        .generate()?
        .write_to_file(headers_dir.join("listener.hpp"));

    Ok(())
}
