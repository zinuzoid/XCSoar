# Dev Quickstart

Quick reference for working on XCSoar JET.

---

## Common Tasks

### Build & Run

```bash
# Quick Linux build
make TARGET=UNIX -j$(nproc)

# Android build (needs NDK)
make TARGET=ANDROID

# Debug with symbols
make TARGET=UNIX DEBUG=y

# Run tests
make TARGET=UNIX check

# Clean everything
make TARGET=UNIX clean
```

### Finding Code

```bash
# InfoBox implementations
src/InfoBoxes/Content/

# Dialog implementations
src/Dialogs/

# Device drivers
src/Device/Driver/

# Engine calculations
src/Engine/

# Android JNI
src/Android/
android/src/
```

---

## Code Patterns

### InfoBox Pattern
InfoBoxes are in `src/InfoBoxes/Content/`. Each inherits from `InfoBoxContent` base.

```cpp
// Example: src/InfoBoxes/Content/Altitude.cpp
class InfoBoxContentAltitude : public InfoBoxContent {
  void Update(InfoBoxData &data) override;
  bool HandleKey(const InfoBoxKeyCodes key) override;
};
```

### Dialog Pattern
Dialogs in `src/Dialogs/`. Usually use XML layout + C++ logic.

```cpp
// Pattern: src/Dialogs/dlg*.cpp
void dlgSomethingShowModal();
```

### Device Driver Pattern
Drivers in `src/Device/Driver/`. Inherit from `AbstractDevice`.

```cpp
// Example structure
class FooDevice : public AbstractDevice {
  bool ParseNMEA(const char *line, NMEAInfo &info) override;
  void OnSensorUpdate(const MoreData &basic) override;
};
```

### Widget Pattern
Widgets in `src/Widget/`. Base class is `Widget`.

```cpp
class MyWidget : public Widget {
  void Prepare(ContainerWindow &parent, const PixelRect &rc) override;
  void Show(const PixelRect &rc) override;
  void Hide() override;
};
```

---

## Key Types

| Type | Location | Purpose |
|------|----------|---------|
| `NMEAInfo` | `src/NMEA/Info.hpp` | GPS/sensor data |
| `DerivedInfo` | `src/Computer/DerivedInfo.hpp` | Calculated values |
| `TaskManager` | `src/Engine/Task/TaskManager.hpp` | Task mgmt |
| `MapWindow` | `src/MapWindow/MapWindow.hpp` | Main map |
| `InfoBoxData` | `src/InfoBoxes/Data.hpp` | InfoBox display |
| `Profile` | `src/Profile/Profile.hpp` | User settings |
| `Waypoint` | `src/Engine/Waypoint/Waypoint.hpp` | Waypoint data |

---

## Adding Features

### New InfoBox
1. Create class in `src/InfoBoxes/Content/`
2. Register in `src/InfoBoxes/Content/Factory.cpp`
3. Add to `src/InfoBoxes/InfoBoxSettings.hpp`

### New Dialog
1. Create layout in `Data/Dialogs/`
2. Implement in `src/Dialogs/dlgYourDialog.cpp`
3. Add declaration to `src/Dialogs/Dialogs.h`

### New Device Driver
1. Create in `src/Device/Driver/YourDevice/`
2. Register in `src/Device/Driver/All.cpp`
3. Add to `src/Device/Descriptor.cpp`

---

## File Naming Conventions

- `.cpp` / `.hpp` - C++ source/headers
- `dlg*.cpp` - Dialog implementations
- `*Panel.cpp` - Panel widgets
- `*Widget.cpp` - Widget classes
- `*Renderer.cpp` - Drawing code

---

## Debugging

### Enable verbose logging
```cpp
#include "LogFile.hpp"
LogFormat("Debug: value=%d", value);
```

### Android logcat
```bash
adb logcat | grep XCSoar
```

### GDB (Linux)
```bash
make TARGET=UNIX DEBUG=y
gdb ./output/UNIX/bin/xcsoar
```

---

## Git Workflow

```bash
# Main branch
git checkout master

# Create feature branch
git checkout -b feature/my-feature

# Submodule update (for libs)
git submodule update --init --recursive
```

---

## CI/CD

- GitHub Actions: `.github/workflows/`
- GitLab CI: `.gitlab-ci.yml`
- ReadTheDocs: `.readthedocs.yaml`

Auto builds on push for:
- Linux (UNIX)
- Android (multiple ABIs)
- Windows (cross-compile)
- Docs

---

## Resources

- Docs: `doc/` (Sphinx RST)
- Data files: `Data/`
- Test data: `test/`
- Build config: `build/*.mk`
