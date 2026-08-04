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

#include "JETProvider.hpp"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoRequest.hxx"
#include "co/Task.hxx"
#include "co/InjectTask.hxx"
#include "Projection/WindowProjection.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"
#include "time/BrokenDateTime.hpp"
#include "util/Exception.hxx"
#include "util/StaticString.hxx"
#include "util/StringFormat.hpp"

// #define API_ENTPOINT_URL "http://192.168.42.113:3000/api/2/radar"
#define API_ENTPOINT_URL "http://xcsoar.imjim.im/api/2/radar"

class EventLoop;

static Co::Task<Curl::CoResponse>
CoGet(CurlGlobal &curl, const char *url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);
  easy.SetTimeout(15);
  /* let libcurl announce gzip support and decompress transparently */
  easy.SetAcceptEncoding("gzip");

  co_return co_await Curl::CoRequest(curl, std::move(easy));
}

/**
 * Hand a status message to the handler, stamped with the local time it
 * was observed.
 */
static void
ReportStatus(JETProvider::Handler &handler, const char *text) noexcept
{
  const BrokenDateTime now = BrokenDateTime::NowLocal();

  StaticString<128> status;
  status.Format("%s (%02u:%02u:%02u)", text,
                unsigned(now.hour), unsigned(now.minute),
                unsigned(now.second));

  handler.OnJETProviderStatus(status);
}

JETProvider::Glue::Glue(CurlGlobal &_curl, Handler *_handler)
  :curl(_curl),
   handler(_handler),
   inject_task(curl.GetEventLoop()) {};

void
JETProvider::Glue::OnTimer(const NMEAInfo &basic, [[maybe_unused]] const DerivedInfo &calculated) {
  if (is_emergency_stop)
    return;

  /* snapshot settings by value on the UI thread; the coroutine must
     never touch CommonInterface or UIGlobals from the curl thread
     (see JETProvider::WindGlue::OnTimer for the same pattern) */
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;
  const StaticString<64> access_token = settings.radar.access_token;
  if (!settings.radar.enabled || access_token.length() <= 2)
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

  if (!clock.CheckUpdate(std::chrono::seconds(settings.radar.interval)))
    return;

  if (inject_task)
    return;

  if (total_requests++ > JET_PROVIDER_EMERGENCY_STOP_MAX_REQUESTS) {
    LogFormat("JETProvider::Glue::OnTimer We're doing more than %d requests "
      "for the session, stop all JETProvider future request!",
      JET_PROVIDER_EMERGENCY_STOP_MAX_REQUESTS);
    is_emergency_stop = true;
    ReportStatus(*handler, "Stopped: request limit reached");
    return;
  }

  inject_task.Start(CoTick(screen_bounds, access_token, basic.clock),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
JETProvider::Glue::CoTick(GeoBounds screen_bounds,
                          StaticString<64> access_token,
                          TimeStamp clock) noexcept
{
  char url[256];
  StringFormat(url, 256, "%s?access_token=%s&bounds=%f,%f,%f,%f", API_ENTPOINT_URL,
    access_token.c_str(),
    screen_bounds.GetNorth().Degrees(),
    screen_bounds.GetSouth().Degrees(),
    screen_bounds.GetWest().Degrees(),
    screen_bounds.GetEast().Degrees()
    );

  auto response = co_await CoGet(curl, url);

  if(response.status == 401) {
    // Unauthorized
    {
      const std::lock_guard lock{mutex};
      unauthorized_access_token = access_token;
    }
    LogFormat("Found unauthorized_access_token: %s, stop all JETProvider "
      "future request!",
      access_token.c_str());
    ReportStatus(*handler, "HTTP 401 Unauthorized");
    handler->OnJETTraffic(std::vector<JETProvider::Traffic>(), Validity{}, false, clock);
    co_return;
  }

  if (response.status != 200) {
    StaticString<64> text;
    text.Format("HTTP %u", response.status);
    ReportStatus(*handler, text);
    handler->OnJETTraffic(std::vector<JETProvider::Traffic>(), Validity{}, false, clock);
    co_return;
  }

  RadarParser::Radar radar;
  if (RadarParser::ParseRadarBuffer(clock, response.body.c_str(), radar)) {
    StaticString<64> text;
    text.Format("OK, %u traffic", radar.count);
    ReportStatus(*handler, text);
    handler->OnJETTraffic(radar.traffics, radar.validity, true, clock);
  } else {
    ReportStatus(*handler, "Invalid response");
    handler->OnJETTraffic(std::vector<JETProvider::Traffic>(), radar.validity, false, clock);
  }
}

void
JETProvider::Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error) {
    LogError(error, "JETProvider error");
    ReportStatus(*handler, GetFullMessage(error).c_str());
    handler->OnJETTraffic(std::vector<JETProvider::Traffic>(), Validity{}, false, TimeStamp::Undefined());
  }
}
