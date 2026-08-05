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

#ifndef JET_PROVIDER_TRACE_HPP
#define JET_PROVIDER_TRACE_HPP

#include "TraceParser.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "time/PeriodClock.hpp"
#include "time/Stamp.hpp"
#include "thread/Mutex.hxx"
#include "co/InjectTask.hxx"
#include "util/StaticString.hxx"

#include <chrono>
#include <map>
#include <string>

class CurlGlobal;

#define JET_PROVIDER_TRACE_OFFLINE_THRESHOLD_SECS 300

namespace JETProvider
{

struct TraceData {
  mutable Mutex mutex;

  std::map<std::string, PilotTrace> traces;
  Validity validity;
  bool success = false;
};

class TraceHandler {
public:
  virtual void OnJETTrace(std::map<std::string, PilotTrace> traces,
                          Validity validity, bool success) = 0;
};

/**
 * Client for the JET trace endpoint.
 *
 * Like JETProvider::Glue, all UI-thread state is read in OnTimer(); the
 * coroutine receives only by-value parameters.
 */
class TraceGlue final
{
  CurlGlobal &curl;
  TraceHandler *const handler;
  Co::InjectTask inject_task;
  PeriodClock clock;

  /**
   * Guards #unauthorized_access_token, which the coroutine writes on
   * the curl thread while OnTimer() reads it on the UI thread.
   */
  mutable Mutex mutex;

  StaticString<64> unauthorized_access_token{""};

public:
  TraceGlue(CurlGlobal &curl, TraceHandler *_handler);

  void OnTimer(const NMEAInfo &basic, const DerivedInfo &calculated);

protected:
  Co::InvokeTask CoTick(TimeStamp clock,
                        StaticString<64> access_token,
                        StaticString<16> src,
                        StaticString<256> pilot_ids,
                        std::chrono::duration<unsigned> timeout) noexcept;

  void OnCompletion(std::exception_ptr error) noexcept;
};

}

#endif
