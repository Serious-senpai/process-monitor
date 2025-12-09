#!/bin/bash
# Setup script for a new Ubuntu 24.04 virtual machine (it may still work with other versions idk)

# Install stuff
sudo apt-get update
sudo apt-get install -y build-essential clang cmake curl doxygen git graphviz libelf-dev libssl-dev linux-tools-$(uname -r) llvm openssh-server pkg-config python3 python-is-python3

# Start OpenSSH server
sudo systemctl enable ssh
sudo systemctl start ssh

# Install Rust toolchain
curl --proto '=https' --tlsv1.2 https://sh.rustup.rs -sSf | sh -s -- --default-toolchain=1.91 -y
$HOME/.cargo/bin/rustup toolchain install nightly
$HOME/.cargo/bin/rustup component add rust-src
$HOME/.cargo/bin/rustup component add rust-src --toolchain nightly-x86_64-unknown-linux-gnu
$HOME/.cargo/bin/cargo install bindgen-cli bpf-linker cargo-generate
$HOME/.cargo/bin/cargo install --git https://github.com/aya-rs/aya -- aya-tool

# Install bpftool from source
cd /tmp
rm -rf /tmp/bpftool
git clone --recursive https://github.com/libbpf/bpftool

cd /tmp/bpftool
git checkout v7.6.0

cd /tmp/bpftool/src
make
sudo make install
