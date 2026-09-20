# Ratio for Windows

> Create more. Consume less.

Ratio is a native Windows system tray application that measures time spent in the active foreground window or browser website, letting you classify your time into **creating** or **consuming**. It displays your current balance in the system tray and tracks a 30-day local history.

This is a native C++ (Win32 + GDI+) port of Visualize Value's macOS application, retaining 100% feature and design parity with zero external dependencies and a sub-500 KB standalone executable.

---

## Direct Downloads (v0.1.0)

- 🚀 **[Download Ratio.exe (Standalone Executable)](https://github.com/nabduljabbar98/ratio-windows/raw/release/windows/build/Ratio.exe)**
- 📦 **[Download Ratio-v0.1.0-windows-x64.zip](https://github.com/nabduljabbar98/ratio-windows/raw/release/windows/build/Ratio-v0.1.0-windows-x64.zip)**

---

## Features

- **Prominent Pill Taskbar Badge**: Renders an illuminated, high-contrast light blue pill badge (`#E0EEFD`) directly in your taskbar with the live direction arrow (`↑`, `↓`, `?`) and current ratio percentage in bold dark text.
- **Automatic Theme Adaptation**: Seamlessly syncs with Windows Dark Mode and Light Mode in real-time, responding dynamically to Windows system theme changes without restarting.
- **Out-of-the-Box Presets**: Pre-categorized for common tools:
  - *Create*: Paper, Antigravity, VS Code, Cursor, Zed, Visual Studio, Figma, Notion, Obsidian, Office, Slack, Terminal.
  - *Consume*: YouTube, LinkedIn, X/Twitter, Reddit, Instagram, TikTok, Netflix, Twitch, Steam, Discord.
- **Unified Categorize Dashboard**: Direct in-row `[↑]` (Create) and `[↓]` (Consume) buttons with instant retrospective accounting.
- **30-Day History**: Daily create/consume ratio breakdown with proportional split bars.
- **Foreground Tracking**: Records active time exclusively for the frontmost application.
- **Browser Tab Tracking**: Automatically detects active website hostnames from Google Chrome, Microsoft Edge, Brave, and Mozilla Firefox via Windows UI Automation.
- **Strict Privacy**: Only domain hostnames (`youtube.com`, `linkedin.com`, `github.com`) are ever stored. URL paths, queries, search terms, and window titles are never collected or retained.
- **Idle Detection**: Automatically halts accounting after 60 seconds of user inactivity.
- **Session & Power Integration**: Pauses tracking on workstation lock (`Win + L`) or system sleep, and cleanly resumes on unlock/wake.
- **Zero Dependencies**: Pure native Win32/C++ executable with zero external runtime requirements. Sub-500 KB standalone executable.

---

## Requirements

- Windows 10 or Windows 11 (64-bit)
- Visual Studio 2022 (Community, Professional, Enterprise, or BuildTools) with the "Desktop development with C++" workload installed.

---

## Building from Source

### Quick Build (Command Prompt)
```cmd
cd windows
build.bat
```

### Quick Build (PowerShell)
```powershell
cd windows
.\build.ps1
```

The build script compiles:
1. `windows\build\Ratio.exe` — The main standalone GUI system tray application (< 500 KB).
2. `windows\build\RatioTest.exe` — The accounting and rule validation test suite.

---

## Running Ratio

To start Ratio:
```cmd
windows\build\Ratio.exe
```

- **Left-Click Tray Icon**: Opens or closes the Ratio flyout window.
- **Right-Click Tray Icon**: Opens context menu (Check for Updates, Share Anonymous Total, Quit).
- **Clicking Outside**: Dismisses the flyout window automatically.

---

## Verification & Self-Tests

To run the accounting invariant and boundary test suite:
```cmd
windows\build\RatioTest.exe
```
or:
```cmd
windows\build\Ratio.exe --self-test
```

All 5 core accounting test suites from the macOS reference implementation will run and report:
- `PASS: website classification and hostname boundaries`
- `PASS: retrospective categorization and duplicate review protection`
- `PASS: app attribution, legacy migration, daily reset, app persistence`
- `PASS: recategorization moves all app time, neutral exclusion, idempotency and persistence`
- `PASS: classified time, unknown exclusion, suspension gaps, persistence`

---

## Default Classifications

### Built-in Apps
- **Create**:
  - Code editors & IDEs: Visual Studio (`devenv.exe`), VS Code (`Code.exe`), JetBrains IDEs (`idea64.exe`, `pycharm64.exe`, `webstorm64.exe`, `rider64.exe`, `clion64.exe`), Notepad++ (`notepad++.exe`), Sublime Text (`sublime_text.exe`), Windows Terminal (`WindowsTerminal.exe`).
  - Design & Creative: Figma (`Figma.exe`), Adobe Photoshop (`Photoshop.exe`), Adobe Illustrator (`Illustrator.exe`), Adobe Premiere (`Premiere.exe`), Adobe After Effects (`AfterFX.exe`), Blender (`blender.exe`).
  - Writing & Productivity: Microsoft Word (`WINWORD.EXE`), Excel (`EXCEL.EXE`), PowerPoint (`POWERPNT.EXE`), Obsidian (`Obsidian.exe`), Notion (`Notion.exe`).
- **Consume**:
  - Entertainment & Media: Netflix (`Netflix.exe`), Spotify (`Spotify.exe`), Windows Media Player (`Video.UI.exe`), Steam (`Steam.exe`), Discord (`Discord.exe`).

### Built-in Websites
- **Create**: `figma.com`, `docs.google.com`, `canva.com`, `github.com`.
- **Consume**: `x.com`, `twitter.com`, `youtube.com`, `reddit.com`, `instagram.com`, `tiktok.com`, `netflix.com`.

---

## Data Storage & Privacy

All classifications, today's ledger, and 30-day history are stored locally on your machine at:
```
%LOCALAPPDATA%\Ratio\ratio_data.json
```
No personal browsing data, window titles, or app names are sent to any external server.
