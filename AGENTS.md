# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> XCSoar JET - Tactical glide computer for paragliding/soaring (fork of XCSoar with paragliding-specific features)

`CLAUDE.md` is a **symlink to `AGENTS.md`** — edit `AGENTS.md`, and don't replace the symlink with a regular file.

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

After cloning: `git submodule update --init --recursive`

## Testing

Tests live in `test/src/` and are plain executables emitting TAP, aggregated by `test/src/testall.pl`. Three lists in `build/test.mk` control what runs:

- `TEST_NAMES` — everything `make check` runs
- `TESTFAST` / `TESTSLOW` — the harness programs split by runtime

```bash
# Build and run a single test (fastest iteration loop)
make TARGET=UNIX output/UNIX/bin/TestMacCready
./output/UNIX/bin/TestMacCready

# Build all test binaries without running them
make TARGET=UNIX build-check

# Re-run the suite without rebuilding
make TARGET=UNIX check-no-build
```

`make TARGET=UNIX debug` builds the manual/interactive tools (`DEBUG_PROGRAM_NAMES` in `build/test.mk`): `Run*` renderer and parser harnesses, `Feed*` NMEA injectors, `Dump*` inspectors, and device emulators (`FLARMEmulator`, `ATR833Emulator`).

Many tools take a recorded flight and replay it through the real computation path via `CreateDebugReplay()` (`test/src/DebugReplay.hpp`, IGC and NMEA backends). This is the normal way to exercise Engine/Computer changes against real data without hardware.

## Build System

GNU Make with 100+ `.mk` files in `build/`. Key files:
- `build/main.mk` - The application's source lists
- `build/targets.mk` - Platform definitions (UNIX, ANDROID, WIN64, OSX, KOBO, PI, etc.)
- `build/options.mk` - Compile options
- `build/test.mk` - Test and debug-tool definitions
- `build/dirs.mk` - Output layout (`output/<TARGET>/bin`)

**Source lists are explicit — there is no globbing.** A new `.cpp` must be added to the appropriate `*_SOURCES` list in `build/main.mk` (and to the relevant program in `build/test.mk` if a test links it) or it will silently never be compiled.

Useful make variables: `DEBUG=y`, `V=2` (verbose), `USE_CCACHE=y`, `CLANG=y`

## Architecture

**Engine** (`src/Engine/`) - Pure calculation layer with no UI dependencies. Contains glide solvers (MacCready theory), task management, airspace detection, contest optimization (FAI triangle), terrain-aware routing (A* pathfinding), and flight trace management.

**Computer** (`src/Computer/`) - Bridges raw sensor data (`NMEAInfo`) into derived calculations (`DerivedInfo`). Runs the Engine and produces computed values for the UI.

**UI Layer** - Three main components:
- `MapWindow/` - Central map display with layered rendering
- `InfoBoxes/` - Customizable data displays (altitude, speed, thermals, etc.)
- `Dialogs/` - Modal dialogs for configuration and interaction

**Device Drivers** (`src/Device/Driver/`) - 30+ hardware drivers parsing proprietary protocols (varios, FLARM, GPS, radios). Each parses NMEA-like sentences into `NMEAInfo`.

### Threads and blackboards

This is the load-bearing design of the app and the source of most of its rules. Sensor data flows one way through four long-lived thread stages, each with its own *blackboard* — a snapshot struct the thread copies under lock once, then reads without locking:

```
Devices (one thread per port) → MergeThread → CalculationThread → UI thread → DrawThread
     NMEAInfo                     MoreData       DerivedInfo         (redraw)
```

- **Device threads** parse NMEA into `NMEAInfo` and publish to `DeviceBlackboard` (`src/Blackboard/DeviceBlackboard.hpp`).
- **`MergeThread`** (`src/MergeThread.hpp`) merges sources and runs cheap calculations (`BasicComputer`, `FlarmComputer`), producing `MoreData`.
- **`CalculationThread`** (`src/CalculationThread.hpp`) runs `GlideComputer` — the expensive work (task, route, contest, airspace) — producing `DerivedInfo`.
- **UI thread** is the main thread and the *only* thread allowed to touch windows. It reads sensor data via `CommonInterface::Basic()` / `Calculated()` from `Interface.hpp`.
- **DrawThread** renders `MapWindow` from its own blackboard copy.

Consequences worth knowing before writing code:

