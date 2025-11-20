use std::env;
use std::path::PathBuf;

fn main() -> anyhow::Result<()> {
    let linux_listener_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")?);
    let headers_dir = linux_listener_dir
        .join("..")
        .join("..")
        .join("process_monitor")
        .join("generated");

    cbindgen_base::default(&linux_listener_dir)
        .with_include("pch.hpp")
        .include_item("Threshold")
        .include_item("Metric")
        .include_item("StaticCommandName")
        .include_item("Event")
        .with_define("feature", "linux-kernel", "__linux__")
        .generate()?
        .write_to_file(headers_dir.join("types.hpp"));

    Ok(())
}
