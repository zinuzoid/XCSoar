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

#include "Wind.hpp"
#include "WindParser.hpp"
#include "Settings.hpp"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoRequest.hxx"
#include "co/Task.hxx"
#include "Projection/WindowProjection.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"
#include "util/StringFormat.hpp"

#include <algorithm>

// #define WIND_API_ENTPOINT_URL "http://192.168.42.113:3000/api/2/wind"
#define WIND_API_ENTPOINT_URL "http://xcsoar.imjim.im/api/2/wind"

static Co::Task<Curl::CoResponse>
CoGet(CurlGlobal &curl, const char *url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);
  /* let libcurl announce gzip support and decompress transparently */
  easy.SetAcceptEncoding("gzip");

  /* Curl::Setup() only limits how long connecting may take; without a
     limit on the transfer too, a server that stalls mid-body would keep
     the request - and with it every later poll - hanging forever.  A
     fixed 20 s is below the 60 s poll interval, so a stalled request
     can never overlap the next one. */
  easy.SetTimeout(20);

  co_return co_await Curl::CoRequest(curl, std::move(easy));
}

JETProvider::WindGlue::WindGlue(CurlGlobal &_curl, WindHandler *_handler)
  :curl(_curl),
   handler(_handler),
   inject_task(curl.GetEventLoop()) {};

void
JETProvider::WindGlue::OnTimer(const NMEAInfo &basic,
                               [[maybe_unused]] const DerivedInfo &calculated)
{
  /* snapshot settings by value on the UI thread; the coroutine must
     never touch CommonInterface or UIGlobals from the curl thread */
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;
  const StaticString<64> access_token = settings.radar.access_token;

  /* one shared token for every JETProvider endpoint; the wind overlay
     also rides on the radar's enabled flag, see IsWindEnabled() */
  if (!settings.IsWindEnabled() || access_token.length() <= 2)
    return;

  {
    const std::lock_guard lock{mutex};
    if (access_token == unauthorized_access_token)
      return;
  }

  /* all viewport reading happens here, on the UI thread; Start() only
     ever receives by-value parameters, so CoTick never touches
     MapWindow or Interface.hpp from the asio thread */
  const GlueMapWindow *map = UIGlobals::GetMap();
  if (map == nullptr)
    return;

  const MapWindowProjection projection = map->VisibleProjection();
  if (!projection.IsValid())
    return;

  const GeoBounds screen_bounds = projection.GetScreenBounds();

  /* a pan or zoom that carried the middle of the view out of the area
     the last request covered is worth an early refresh; without this,
     panning to a new area shows an empty map for a whole interval */
  const bool moved = !last_query_bounds.IsValid() ||
    !last_query_bounds.IsInside(screen_bounds.Scale(0.5));

  const auto interval = moved
    ? std::chrono::seconds(JET_PROVIDER_WIND_MOVED_INTERVAL_SECS)
    : std::chrono::seconds(JET_PROVIDER_WIND_INTERVAL_SECS);

  if (!clock.CheckUpdate(interval))
    return;

  if (inject_task)
    return;

  last_query_bounds = screen_bounds;

  inject_task.Start(CoTick(screen_bounds, access_token, basic.clock),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
JETProvider::WindGlue::CoTick(GeoBounds screen_bounds,
                              StaticString<64> access_token,
                              TimeStamp clock) noexcept
{
  char url[256];
  StringFormat(url, sizeof(url), "%s?access_token=%s&bounds=%f,%f,%f,%f",
               WIND_API_ENTPOINT_URL,
               access_token.c_str(),
               screen_bounds.GetNorth().Degrees(),
               screen_bounds.GetSouth().Degrees(),
               screen_bounds.GetWest().Degrees(),
               screen_bounds.GetEast().Degrees());

  const auto response = co_await CoGet(curl, url);

  if (response.status == 401) {
    /* the token is bad - stop asking, exactly like the radar does */
    {
      const std::lock_guard lock{mutex};
      unauthorized_access_token = access_token;
    }
    LogFormat("Found unauthorized_access_token: %s, stop all JETProvider "
              "future wind request!",
              access_token.c_str());
    handler->OnJETWind(std::vector<JETProvider::WindStation>(), Validity{}, false);
    co_return;
  }

  if (response.status != 200) {
    LogFormat("JETProvider wind returned status %u", response.status);
    handler->OnJETWind(std::vector<JETProvider::WindStation>(), Validity{}, false);
    co_return;
  }

  WindParser::Wind wind;
  if (!WindParser::ParseWindBuffer(clock, response.body.c_str(), wind)) {
    LogFormat("JETProvider wind: malformed response");
    handler->OnJETWind(std::vector<JETProvider::WindStation>(), Validity{}, false);
    co_return;
  }

  handler->OnJETWind(std::move(wind.stations), wind.validity, true);
}

void
JETProvider::WindGlue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error) {
    LogError(error, "JETProvider wind error");
    handler->OnJETWind(std::vector<JETProvider::WindStation>(), Validity{}, false);
  }
}
