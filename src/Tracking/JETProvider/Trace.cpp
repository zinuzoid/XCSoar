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

#include "Trace.hpp"
#include "TraceParser.hpp"
#include "Settings.hpp"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoRequest.hxx"
#include "co/Task.hxx"
#include "Interface.hpp"
#include "util/StringFormat.hpp"
#include "LogFile.hpp"

#include <cstring>

// #define TRACE_API_ENTPOINT_URL "http://192.168.42.113:3000/api/2/trace"
#define TRACE_API_ENTPOINT_URL "http://xcsoar.imjim.im/api/2/trace"

static Co::Task<Curl::CoResponse>
CoGet(CurlGlobal &curl, const char *url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);

  co_return co_await Curl::CoRequest(curl, std::move(easy));
}

JETProvider::TraceGlue::TraceGlue(CurlGlobal &_curl, TraceHandler *_handler)
  :curl(_curl),
   handler(_handler),
   inject_task(curl.GetEventLoop()) {};

void
JETProvider::TraceGlue::OnTimer(const NMEAInfo &basic,
                                [[maybe_unused]] const DerivedInfo &calculated)
{
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;

  if (!settings.trace.enabled || settings.trace.pilot_ids.empty() ||
      settings.radar.access_token.length() <= 2 ||
      strcmp(settings.radar.access_token.c_str(),
             unauthorized_access_token) == 0)
  {
    return;
  }

  if (!clock.CheckUpdate(std::chrono::seconds(settings.trace.interval)))
    return;

  if (inject_task)
    return;

  inject_task.Start(CoTick(basic, settings.radar.access_token,
                           settings.trace.src, settings.trace.pilot_ids),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
JETProvider::TraceGlue::CoTick(const NMEAInfo &basic,
                               StaticString<64> access_token,
                               StaticString<16> src,
                               StaticString<256> pilot_ids) noexcept
{
  /* remember the clock now; #basic must not be touched after co_await */
  const auto clock_value = basic.clock;

  std::map<std::string, PilotTrace> traces;

  /* the endpoint takes every followed pilot in one request */
  const std::string ids = JoinPilotIds(pilot_ids.c_str());
  if (ids.empty())
    co_return;

  char url[512];
  StringFormat(url, sizeof(url), "%s?access_token=%s&src=%s&id=%s",
               TRACE_API_ENTPOINT_URL, access_token.c_str(), src.c_str(),
               ids.c_str());

  const auto response = co_await CoGet(curl, url);

  if (response.status == 401) {
    /* the token is bad - stop asking, exactly like the radar does */
    strcpy(unauthorized_access_token, access_token.c_str());
    LogFormat("Found unauthorized_access_token: %s, stop all JETProvider "
              "future trace request!",
              access_token.c_str());
    handler->OnJETTrace(std::move(traces), Validity{}, false);
    co_return;
  }

  if (response.status != 200) {
    LogFormat("JETProvider trace returned status %u", response.status);
    handler->OnJETTrace(std::move(traces), Validity{}, false);
    co_return;
  }

  const bool success = ParseTraceResponse(response.body, traces);
  if (!success)
    LogFormat("JETProvider trace: malformed response, got %d pilot(s)",
              (int)traces.size());

  Validity validity;
  validity.Update(clock_value);

  handler->OnJETTrace(std::move(traces), validity, success);
}

void
JETProvider::TraceGlue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error) {
    LogError(error, "JETProvider trace error");
    handler->OnJETTrace(std::map<std::string, PilotTrace>(), Validity{}, false);
  }
}
