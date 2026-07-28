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
#include "Settings.hpp"
#include "Task/PolylineDecoder.hpp"
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

static constexpr bool
IsWhitespace(char ch) noexcept
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

/**
 * The decoder rejects every character below '?', so the trailing newline
 * a HTTP body may carry has to go first.
 */
static std::string_view
StripWhitespace(std::string_view src) noexcept
{
  while (!src.empty() && IsWhitespace(src.front()))
    src.remove_prefix(1);
  while (!src.empty() && IsWhitespace(src.back()))
    src.remove_suffix(1);
  return src;
}

/**
 * Pull the next comma separated pilot id out of #remaining, trimming
 * surrounding whitespace.  Returns an empty view once nothing is left.
 */
static std::string_view
NextPilotId(std::string_view &remaining) noexcept
{
  while (!remaining.empty()) {
    std::string_view id;

    if (const auto comma = remaining.find(','); comma == std::string_view::npos) {
      id = remaining;
      remaining = {};
    } else {
      id = remaining.substr(0, comma);
      remaining = remaining.substr(comma + 1);
    }

    while (!id.empty() && id.front() == ' ')
      id.remove_prefix(1);
    while (!id.empty() && id.back() == ' ')
      id.remove_suffix(1);

    if (!id.empty())
      return id;
  }

  return {};
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
  bool success = true;

  std::string_view remaining{pilot_ids.c_str()};

  while (true) {
    const auto id = NextPilotId(remaining);
    if (id.empty())
      break;

    const std::string id_str{id};

    char url[512];
    StringFormat(url, sizeof(url), "%s?access_token=%s&src=%s&id=%s",
                 TRACE_API_ENTPOINT_URL, access_token.c_str(), src.c_str(),
                 id_str.c_str());

    const auto response = co_await CoGet(curl, url);

    if (response.status == 401) {
      /* the token is bad - stop asking, exactly like the radar does */
      strcpy(unauthorized_access_token, access_token.c_str());
      LogFormat("Found unauthorized_access_token: %s, stop all JETProvider "
                "future trace request!",
                access_token.c_str());
      success = false;
      break;
    }

    if (response.status != 200) {
      LogFormat("JETProvider trace for %s returned status %u",
                id_str.c_str(), response.status);
      success = false;
      continue;
    }

    PilotTrace trace;
    trace.id = id_str;

    try {
      /* DecodePolyline() throws; keep it contained so one bad response
         does not discard the other pilots we already fetched */
      trace.points = DecodePolyline(StripWhitespace(response.body));
    } catch (...) {
      LogError(std::current_exception(),
               "JETProvider trace: malformed polyline");
      success = false;
      continue;
    }

    if (!trace.points.empty())
      traces.emplace(id_str, std::move(trace));
  }

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
