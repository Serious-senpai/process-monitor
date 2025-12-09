use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() -> Result<(), wdk_build::ConfigError> {
    let windows_listener_driver = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let wfp = windows_listener_driver
        .parent()
        .unwrap()
        .join("windows-listener-wfp");

    let wfp_sln = wfp.join("windows-listener-wfp.sln");

    let arch = match env::var("CARGO_CFG_TARGET_ARCH").unwrap().as_str() {
        "aarch64" => "ARM64",
        "x86_64" => "x64",
        a => panic!("Unsupported architecture: {a}"),
    };
    let wfp_release = wfp.join(arch).join("Release");

    let process = Command::new("msbuild")
        .arg(wfp_sln)
        .arg(format!("/property:Configuration=Release;Platform={arch}"))
        .arg("/target:Clean;Build")
        .status()
        .expect("Failed to build WFP static library");

    if !process.success() {
        panic!("Failed to build WFP static library");
    }

    println!("cargo:rustc-link-search=native={}", wfp_release.display());
    println!("cargo:rustc-link-lib=static=windows-listener-wfp");
    println!("cargo:rustc-link-lib=static=fwpkclnt");

    wdk_build::configure_wdk_binary_build()
}
