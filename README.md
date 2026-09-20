# Ratio

Create more. Consume less.

Ratio is a minimalist menu-bar and taskbar app that measures time in the active app or browser site, then lets you classify that time as creating or consuming. It shows the balance live in your taskbar tray and keeps a local daily history.

## Download for Windows (v0.1.0)

Direct downloads for Windows 10 & 11 (64-bit, zero dependencies):
- 🚀 **[Download Ratio.exe (Standalone Executable)](https://github.com/nabduljabbar98/ratio-windows/raw/release/windows/build/Ratio.exe)**
- 📦 **[Download Ratio-v0.1.0-windows-x64.zip](https://github.com/nabduljabbar98/ratio-windows/raw/release/windows/build/Ratio-v0.1.0-windows-x64.zip)**

*Just download and run `Ratio.exe`. It automatically lives in your taskbar system tray with a prominent status pill, tracks your active apps, and synchronizes with your Windows Dark/Light theme.*

## Get Ratio (macOS)

- **Signed app:** Buy the notarized, automatically updating build for $20 at [ratio.visualizevalue.com](https://ratio.visualizevalue.com/).
- **Build it yourself:** Clone this repository and follow the instructions below.

The paid build funds development and removes the work of compiling, signing, notarizing, and updating the app. The application source is available under GPL-3.0.

## What it records

- Ratio counts time only for the foreground window.
- For supported browsers, it can read the active tab and retains only the hostname.
- App usage, classifications, daily history, and preferences stay in macOS UserDefaults on your Mac.
- Update authentication is stored in Keychain.
- The signed build can share one anonymous cumulative tracked-time total. It never sends app names, site names, window titles, classifications, or daily history. This is enabled by default and can be disabled from **Share Anonymous Total** in the right-click menu. Self-built copies do not report unless they have a valid purchaser update credential.

See [`native/Sources/main.swift`](native/Sources/main.swift) for the complete implementation.

## Requirements

- macOS 12 or newer
- Intel or Apple silicon
- Xcode command-line tools

## Build the Mac app

```sh
git clone https://github.com/visualizevalue/ratio.git
cd ratio/native
./setup-sparkle.sh
./build.sh
open Ratio.app
```

`build.sh` creates a universal Intel/Apple-silicon app, applies an ad-hoc local signature, and runs the accounting self-tests. An ad-hoc build may require right-clicking the app and choosing **Open**. It does not carry Visualize Value's Developer ID signature or Apple notarization.

Automatic updates for the distributed build use Sparkle. Update downloads require a verified Ratio purchase; this does not prevent local builds or modify local tracking data.

## Build the Windows app

```cmd
cd ratio\windows
build.bat
```

Or using PowerShell:
```powershell
cd ratio\windows
.\build.ps1
```

This compiles a standalone, native 64-bit `Ratio.exe` (< 500 KB) with zero external dependencies and runs the accounting self-tests. See [`windows/README.md`](windows/README.md) for full architecture and configuration details.

## Contributing

Issues and focused pull requests are welcome. Please keep the interface compact, preserve local-first tracking, and do not add collection of app names, sites, window titles, or browsing history.

## License

Ratio's application source is licensed under [GNU GPL v3](LICENSE). The Ratio name, icon, and Visualize Value name are trademarks or brand assets and are not granted for use by the GPL software license. Third-party components retain their own licenses.
