#!/usr/bin/env bash
# Builds gogx from this folder.
#
#   ./build.sh          a release build for this machine, in target/release/gogx
#   ./build.sh --mac    on a Mac: the Intel slice for macOS 10.13 and the Apple
#                       Silicon slice for 11.0, joined into one universal
#                       binary and signed ad hoc, in target/gogx
#
# Both first run prepare.py (innospect with patches/). The source paths of
# gogx and of every crate are rewritten to short names, so the binary holds
# no home folder (the release scrub refuses one).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
python3 prepare.py
export RUSTFLAGS="--remap-path-prefix=${CARGO_HOME:-$HOME/.cargo}=cargo --remap-path-prefix=$PWD=gogx"
if [ "${1:-}" != "--mac" ]; then
    cargo build --release --locked
    exit 0
fi
MACOSX_DEPLOYMENT_TARGET=10.13 cargo build --release --locked --target x86_64-apple-darwin
MACOSX_DEPLOYMENT_TARGET=11.0 cargo build --release --locked --target aarch64-apple-darwin
lipo -create -output target/gogx \
    target/x86_64-apple-darwin/release/gogx target/aarch64-apple-darwin/release/gogx
# lipo's output carries no signature, and macOS kills an unsigned arm64 binary.
codesign --force -s - target/gogx
