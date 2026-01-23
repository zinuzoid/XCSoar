# CLAUDE.md

> XCSoar JET - Tactical glide computer for paragliding/soaring

## Quick Facts

- **Language:** C++ (primary), Java (Android), Lua (scripting)
- **Build:** `make TARGET=<platform>` (UNIX, ANDROID, WIN64, OSX, etc.)
- **Version:** 7.43_JET_1.36
- **License:** GPL-2.0-or-later

## Build Commands

```bash
make TARGET=UNIX -j$(nproc)                                                                                    # Linux
make TARGET=ANDROIDAARCH64 -j$(nproc) ANDROID_NDK=~/opt/android-ndk-r27c/ ANDROID_SDK=~/opt/android-sdk-linux/ # Android Aarch64
make TARGET=UNIX DEBUG=y                                                                                       # Debug build
make TARGET=UNIX check                                                                                         # Run tests
```

## Project Structure

```
src/                 # Main C++ source (~3k files)
├── Engine/          # Flight calculations (glide, task, airspace)
├── Device/          # Hardware drivers (30+ types)
├── MapWindow/       # Map rendering
├── Dialogs/         # UI dialogs (40+)
├── InfoBoxes/       # Customizable info displays
├── NMEA/            # GPS/sensor parsing
├── Android/         # JNI bindings
android/             # Android Java wrapper
build/               # Makefiles (100+ .mk files)
lib/                 # Third-party libs (git submodules)
test/                # Tests
```

## Code Conventions

- Style: LLVM (see `.clang-format`)
- Headers: `.hpp`, Sources: `.cpp`
- Dialogs: `dlg*.cpp` pattern
- Use `LogFormat()` for debug logging

## Key Patterns

**InfoBox:** Inherit `InfoBoxContent`, register in `Factory.cpp`
**Dialog:** XML layout in `Data/Dialogs/`, impl in `src/Dialogs/`
**Device:** Inherit `AbstractDevice`, register in `Driver/All.cpp`
**Widget:** Inherit `Widget` base class

## Important Files

- `src/InfoBoxes/Content/` - InfoBox implementations
- `src/Device/Driver/` - Device drivers
- `src/Engine/` - Core flight engine
- `build/targets.mk` - Platform definitions

## Submodules

Run `git submodule update --init --recursive` after clone.

## See Also

- `.claude/CODEBASE.md` - Full codebase overview
- `.claude/DEV_QUICKSTART.md` - Dev patterns & tips
