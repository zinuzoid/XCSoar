// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Glue.hpp"
#include "Parser.hpp"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoStreamRequest.hxx"
#include "json/ParserOutputStream.hxx"
#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"
#include "LogFile.hpp"

#include <algorithm>
#include <cstdio>

namespace WindsMobi {

/**
 * Give up on this session rather than hammering a free community
 * service if something keeps going wrong (server outage, a bug that
 * always fails CheckUpdate() early, etc).  At the 5-minute steady
 * poll rate this is more than three weeks of continuous polling.
 */
static constexpr unsigned EMERGENCY_STOP_MAX_REQUESTS = 10000;

Glue::Glue(CurlGlobal &_curl) noexcept
  :curl(_curl),
   inject_task(curl.GetEventLoop())
{
}

Glue::~Glue() noexcept = default;

void
Glue::OnTimer() noexcept
{
  if (emergency_stop || inject_task)
    return;

  /* all viewport reading happens here, on the UI thread; Start() only
     ever receives by-value parameters, so it never touches MapWindow
     or Interface.hpp from the asio thread */
  const GlueMapWindow *map = UIGlobals::GetMap();
  if (map == nullptr)
    return;

  const auto &projection = map->VisibleProjection();
  if (!projection.IsValid())
    return;

  const GeoPoint center = projection.GetGeoScreenCenter();
  const double radius = std::clamp(projection.GetScreenDistanceMeters() / 2,
                                   10000., 200000.);

  /* winds.mobi is a free community service: back off to a slow poll
     once the current query still covers the visible area, and only
     refresh quickly right after a pan/zoom that moved outside it */
  const bool moved = !last_query_center.IsValid() ||
    last_query_center.DistanceS(center) > last_query_radius * 0.4 ||
    radius > last_query_radius * 1.3;

  if (!clock.CheckUpdate(moved ? std::chrono::seconds(30)
                                : std::chrono::minutes(5)))
    return;

  if (total_requests++ > EMERGENCY_STOP_MAX_REQUESTS) {
    LogFormat("WindsMobi::Glue::OnTimer giving up after %u requests this session",
              total_requests);
    emergency_stop = true;
    return;
  }

  last_query_center = center;
  last_query_radius = radius;
  inject_task.Start(Start(center, radius), BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
Glue::Start(GeoPoint center, double radius)
{
  char url[384];
  snprintf(url, sizeof(url),
           "https://winds.mobi/api/2.3/stations/"
           "?near-lat=%f&near-lon=%f&near-distance=%.0f"
           "&last-measure=3600&limit=50&is-highest-duplicates-rating=true"
           "&keys=short&keys=loc&keys=pv-name&keys=alt"
           "&keys=last._id&keys=last.w-dir&keys=last.w-avg&keys=last.w-max",
           center.latitude.Degrees(), center.longitude.Degrees(), radius);

  CurlEasy easy{url};
  Curl::Setup(easy);
  easy.SetTimeout(20);
  easy.SetFailOnError();
  /* let libcurl announce gzip support and decompress transparently */
  easy.SetAcceptEncoding("gzip");

  Json::ParserOutputStream parser;
  co_await Curl::CoStreamRequest(curl, std::move(easy), parser);

  auto new_stations = ParseStations(parser.Finish());
  LogDebug("Downloaded {} wind stations from winds.mobi",
           new_stations.size());

  const auto lock = Lock();
  stations = std::move(new_stations);
}

void
Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error)
    LogError(error, "winds.mobi request failed");
}

} // namespace WindsMobi
