# SnapOCR

English | [简体中文](README.md)

A lightweight **screenshot + annotation + OCR** utility for Windows 10/11.

SnapOCR is built around one idea: fast, frictionless, offline. Press a hotkey to freeze the
screen and drag a region, then draw rectangles/arrows/brush strokes/text/mosaic and extract
text from the image with one click — copy, save and pin all happen from the floating toolbar.
Everything runs locally: no network, no telemetry, no administrator rights needed.

[![Build](https://github.com/poluce/SnapOCR/actions/workflows/build.yml/badge.svg)](https://github.com/poluce/SnapOCR/actions/workflows/build.yml)

## Screenshots

| Main window | Annotation editor + toolbar | OCR result |
|-------------|-----------------------------|------------|
| ![main](docs/assets/phase1-main.png) | ![editor](docs/assets/phase1-editor.png) | ![ocr](docs/assets/phase1-ocr.png) |

## Features

### Capture
- `F1` global hotkey (configurable, conflict-aware), tray menu, `Ctrl+A` full screen
- Frozen frame with dimmed overlay, drag/8-handle resize/arrow-key nudging, size & coordinate HUD
- Window detection (hover to highlight, click to select a window)
- Delayed capture (1–10 s), optional mouse-cursor capture, **pixel magnifier + color picker**
- **Scrolling capture** with automatic overlap stitching
- **Screenshot history** (last 50, reopen/copy/pin/delete/clear)
- Multi-monitor, mixed DPI and negative coordinates; 1:1 physical pixels, no scaling blur

### Annotate
- Rectangle / ellipse / line / arrow / brush / text / step numbers / highlight / mosaic /
  **Gaussian blur brush**
- Select & move, resize handles, z-order (`Ctrl+]` / `Ctrl+[`), filled or dashed shapes
- Undo/redo (≥100 steps), color, line width, font size
- `Ctrl+C` copy (PNG + DIB for IM compatibility), `Ctrl+S` save as PNG/JPG with name templates

### OCR (offline)
- Toolbar "Grab Text" or `F4` screen selection, Windows built-in OCR engine, follows system language
- Result window: editable text, paragraph merging, retry, one-click copy
- Optional auto-copy; helpful hints when a language pack is missing

### System
- **Pinned images**: always on top, drag/zoom/rotate/opacity, grab text (`Ctrl+T`),
  annotate further (`Ctrl+E`)
- **Drag to export**: `Ctrl`+drag a selection in the overlay, `Ctrl`+drag a pinned image, or drag a
  history thumbnail straight into Explorer / WeChat / Word
- Themes (light / dark / follow system), single instance, start with Windows, portable mode
- Live-applied settings with `QSettings` persistence, rotating local logs
- **English UI** (119 strings; set Language to English and restart)
- Crash logs, resource-stability self test, performance self test

## Download

Get the latest build from [Releases](https://github.com/poluce/SnapOCR/releases):

- **Installer**: `SnapOCR-<version>-setup.exe` — installs per user (no admin rights),
  bilingual wizard, optional desktop icon / start with Windows
- **Portable zip**: `SnapOCR-<version>-win64.zip` — unzip anywhere and run `SnapOCR.exe`
  (includes `vc_redist.x64.exe` in case the MSVC runtime is missing)

Run the environment check if something does not work:

```bat
SnapOCR.exe --doctor
```

## Build from source

Requirements: Visual Studio 2022 (C++ workload), Qt 6.11 MSVC 2022 x64, CMake ≥ 3.21, Ninja,
Windows SDK 10.0.26100. `QTDIR` defaults to `E:\Qt6\6.11.1\msvc2022_64` and can be overridden.

```bat
:: Debug build + unit tests (offscreen)
scripts\build.bat

:: Release build
scripts\build.bat Release
```

WSL users can call the Windows toolchain the same way this repo is developed:

```bash
cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat Debug'
```

## Documentation

The project docs are maintained in Chinese (four documents, index in [docs/README.md](docs/README.md)):
requirements, design, development guide and current status.

## License

Private development stage — no open-source license yet. If open-sourced later, MIT is suggested
(third-party licenses are listed in the design document). Qt is dual-licensed under LGPLv3/GPLv3;
distributing a closed-source binary must comply with LGPLv3 (dynamic linking, notices, replacement).
