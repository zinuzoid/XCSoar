// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct SkysightRegionDefault {
  const char *id;
  const char *name;
};

/**
 * Fallback region list used until the real one has been fetched
 * from the "/regions" endpoint.  Terminated by a {nullptr, nullptr}
 * entry.
 */
extern const SkysightRegionDefault skysight_region_defaults[];
