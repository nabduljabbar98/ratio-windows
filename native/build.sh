#!/bin/zsh
set -eu
cd "$(dirname "$0")"
mkdir -p Ratio.app/Contents/MacOS Ratio.app/Contents/Frameworks Ratio.app/Contents/Resources
cp vendor/Sparkle/LICENSE Ratio.app/Contents/Resources/Sparkle-LICENSE.txt
ditto vendor/Sparkle/Sparkle.framework Ratio.app/Contents/Frameworks/Sparkle.framework
xcrun swiftc -module-cache-path "${TMPDIR:-/tmp}/ratio-swift-cache" Sources/main.swift -o Ratio.app/Contents/MacOS/Ratio -framework AppKit -framework CoreGraphics -F vendor/Sparkle -framework Sparkle -Xlinker -rpath -Xlinker @executable_path/../Frameworks
codesign --force --deep --sign - Ratio.app
Ratio.app/Contents/MacOS/Ratio --self-test
