use std::env;
use std::path::PathBuf;

fn main() -> anyhow::Result<()> {
    let linux_listener_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")?);
    let headers_dir = linux_listener_dir
        .join("..")
        .join("..")
        .join("process_monitor")
        .join("generated");

    cbindgen::Builder::new()
        .with_crate(&linux_listener_dir)
        .with_language(cbindgen::Language::Cxx)
        .with_tab_width(4)
        .with_braces(cbindgen::Braces::NextLine)
        .with_cpp_compat(true)
        // .with_parse_deps(true)
        .with_pragma_once(true)
        .with_no_includes()
        .with_include("pch.hpp")
        .with_after_include("#define TASK_COMM_LEN 16")
        .include_item("Threshold")
        .include_item("Metric")
        .include_item("StaticCommandName")
        .include_item("Violation")
        .generate()?
        .write_to_file(headers_dir.join("types.hpp"));

    Ok(())
}
