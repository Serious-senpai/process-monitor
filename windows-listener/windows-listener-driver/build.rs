use std::env;
use std::path::PathBuf;

fn main() -> Result<(), wdk_build::ConfigError> {
    let windows_listener_driver = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let windows_listener_wfp_x64_release = windows_listener_driver
        .parent()
        .unwrap()
        .join("windows-listener-wfp")
        .join("x64")
        .join("Release");

    println!(
        "cargo:rustc-link-search=native={}",
        windows_listener_wfp_x64_release.display()
    );
    println!("cargo:rustc-link-lib=static=windows-listener-wfp");

    let static_library = windows_listener_wfp_x64_release.join("windows-listener-wfp.lib");
    println!("cargo::rerun-if-changed={}", static_library.display());

    println!("cargo:rustc-link-lib=static=fwpkclnt");

    wdk_build::configure_wdk_binary_build()
}
