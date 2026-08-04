// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "WindBarbLook.hpp"

class Font;

/**
 * Look for the winds.mobi wind station map overlay: one
 * #WindBarbLook per gust-strength band, plus a desaturated set for
 * stale measurements, and the font used for the avg/gust label drawn
 * alongside each barb.
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

  WindBarbLook bands[N_BANDS];

  /** same bands, desaturated, used when the measurement is stale */
  WindBarbLook stale_bands[N_BANDS];

  /** shared by every band: the barb colour carries the gust strength,
      the label font does not need to */
  const Font *font;

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
