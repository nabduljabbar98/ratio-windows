#!/bin/zsh
set -eu
cd "$(dirname "$0")"
mkdir -p vendor/Sparkle
archive=$(mktemp -t ratio-sparkle).tar.xz
trap 'rm -f "$archive"' EXIT
curl -fL https://github.com/sparkle-project/Sparkle/releases/download/2.10.0/Sparkle-2.10.0.tar.xz -o "$archive"
expected=c2bf58aa8387266ac179357b1415d6f2635f044da8be41042af32425dae6da0c
actual=$(shasum -a 256 "$archive" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { print -u2 'Sparkle checksum mismatch'; exit 1; }
tar -xf "$archive" -C vendor/Sparkle
