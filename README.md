# Ratio

A macOS menu-bar tracker for time spent creating and consuming, plus an interactive desktop demo.

## Download

Download `Ratio-macOS-AppleSilicon.zip` from Releases, unzip, and move Ratio.app to Applications.

This prototype requires an Apple Silicon Mac running macOS 13 or later. It is ad-hoc signed, not Apple-notarized, so macOS may block opening a downloaded copy. Developer ID signing and notarization remain necessary for a frictionless public release. You can also build from source.

## How it works

- Counts foreground app time; background apps are not counted simultaneously.
- The green up arrow means creating; the red down arrow means consuming.
- Reclassifying an app moves its recorded time for the current day and remembers the category.
- Unknown activity counts toward tracked time but stays outside the ratio until categorized.
- Tracking pauses after 60 seconds without input, during sleep, or when manually paused. Reading without input also triggers that idle threshold.
- Supported browsers use macOS Automation to inspect the active tab URL every three seconds. Only the hostname is retained. Without permission, tracking falls back to the browser app.
- Totals and preferences stay locally on your Mac. No account or cloud synchronization.
- Reset clears today's data and custom categories. Daily totals reset at local midnight; historical days are not archived yet.

Website attribution can lag by up to three seconds. Categorization describes the app/site, not your intent within it. The prototype has no auto-update mechanism.

## Build the Mac app

Install Apple's command-line developer tools, then run:

```sh
cd native
./build.sh
open Ratio.app
```

The build uses Swift/AppKit, signs locally, and runs accounting self-tests. Building on Intel produces an Intel executable; the attached prebuilt download is Apple Silicon only.

## Run the demo

Requires Node.js 22.13 or newer.

```sh
cd web
npm ci
npm run dev
```

The demo simulates app activity; it does not record your Mac. Wallpaper selection reuses the vv-store OS approach: select a random square VV visual on the server before rendering, fit it within the desktop, and use a fixed fallback on image failure. The included visual URL pool is a snapshot, rather than a connection to the private vv-store database.

VV artwork is owned by Visualize Value; no additional artwork license is granted here.
