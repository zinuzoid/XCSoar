# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> XCSoar JET - Tactical glide computer for paragliding/soaring (fork of XCSoar with paragliding-specific features)

## Build Commands

```bash
make TARGET=UNIX -j$(nproc)              # Linux build
make TARGET=UNIX DEBUG=y -j$(nproc)      # Debug build with symbols
make TARGET=UNIX check                   # Run all tests
make TARGET=UNIX testfast                # Run fast tests only
make TARGET=UNIX testslow                # Run slow tests only
make TARGET=UNIX clean                   # Clean build artifacts

# Android (needs NDK + SDK)
make TARGET=ANDROIDAARCH64 -j$(nproc) ANDROID_NDK=~/opt/android-ndk-r27c/ ANDROID_SDK=~/opt/android-sdk-linux/

# macOS
make TARGET=OSX -j$(sysctl -n hw.ncpu)
```

Output binary: `output/<TARGET>/bin/xcsoar`

Individual test binaries are built to `output/<TARGET>/bin/` and can be run directly. The `check` target runs all tests via `test/src/testall.pl`.

After cloning: `git submodule update --init --recursive`

## Architecture

**Engine** (`src/Engine/`) - Pure calculation layer with no UI dependencies. Contains glide solvers (MacCready theory), task management, airspace detection, contest optimization (FAI triangle), terrain-aware routing (A* pathfinding), and flight trace management.

**Computer** (`src/Computer/`) - Bridges raw sensor data (`NMEAInfo`) into derived calculations (`DerivedInfo`). Runs the Engine and produces computed values for the UI.

**UI Layer** - Three main components:
- `MapWindow/` - Central map display with layered rendering
- `InfoBoxes/` - Customizable data displays (altitude, speed, thermals, etc.)
- `Dialogs/` - Modal dialogs for configuration and interaction

**Device Drivers** (`src/Device/Driver/`) - 30+ hardware drivers parsing proprietary protocols (varios, FLARM, GPS, radios). Each parses NMEA-like sentences into `NMEAInfo`.

**Data flow:** Hardware → Device Driver → `NMEAInfo` → Computer → `DerivedInfo` → UI (MapWindow, InfoBoxes, Dialogs)

## Code Conventions

- Style: LLVM-based (`.clang-format`), 79-col limit, 2-space indent, no tabs
- Naming: `CamelCase` for classes/functions, `lower_case` for variables, `ALL_CAPS` for constants/enums
- Headers: `.hpp`, Sources: `.cpp`
- Dialog files: `dlg*.cpp` pattern
- Debug logging: `LogFormat()` from `LogFile.hpp`
- Return type on own line for top-level function definitions

## Key Extension Patterns

**New InfoBox:** Create class inheriting `InfoBoxContent` in `src/InfoBoxes/Content/`, register in `Factory.cpp`, add enum to `InfoBoxSettings.hpp`

**New Dialog:** XML layout in `Data/Dialogs/`, implementation in `src/Dialogs/dlg*.cpp`, declare in `Dialogs.h`

**New Device Driver:** Create in `src/Device/Driver/<Name>/`, inherit `AbstractDevice`, register in `Driver/All.cpp`

**New Widget:** Inherit `Widget` base class, implement `Prepare()`, `Show()`, `Hide()`

## Key Types

| Type | Header | Purpose |
|------|--------|---------|
| `NMEAInfo` | `src/NMEA/Info.hpp` | Raw GPS/sensor data |
| `DerivedInfo` | `src/Computer/DerivedInfo.hpp` | Computed flight values |
| `InfoBoxData` | `src/InfoBoxes/Data.hpp` | InfoBox display data |
| `MapWindow` | `src/MapWindow/MapWindow.hpp` | Main map widget |
| `TaskManager` | `src/Engine/Task/TaskManager.hpp` | Task management |

## Build System

GNU Make with 100+ `.mk` files in `build/`. Key files:
- `build/targets.mk` - Platform definitions (UNIX, ANDROID, WIN64, OSX, KOBO, PI, etc.)
- `build/options.mk` - Compile options
- `build/test.mk` - Test configuration

Useful make variables: `DEBUG=y`, `V=2` (verbose), `USE_CCACHE=y`, `CLANG=y`

## Important Notes

- **Do not run build or test verification commands** — the environment may not have the required toolchain, NDK, SDK, or dependencies available. Avoid running `make`, compilers, or test runners unless explicitly asked.

## See Also

- `.claude/CODEBASE.md` - Full codebase overview with all modules
- `.claude/DEV_QUICKSTART.md` - Dev patterns, code examples, and debugging tips
