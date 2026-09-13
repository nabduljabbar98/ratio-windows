#!/bin/zsh
set -eu
cd "$(dirname "$0")"
mkdir -p Ratio.app/Contents/MacOS
xcrun swiftc -module-cache-path "${TMPDIR:-/tmp}/ratio-swift-cache" Sources/main.swift -o Ratio.app/Contents/MacOS/Ratio -framework AppKit -framework CoreGraphics
codesign --force --sign - Ratio.app
Ratio.app/Contents/MacOS/Ratio --self-test
