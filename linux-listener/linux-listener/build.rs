use std::env;
use std::path::PathBuf;

use anyhow::{anyhow, Context};
use aya_build::Toolchain;

fn build_ebpf() -> anyhow::Result<()> {
    let cargo_metadata::Metadata { packages, .. } = cargo_metadata::MetadataCommand::new()
        .no_deps()
        .exec()
        .context("MetadataCommand::exec")?;

    let cargo_metadata::Package {
        name,
        manifest_path,
        ..
    } = packages
        .into_iter()
        .find(|cargo_metadata::Package { name, .. }| name.as_str() == "linux-listener-ebpf")
        .ok_or_else(|| anyhow!("linux-listener-ebpf package not found"))?;

    let ebpf_package = aya_build::Package {
        name: name.as_str(),
        root_dir: manifest_path
            .parent()
            .ok_or_else(|| anyhow!("no parent for {manifest_path}"))?
            .as_str(),
        ..Default::default()
    };

    aya_build::build_ebpf([ebpf_package], Toolchain::default())
}

fn generate_c_header() -> anyhow::Result<()> {
    let linux_listener_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")?);
    let headers_dir = linux_listener_dir
        .join("..")
        .join("..")
        .join("process-monitor")
        .join("include")
        .join("generated");

    cbindgen_base::default(&linux_listener_dir)
        .with_include("types.hpp")
        .generate()?
        .write_to_file(headers_dir.join("listener.hpp"));

    Ok(())
}

fn main() -> anyhow::Result<()> {
    build_ebpf()?;
    generate_c_header()?;

    Ok(())
}