- Non-UI threads must not include `Interface.hpp` — `InterfaceBlackboard` is unprotected and exists solely for the UI thread.
- Rarely-modified shared data (waypoints, airspace) carries no lock; modifying it requires suspending all threads.
- Objects too expensive to copy use the `Guard` / `Protected*` wrappers instead (`ProtectedTaskManager`, `ProtectedAirspaceWarningManager`).
- The backend object graph is owned and wired up by `BackendComponents` (`src/BackendComponents.hpp`).
- On Android the UI thread is not the process main thread (that one is Java); Bluetooth I/O runs on Java threads via `BluetoothHelper.java`.

Full details: `doc/architecture.rst`.

## Code Conventions

- Style: LLVM-based (`.clang-format`), 79-col limit, 2-space indent, no tabs
- Naming: `CamelCase` for classes/functions, `lower_case` for variables, `ALL_CAPS` for constants/enums
- Headers: `.hpp`, Sources: `.cpp`
- Dialog files: `dlg*.cpp` pattern
- Debug logging: `LogFormat()` from `LogFile.hpp`
- Return type on own line for top-level function definitions
- Commit subjects use a component tag: `[Skysight] fix use-after-free crash on teardown`, `[JETProvider] Update minimum interval to 15s`, `[Android] support immersive mode`. Lowercase module prefixes (`io/async:`, `build:`) also appear for upstream-style changes.

## Key Extension Patterns

Each of these also needs its new source files added to `build/main.mk`.

**New InfoBox:** Create class inheriting `InfoBoxContent` in `src/InfoBoxes/Content/`, register in `Factory.cpp`, add enum to `InfoBoxSettings.hpp`

**New Dialog:** XML layout in `Data/Dialogs/`, implementation in `src/Dialogs/dlg*.cpp`, declare in `Dialogs.h`

**New Device Driver:** Add `src/Device/Driver/<Name>.cpp` (or a directory for multi-file drivers), inherit `AbstractDevice`, and add the `DeviceRegister` entry to `driver_list[]` in `src/Device/Register.cpp`

**New Widget:** Inherit `Widget` base class, implement `Prepare()`, `Show()`, `Hide()`

## Key Types

| Type | Header | Purpose |
|------|--------|---------|
| `NMEAInfo` | `src/NMEA/Info.hpp` | Raw GPS/sensor data |
| `MoreData` | `src/NMEA/MoreData.hpp` | `NMEAInfo` plus cheap merged calculations |
| `DerivedInfo` | `src/NMEA/Derived.hpp` | Computed flight values |
| `InfoBoxData` | `src/InfoBoxes/Data.hpp` | InfoBox display data |
| `MapWindow` | `src/MapWindow/MapWindow.hpp` | Main map widget |
| `TaskManager` | `src/Engine/Task/TaskManager.hpp` | Task management |
| `BackendComponents` | `src/BackendComponents.hpp` | Owns devices, computers, threads |

## JET Fork Code

The paragliding-specific additions that distinguish this fork from upstream XCSoar:

- `src/Tracking/JETProvider/` - Live traffic and flight-trace overlay for followed pilots (HTTP polling via curl coroutines; `RadarParser`, `TraceParser`). Settings in `src/Profile/JETProviderProfile.cpp` and `src/Dialogs/Settings/Panels/JETProviderConfigPanel.cpp`
- `src/Weather/Skysight/` - Skysight.io weather layer integration (`SkysightAPI`, `APIQueue`, `CDFDecoder`, GeoTIFF rendering); UI in `src/Dialogs/Weather/SkysightDialog.cpp`
- Also: PPG fuel burn, enhanced cross-section view, artificial horizon, vario-coloured traffic and trails

Version is tracked in `VERSION.txt` (`<upstream>_JET_<fork>`, e.g. `7.43_JET_1.52`).

## Important Notes

- **Do not run build or test verification commands** — the environment may not have the required toolchain, NDK, SDK, or dependencies available. Avoid running `make`, compilers, or test runners unless explicitly asked.

## See Also

- `.claude/CODEBASE.md` - Full codebase overview with all modules
- `.claude/DEV_QUICKSTART.md` - Dev patterns, code examples, and debugging tips
- `doc/architecture.rst` - Upstream architecture reference (threads, locking, sensor access)
- `doc/build.rst` - Full build instructions per platform
