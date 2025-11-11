# Reference: https://docs.docker.com/reference/dockerfile/
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    curl \
    doxygen \
    git \
    graphviz \
    libssl-dev \
    pkg-config \
    python3 \
    python-is-python3 \
    && curl --proto '=https' --tlsv1.2 https://sh.rustup.rs -sSf | sh -s -- --default-toolchain=1.91 -y \
    && rustup component add rust-src \
    && cargo install bpf-linker cargo-generate
