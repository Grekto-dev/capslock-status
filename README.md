# CapsLock Status

[![Release](https://img.shields.io/github/v/release/Grekto-dev/capslock-status?color=2EA44F&label=Release)](https://github.com/Grekto-dev/capslock-status/releases)
[![Build & Release](https://github.com/Grekto-dev/capslock-status/actions/workflows/release.yml/badge.svg)](https://github.com/Grekto-dev/capslock-status/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A native, ultra-lightweight Linux port of [CapsLockIndicator](https://github.com/jonaskohl/CapsLockIndicator) by Jonas Kohl, tailored specifically for **Linux Mint**. Written in pure C99 for negligible resource usage (0.00% idle CPU, ~2.9 MB RAM), designed to provide lock key status notifications with maximum responsiveness, even on modest hardware.

---

## 💻 Supported Distribution & Compatibility

- **Officially Supported**: **Linux Mint** (Cinnamon Edition).
- **Disclaimer**: *CapsLock Status is tailored and tested specifically for Linux Mint. While it can be tested on other Linux distributions and desktop environments, it has not been tested on them and there is no guarantee that it will work properly.*

---

## ✨ Features

- **Smooth On-Screen Display (OSD)**:
  - Exact Windows 10/11 visual replica: `#424242` background, active/inactive colored borders (`#4DB434` / `#B4344D`), and crisp TrueType typography rendered via Cairo subpixel anti-aliasing.
  - Faithful fade-out animation matching the original Windows timing: holds 100% opaque for the first half of the duration, then smoothly fades out at 50 fps.
  - Zero input delay: instant appearance on key presses without compositor lag.
- **Native Linux Mint Tray Integration (`XAppStatusIcon`)**:
  - Dedicated **CapsLock Status** application tray icon for quick access to settings and quitting the application.
  - Independent status indicators for **Num Lock** `[N]`, **Caps Lock** `[C]`, and **Scroll Lock** `[S]`.
  - Full support for dragging icons into the collapsible menu (`cinnamon-collapsible-tray`) or rearranging them on the panel.
  - Native tooltips on every icon (`CapsLock Status`, `Num Lock`, `Caps Lock`, `Scroll Lock`).
  - Single-instance settings: clicking any tray icon opens or restores focus to the settings dialog.
  - Dynamic tray management: toggling icons in Settings immediately adds or removes them from the tray.
  - Smart Tray mode: option to only show indicator icons when keys are actively turned on.
- **Modern Settings Interface**:
  - Clean Linux Mint-styled configuration window with dark cards, accent colors, and responsive tabs.
  - Instant live retranslation across English, Portuguese, German, Spanish, and French.
  - Customizable display duration, screen position (9 positions), persistent overlays, and alert sounds.
- **Lightweight Debian Package (`.deb`)**:
  - Fully packaged with vector SVG icons, `.desktop` launcher, and system-wide binaries.

---

## ⚡ Performance Comparison

| Metric | CapsLock Status (C99) | Typical Python / Web / Electron Apps |
| :--- | :--- | :--- |
| **Idle CPU Usage** | **0.00%** (kernel socket blocking) | 0.5% – 2.0% |
| **RAM Footprint (RSS)** | **~2.9 MB** | 45 MB – 180 MB |
| **Stripped Binary Size** | **~50 KB** | 50 MB – 200 MB |
| **Runtime Dependencies** | **`libX11`, `libcairo`** | Heavy runtimes and browser engines |

---

## 📥 Installation

### Option 1: Install Debian Package (`.deb`)

Download the latest `.deb` package from the [Releases page](https://github.com/Grekto-dev/capslock-status/releases):

```bash
# Install the downloaded package
sudo dpkg -i capslock-status_1.0.0_amd64.deb
sudo apt-get install -f
```

### Option 2: Build from Source

#### 1. Install Build Dependencies
On Linux Mint:
```bash
sudo apt-get update
sudo apt-get install -y build-essential libx11-dev libcairo2-dev python3 python3-gi gir1.2-gtk-3.0
```

#### 2. Compile and Install
```bash
git clone https://github.com/Grekto-dev/capslock-status.git
cd capslock-status
make clean && make strip
sudo make install
```

*(Creates system binaries and compatibility aliases for `capslock-indicator`)*.

To build a `.deb` package locally:
```bash
make deb
```

---

## 🚀 Usage

### Start Daemon with Tray Icons
```bash
capslock-status -d -t
```

### Open Settings GUI
```bash
capslock-status -g
# or directly:
capslock-status-gui
```
*(You can also left-click any tray icon or right-click to open the context menu and choose **Settings**)*.

### Test On-Screen Display (OSD)
```bash
capslock-status --test
```

### Enable Autostart on Login
```bash
capslock-status --install-autostart
```

---

## ⌨️ CLI Reference

```text
Usage: capslock-status [OPTIONS]

Main Options:
  -d, --daemon             Run in background (daemon mode)
  -g, --gui                Open the Settings graphical interface
  -t, --tray               Enable system tray icons [N] [C] [S]
  -s, --status             Print current lock key states (Caps/Num/Scroll) and exit
      --json               Format --status output as JSON
      --test               Display test OSD popup and exit
      --toggle KEY         Toggle key state (caps, num, scroll) and exit

OSD Options:
      --no-osd             Disable on-screen display notifications
      --position POS       Position: bottom-right, bottom-center, top-right, center, etc.
      --duration MS        Notification display time in milliseconds (default: 1200)
      --sticky             Keep notification visible while key is active

Autostart & Configuration:
      --install-autostart   Register automatic startup on user login
      --uninstall-autostart Remove automatic startup entry
  -c, --config PATH        Specify custom config.ini path
  -v, --version            Display version information
  -h, --help               Display help message
```

---

## 📄 License & Acknowledgments

This project is licensed under the [MIT License](LICENSE).

Linux port based on the original [CapsLockIndicator](https://github.com/jonaskohl/CapsLockIndicator) created by [Jonas Kohl](https://github.com/jonaskohl).
