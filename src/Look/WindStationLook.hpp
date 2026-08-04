// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "WindArrowLook.hpp"

class Font;

/**
 * Look for the winds.mobi wind station map overlay.  Reuses
 * #WindArrowLook (and therefore the existing WindArrowRenderer)
 * unmodified, just with one set of pens/brushes per gust strength
 * band, plus a desaturated set for stale measurements.
 */
struct WindStationLook {
  /** calm, moderate, strong, dangerous - by gust speed */
  static constexpr unsigned N_BANDS = 4;

  /**
   * The boundaries between the four gust bands [m/s]: 15, 25 and
   * 35 km/h.
   */
  static constexpr double THRESHOLDS[N_BANDS - 1] = {
    15. / 3.6,
    25. / 3.6,
    35. / 3.6,
  };

  WindArrowLook bands[N_BANDS];

  /** same bands, desaturated, used when the measurement is stale */
  WindArrowLook stale_bands[N_BANDS];

  void Initialise(const Font &font);

  /**
   * Map a gust speed [m/s] to a band index in [0, N_BANDS).
   */
  [[gnu::const]]
  static unsigned BandIndex(double gust) noexcept {
    unsigned i = 0;
    while (i < N_BANDS - 1 && gust > THRESHOLDS[i])
      ++i;
    return i;
  }
};
