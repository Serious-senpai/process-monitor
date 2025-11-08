#!/bin/bash

# Install stuff
sudo apt-get update
sudo apt-get install -y build-essential cmake curl doxygen git graphviz libssl-dev openssh-server pkg-config python3 python-is-python3

# Start OpenSSH server
sudo systemctl enable ssh
sudo systemctl start ssh

# Install Rust toolchain
curl --proto '=https' --tlsv1.2 https://sh.rustup.rs -sSf | sh -s -- --default-toolchain=1.91 -y
$HOME/.cargo/bin/rustup toolchain install nightly
$HOME/.cargo/bin/rustup component add rust-src
$HOME/.cargo/bin/rustup component add rust-src --toolchain nightly-x86_64-unknown-linux-gnu
$HOME/.cargo/bin/cargo install bpf-linker cargo-generate
