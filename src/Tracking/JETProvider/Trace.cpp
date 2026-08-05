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
CoGet(CurlGlobal &curl, const char *url,
      std::chrono::duration<unsigned> timeout)
{
  CurlEasy easy{url};
  Curl::Setup(easy);
  /* let libcurl announce gzip support and decompress transparently */
  easy.SetAcceptEncoding("gzip");

  /* Curl::Setup() only limits how long connecting may take; without a
     limit on the transfer too, a server that stalls mid-body would keep
     the request - and with it every later poll - hanging forever */
  easy.SetTimeout(long(timeout.count()));

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
      settings.radar.access_token.length() <= 2)
  {
    return;
  }

  {
    const std::lock_guard lock{mutex};
    if (settings.radar.access_token == unauthorized_access_token)
      return;
  }

  if (!clock.CheckUpdate(std::chrono::seconds(settings.trace.interval)))
    return;

  if (inject_task)
    return;

  /* time the request out after one interval, so a stalled request costs
     at most the poll it was already occupying */
  inject_task.Start(CoTick(basic.clock, settings.radar.access_token,
                           settings.trace.src, settings.trace.pilot_ids,
                           settings.trace.interval),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
JETProvider::TraceGlue::CoTick(TimeStamp clock,
                               StaticString<64> access_token,
                               StaticString<16> src,
                               StaticString<256> pilot_ids,
                               std::chrono::duration<unsigned> timeout) noexcept
{

  std::map<std::string, PilotTrace> traces;

  /* the endpoint takes every followed pilot in one request */
  const std::string ids = JoinPilotIds(pilot_ids.c_str());
  if (ids.empty())
    co_return;

  char url[512];
  StringFormat(url, sizeof(url), "%s?access_token=%s&src=%s&id=%s",
               TRACE_API_ENTPOINT_URL, access_token.c_str(), src.c_str(),
               ids.c_str());

  const auto response = co_await CoGet(curl, url, timeout);

  if (response.status == 401) {
    /* the token is bad - stop asking, exactly like the radar does */
    {
      const std::lock_guard lock{mutex};
      unauthorized_access_token = access_token;
    }
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
  validity.Update(clock);

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
