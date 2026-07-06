# XCSoar JET Codebase Overview

> A tactical glide computer for paragliding and soaring pilots - JET fork with paragliding-specific features

**Version:** See `VERSION.txt`
**License:** GPL-2.0-or-later
**Language:** C++ (primary), Java (Android), Python, Lua

---

## Quick Reference

| Item | Details |
|------|---------|
| Build System | GNU Make (`make TARGET=<platform>`) |
| Main Source | `src/` (~3,000 C++ files) |
| Android Wrapper | `android/` (Java) |
| Tests | `test/src/` |
| Docs | `doc/` (Sphinx/RST) |
| Build Config | `build/*.mk` (100+ makefiles) |

---

## Directory Structure

```
.
├── src/                    # Main C++ source
│   ├── Engine/             # Core flight calculations
│   ├── Device/             # Hardware drivers (~30 types)
│   ├── MapWindow/          # Map rendering
│   ├── Renderer/           # Graphics (OpenGL, software)
│   ├── Dialogs/            # UI dialogs (40+)
│   ├── InfoBoxes/          # Customizable info displays
│   ├── Widget/             # UI components
│   ├── FLARM/              # Collision avoidance
│   ├── NMEA/               # GPS/sensor parsing
│   ├── Android/            # Android JNI bindings
│   ├── Terrain/            # Terrain data
│   ├── Topography/         # Map topology
│   ├── Waypoint/           # Waypoint DB
│   ├── Task/               # Task engine
│   ├── Weather/            # Weather integration
│   ├── Tracking/           # Flight tracking
│   ├── Profile/            # User settings
│   ├── Logger/             # IGC flight logging
│   ├── net/                # Networking
│   ├── io/                 # I/O & async ops
│   ├── thread/             # Threading primitives
│   └── event/              # Event handling
├── build/                  # Build system (100+ .mk files)
├── android/                # Android app (Java)
├── lib/                    # Third-party libs (submodules)
├── test/                   # Test infrastructure
├── doc/                    # Documentation
├── tools/                  # Build/utility scripts
├── python/                 # Python bindings
├── Data/                   # Resources (graphics, sounds)
└── debian/                 # Debian packaging
```

---

## Key Modules

### Engine (`src/Engine/`)
Core flight calculation engine:
- **Airspace** - Airspace warnings, boundary detection
- **Task** - Task declarations, optimization
- **Route** - Terrain-aware routing, A* pathfinding
- **GlideSolvers** - MacCready theory, glide calcs
- **Contest** - Competition optimization (FAI triangle)
- **Navigation** - Waypoint routing
- **Trace** - Flight trace management

### Device Drivers (`src/Device/`)
30+ hardware drivers:
- Varios (multiple brands)
- FLARM units
- GPS receivers
- Radios (ATR833, etc.)
- Custom sensors

### UI System
- **MapWindow/** - Central map with layers
- **Renderer/** - Charts, aircraft, terrain viz
- **InfoBoxes/** - 20+ info box types
- **Widget/** - Form framework, dialogs
- **Dialogs/** - 40+ specialized dialogs

### Platform-Specific
- **Android/** - JNI, Java bridge
- **Apple/** - CoreGraphics, AppKit
- **unix/** - POSIX threading, I/O

---

## Build System

### Supported Targets

| Target | Platform |
|--------|----------|
| `UNIX` | Linux (x86/x64) |
| `ANDROID` | Android (ARMv7) |
| `ANDROID7` | Android (API 24+) |
| `ANDROIDAARCH64` | Android (ARM64) |
| `WIN64` | Windows 64-bit |
| `OSX` | macOS |
| `PI` / `PI2` | Raspberry Pi |
| `KOBO` | Kobo e-readers |

### Build Commands

```bash
# Linux
make TARGET=UNIX

# Android
make TARGET=ANDROID

# Windows (cross-compile)
make TARGET=WIN64

# Debug build
make TARGET=UNIX DEBUG=y

# Clean
make TARGET=UNIX clean
```

### Key Build Files
- `Makefile` - Entry point
- `build/targets.mk` - Platform defs
- `build/options.mk` - Compile options
- `build/test.mk` - Test config

---

## Testing

Test files in `test/src/`:
- Unit tests
- Integration tests
- Benchmark tests
- Device emulators
- NMEA injection tools

```bash
make TARGET=UNIX check
```

---

## Platform Support

### Full Support
- Android (API 23+, SDK 35)
- Linux (x86, x64, ARM, ARM64)
- Windows (32/64-bit)
- macOS (Intel + Apple Silicon)

### Specialized
- Raspberry Pi
- Kobo e-readers
- OpenVario

### Graphics APIs
- OpenGL / OpenGL ES 2.0
- SDL2
- Software rendering
- Wayland, X11, KMS

---

## Config Files

| File | Purpose |
|------|---------|
| `.clang-format` | Code style (LLVM) |
| `.gitlab-ci.yml` | GitLab CI |
| `.github/workflows/*.yml` | GitHub Actions |
| `android/AndroidManifest.xml` | Android config |
| `doc/conf.py` | Sphinx docs |
| `debian/control` | Debian pkg |

---

## Third-Party Libraries

Located in `lib/` (git submodules):
- Boost
- GLM (math)
- Lua
- curl
- LibTIFF
- netcdf
- And more...

---

## Key Features

- Glide computer calculations
- Task planning & optimization
- Airspace warnings
- FLARM integration (collision avoidance)
- Waypoint navigation
- Thermal analysis
- Wind calculations
- IGC flight logging
- Cross-section views
- Barograph analysis
- Customizable InfoBox system

---

## JET Fork Additions

Paragliding-specific features:
- PPG fuel burn calculations
- Skysight.io integration (`src/Weather/Skysight/`: coroutine protocol
  layer + single decoder thread + UI-thread glue object; see
  `Skysight.hpp` for the threading contract)
- Enhanced cross-section view
- UI improvements (artificial horizon)
- Modern Android SDK support

---

## Stats

- ~3,000 C++ files
- 30+ device drivers
- 40+ dialogs
- 20+ InfoBox types
- 30+ language translations
- 15+ third-party libs
