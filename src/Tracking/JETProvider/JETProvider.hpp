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

#ifndef JET_PROVIDER_HPP
#define JET_PROVIDER_HPP

#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "Geo/GeoBounds.hpp"
#include "time/PeriodClock.hpp"
#include "time/Stamp.hpp"
#include "Language/Language.hpp"
#include "thread/Mutex.hxx"
#include "lib/curl/Request.hxx"
#include "co/InjectTask.hxx"
#include "util/StaticString.hxx"
#include "RadarParser.hpp"

#include <map>
#include <string>

#define JET_PROVIDER_TRAFFIC_OFFLINE_THRESHOLD_SECS 60
#define JET_PROVIDER_EMERGENCY_STOP_MAX_REQUESTS 10000

/**
 * API JET XCSOAR provider
 */
namespace JETProvider
{

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
  mutable Mutex mutex;

  std::map<std::string, Traffic> traffics;
  Validity validity;
  bool success = false;

  /**
   * Human-readable outcome of the most recent radar request; empty
   * until the first request has completed.
   */
  StaticString<128> status;
};

class Handler {
public:
  virtual void OnJETTraffic(std::vector<JETProvider::Traffic> traffics, Validity validity, bool success, TimeStamp now) = 0;
  virtual void OnJETProviderError(std::exception_ptr e) = 0;
  virtual void OnJETProviderReset() = 0;

  /**
   * The outcome of a radar request, for display in the configuration
   * panel.
   */
  virtual void OnJETProviderStatus(const char *status) noexcept = 0;
};

class Glue final
{

CurlGlobal &curl;
Handler *const handler;
Co::InjectTask inject_task;
PeriodClock clock;

mutable Mutex mutex;
StaticString<64> unauthorized_access_token{""};
bool is_emergency_stop = false;
unsigned total_requests = 0;

public:
  Glue(CurlGlobal &curl, Handler *_handler);

  void OnTimer(const NMEAInfo &basic, const DerivedInfo &calculated);

protected:
  Co::InvokeTask CoTick(GeoBounds screen_bounds,
                        StaticString<64> access_token,
                        TimeStamp clock) noexcept;

  void OnCompletion(std::exception_ptr error) noexcept;

};

}

#endif
