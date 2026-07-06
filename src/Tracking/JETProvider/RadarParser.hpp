// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Data.hpp"

#include <vector>

namespace RadarParser {

struct Radar {
  unsigned count = 0;
  unsigned total_count = 0;

  std::vector<JETProvider::Traffic> traffics;
};

/**
 * Parse a JET radar API response body.  The first line is a header
 * ("count,total_count"); each following non-empty line that does not
 * start with '#' describes one traffic.
 *
 * @return true on success
 */
bool
ParseRadarBuffer(const char *buffer, Radar &radar);

} // namespace RadarParser
