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
#include "time/PeriodClock.hpp"
#include "Language/Language.hpp"
#include "thread/Mutex.hxx"
#include "lib/curl/Request.hxx"
#include "co/InjectTask.hxx"
#include "RadarParser.hpp"

#include <map>

#define JET_PROVIDER_TRAFFIC_OFFLINE_THRESHOLD_SECS 60

/**
 * API JET XCSOAR provider
 */
namespace JETProvider
{

struct Traffic {
  const char *traffic_id = nullptr;
  const char *display = nullptr;
  const char *code = nullptr;
  uint32_t epoch = 0;
  GeoPoint location;
  int track = -1;
  int altitude = -1;
  double speed = -1;
  double vspeed = -1;
  const char *type = nullptr;
};

struct Data {
  mutable Mutex mutex;

  struct cmp_str {
    bool operator()(char const *a, char const *b) const {
      return std::strcmp(a, b) < 0;
    }
  };

  std::map<const char*, Traffic, cmp_str> traffics;
  Validity validity;
  bool success = false;
};

class Handler {
public:
  virtual void OnJETTraffic(std::vector<JETProvider::Traffic> traffics, Validity validity, bool success) = 0;
  virtual void OnJETProviderError(std::exception_ptr e) = 0;
  virtual void OnJETProviderReset() = 0;
};

class Glue final
{

CurlGlobal &curl;
Handler *const handler;
Co::InjectTask inject_task;
PeriodClock clock;

const char *access_token;

public:
  Glue(CurlGlobal &curl, Handler *_handler);

  void OnTimer(const NMEAInfo &basic, const DerivedInfo &calculated);

protected:
  Co::InvokeTask CoTick(const NMEAInfo &basic) noexcept;

  void OnCompletion(std::exception_ptr error) noexcept;

};

}

#endif
