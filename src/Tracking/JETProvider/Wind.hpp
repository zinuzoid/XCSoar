/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2021 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef JET_PROVIDER_WIND_HPP
#define JET_PROVIDER_WIND_HPP

#include "WindParser.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "Geo/GeoBounds.hpp"
#include "time/PeriodClock.hpp"
#include "time/Stamp.hpp"
#include "thread/Mutex.hxx"
#include "co/InjectTask.hxx"
#include "util/StaticString.hxx"

#include <chrono>
#include <vector>

class CurlGlobal;

#define JET_PROVIDER_WIND_OFFLINE_THRESHOLD_SECS 600

/**
 * The fixed poll interval for the wind overlay.  Not user configurable
 * (the config panel exposes only an enabled toggle); a fixed 60 s is a
 * good default for a station network whose readings only change slowly.
 */
#define JET_PROVIDER_WIND_INTERVAL_SECS 60

/**
 * The shortest interval a pan or zoom may pull the next request
 * forward to, regardless of #JET_PROVIDER_WIND_INTERVAL_SECS.
 */
#define JET_PROVIDER_WIND_MOVED_INTERVAL_SECS 10

namespace JETProvider
{

struct WindData {
  mutable Mutex mutex;

  std::vector<WindStation> stations;
  Validity validity;
  bool success = false;
};

class WindHandler {
public:
  virtual void OnJETWind(std::vector<WindStation> stations,
                         Validity validity, bool success) = 0;
};

class WindGlue final
{
  CurlGlobal &curl;
  WindHandler *const handler;
  Co::InjectTask inject_task;
  PeriodClock clock;

  mutable Mutex mutex;
  StaticString<64> unauthorized_access_token{""};

  /**
   * The viewport the last request asked about, used to detect a pan or
   * zoom worth an early refresh.  UI thread only - CoTick() never sees
   * it.
   */
  GeoBounds last_query_bounds = GeoBounds::Invalid();

public:
  WindGlue(CurlGlobal &curl, WindHandler *_handler);

  void OnTimer(const NMEAInfo &basic, const DerivedInfo &calculated);

protected:
  Co::InvokeTask CoTick(GeoBounds screen_bounds,
                        StaticString<64> access_token,
                        TimeStamp clock) noexcept;

  void OnCompletion(std::exception_ptr error) noexcept;
};

}

#endif
