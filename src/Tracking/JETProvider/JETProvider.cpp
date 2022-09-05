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
#include "RadarParser.hpp"
#include "Projection/WindowProjection.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"

// #define API_ENTPOINT_URL "http://192.168.42.113:3000/api/1/radar"
#define API_ENTPOINT_URL "http://xcsoar.imjim.im/api/1/radar"

class EventLoop;

static Co::Task<Curl::CoResponse>
CoGet(CurlGlobal &curl, const char *url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);
  easy.SetFailOnError();

  co_return co_await Curl::CoRequest(curl, std::move(easy));
}

JETProvider::Glue::Glue(CurlGlobal &_curl, Handler *_handler)
  :curl(_curl),
   handler(_handler),
   inject_task(curl.GetEventLoop()) {};

void
JETProvider::Glue::OnTimer(const NMEAInfo &basic, __attribute__((unused)) const DerivedInfo &calculated) {
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;
  access_token = settings.radar.access_token;
  if (!settings.radar.enabled || strlen(access_token) <= 5) {
    return;
  }

  if (!clock.CheckUpdate(std::chrono::seconds(settings.radar.interval)))
    return;

  if (inject_task)
    return;

  inject_task.Start(CoTick(basic), BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
JETProvider::Glue::CoTick(const NMEAInfo &basic) noexcept
{
  const GlueMapWindow *map = UIGlobals::GetMap();
  if (map == nullptr)
    co_return;
  MapWindowProjection projection = map->VisibleProjection();
  if (!projection.IsValid()) {
    co_return;
  }
  GeoBounds screen_bounds = projection.GetScreenBounds();

  char url[256];
  StringFormat(url, 256, "%s?access_token=%s&bounds=%f,%f,%f,%f", API_ENTPOINT_URL,
    access_token,
    screen_bounds.GetNorth().Degrees(),
    screen_bounds.GetSouth().Degrees(),
    screen_bounds.GetWest().Degrees(),
    screen_bounds.GetEast().Degrees()
    );

  auto response = co_await CoGet(curl, url);
  
  RadarParser::Radar radar;
  if (!RadarParser::ParseRadarBuffer(basic, response.body.c_str(), radar) || !radar.validity.IsValid()) {
    handler->OnJETTraffic(std::vector<JETProvider::Data::Traffic>(), false);
  } else {
    handler->OnJETTraffic(radar.traffics, true);
  }
}

void
JETProvider::Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error) {
    LogError(error, "JETProvider error");
    handler->OnJETTraffic(std::vector<JETProvider::Data::Traffic>(), false);
  }
}
