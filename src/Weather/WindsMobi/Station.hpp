// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Geo/SpeedVector.hpp"
#include "util/StaticString.hxx"

#include <chrono>

namespace WindsMobi {

/**
 * A snapshot of a single winds.mobi weather station and its last
 * measurement.  This is a self-contained value (no pointers into the
 * #Glue's internal state), because the station vector is replaced
 * wholesale on every poll and instances of this struct are copied into
 * map items that must outlive the poll that produced them.
 */
struct Station {
  /** "{pv-code}-{pv-id}", e.g. "holfuy-1636" */
  StaticString<64> id;

  /** the station's short name */
  StaticString<48> name;

  /** the data provider's name, e.g. "holfuy.com via winds.mobi" */
  StaticString<64> provider;

  GeoPoint location;

  /** station altitude [m]; negative if unknown */
  int altitude = -1;

  /** time of the last measurement */
  std::chrono::system_clock::time_point measured_at;

  /** true if #wind and #measured_at are valid */
  bool wind_available = false;

  /** wind bearing = direction the wind blows FROM, norm in [m/s] */
  SpeedVector wind = SpeedVector::Zero();

  /** wind gust [m/s]; negative if unknown */
  double wind_max = -1;
};

} // namespace WindsMobi
