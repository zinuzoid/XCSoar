// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Features.hpp"
#include "util/TriState.hpp"

#ifdef HAVE_SKYLINES_TRACKING

#include <cstdint>

namespace SkyLinesTracking {

struct CloudSettings {
  /**
   * Is submitting data to the (experimental) XCSoar Cloud enabled?
   * Defaults to FALSE (disabled). Users can enable it manually in
   * the "Tracking" settings.
   */
  TriState enabled;

  bool show_thermals;

  uint64_t key;

  void SetDefaults() {
    enabled = TriState::FALSE;
    show_thermals = true;
    key = 0;
  }
};

} /* namespace SkyLinesTracking */

#endif
