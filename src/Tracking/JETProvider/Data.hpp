// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "NMEA/Validity.hpp"
#include "thread/Mutex.hxx"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

/**
 * API JET XCSOAR provider
 */
namespace JETProvider {

struct Traffic {
  std::string traffic_id;
  std::string display;
  std::string code;
  uint32_t epoch = 0;
  GeoPoint location;
  int track = -1;
  int altitude = -1;
  double speed = -1;
  double vspeed = -1;
  double climb_rate_avg30s = -1;
  std::string type;
  int icon_type = -1;
};

struct Data {
  /**
   * Traffic older than this is considered offline and expired by
   * TrackingGlue.
   */
  static constexpr std::chrono::seconds OFFLINE_THRESHOLD{60};

  mutable Mutex mutex;

  /**
   * Latest traffic, keyed by Traffic::traffic_id.  Protected by
   * #mutex.
   */
  std::map<std::string, Traffic> traffics;

  /**
   * Updated on every successful poll, cleared on failure.  Protected
   * by #mutex.
   */
  Validity validity;

  /**
   * Was the last poll successful?  While false, the renderer draws
   * the (retained) traffic in "offline" style.  Protected by #mutex.
   */
  bool success = false;
};

} // namespace JETProvider
