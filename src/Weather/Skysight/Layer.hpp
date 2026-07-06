// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>
#include <ctime>
#include <map>
#include <string>

struct SkysightLegendColor {
  uint8_t red, green, blue;
};

/**
 * A weather layer ("metric") offered by the Skysight API for one
 * region.  Plain value type; safe to copy across threads.
 */
struct SkysightLayer {
  std::string id;
  std::string name;
  std::string description;

  /**
   * Colour ramp keyed by data value; values below the first entry
   * are rendered transparent.
   */
  std::map<float, SkysightLegendColor> legend;

  /** UNIX timestamp of the last server-side update; 0 = unknown */
  std::time_t last_update = 0;
};

struct SkysightRegion {
  std::string id;
  std::string name;
};
