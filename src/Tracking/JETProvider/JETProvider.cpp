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
#include "net/http/ToBuffer.hpp"
#include "Job/InlineJobRunner.hpp"
#include "RadarParser.hpp"
#include "Projection/WindowProjection.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"

// #define API_ENTPOINT_URL "http://192.168.42.113:3000/api/1/radar"
#define API_ENTPOINT_URL "http://xcsoar.imjim.im/api/1/radar"

JETProvider::Glue::Glue(CurlGlobal &_curl, Handler *_handler)
  :StandbyThread("JETProvider"),
  curl(_curl),
  handler(_handler) {};

void
JETProvider::Glue::StopAsync()
{
  std::lock_guard<Mutex> lock(mutex);
  StandbyThread::StopAsync();
}

void
JETProvider::Glue::WaitStopped()
{
  std::lock_guard<Mutex> lock(mutex);
  StandbyThread::WaitStopped();
}

void JETProvider::Glue::OnTimer(const NMEAInfo &basic, const DerivedInfo &calculated) {
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;
  access_token = settings.radar.access_token;
  if (!settings.radar.enabled || strlen(access_token) <= 5) {
    return;
  }

  if (!clock.CheckUpdate(std::chrono::seconds(settings.radar.interval)))
    return;

  if (IsBusy())
    return;


  Trigger();
}

void
JETProvider::Glue::Tick() noexcept
{
  const GlueMapWindow *map = UIGlobals::GetMap();
  if (map == nullptr)
    return;
  MapWindowProjection projection = map->VisibleProjection();
  if (!projection.IsValid()) {
    return;
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
  InlineJobRunner runner;
  char buffer[4096];
  buffer[0] = 0;
  try {
    Net::DownloadToBufferJob job(curl, url, buffer, sizeof(buffer) - 1);
    runner.Run(job);
    buffer[job.GetLength()] = 0;
  } catch(...) {
    handler->OnJETTraffic(std::vector<JETProvider::Data::Traffic>(), false);
    handler->OnJETProviderError(std::current_exception());
  }

  RadarParser::Radar radar;
  if (!RadarParser::ParseRadarBuffer(buffer, radar) || !radar.validity.IsValid()) {
    handler->OnJETTraffic(std::vector<JETProvider::Data::Traffic>(), false);
  } else {
    handler->OnJETTraffic(radar.traffics, true);
  }

}
