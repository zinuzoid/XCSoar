// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Glue.hpp"
#include "Handler.hpp"
#include "RadarParser.hpp"
#include "NMEA/Info.hpp"
#include "lib/curl/Global.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/CoRequest.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "co/Task.hxx"

#include <stdexcept>

namespace JETProvider {

// static constexpr char API_ENDPOINT_URL[] = "http://192.168.42.113:3000/api/2/radar";
static constexpr char API_ENDPOINT_URL[] = "http://xcsoar.imjim.im/api/2/radar";

Glue::Glue(CurlGlobal &_curl, Handler *_handler)
  :curl(_curl),
   handler(_handler),
   inject_task(curl.GetEventLoop())
{
  settings.SetDefaults();
  unauthorized_access_token.clear();
}

void
Glue::SetSettings(const JETProviderSettings &_settings)
{
  const JETProviderSettings::Radar &radar = _settings.radar;

  if (radar.enabled != settings.enabled ||
      radar.access_token != settings.access_token) {
    /* wait for the current request to finish */
    inject_task.Cancel();

    {
      const std::lock_guard lock{mutex};
      unauthorized_access_token.clear();
    }

    /* discard traffic obtained with the old configuration */
    handler->OnJETProviderReset();
  }

  settings = radar;
}

void
Glue::OnTimer(const NMEAInfo &basic, const GeoBounds &visible_bounds)
{
  if (!settings.enabled || settings.access_token.length() <= 2)
    return;

  {
    const std::lock_guard lock{mutex};
    if (settings.access_token == unauthorized_access_token)
      /* the server rejected this token; wait for the user to
         configure a different one */
      return;
  }

  if (!visible_bounds.IsValid())
    /* no visible map area to query traffic for */
    return;

  if (!clock.CheckUpdate(std::chrono::seconds(settings.interval)))
    return;

  if (inject_task)
    /* still running, skip this poll */
    return;

  inject_task.Start(CoTick(settings, visible_bounds, basic.clock),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
Glue::CoTick(JETProviderSettings::Radar settings, GeoBounds bounds,
             TimeStamp now)
{
  CurlEasy easy;

  NarrowString<1024> url;
  url.Format("%s?access_token=%s&bounds=%f,%f,%f,%f", API_ENDPOINT_URL,
             easy.Escape(settings.access_token).c_str(),
             bounds.GetNorth().Degrees(),
             bounds.GetSouth().Degrees(),
             bounds.GetWest().Degrees(),
             bounds.GetEast().Degrees());
  easy.SetURL(url);

  Curl::Setup(easy);
  easy.SetTimeout(15);

  const auto response = co_await Curl::CoRequest(curl, std::move(easy));

  if (response.status == 401) {
    /* remember the rejected token and suspend polling until the user
       configures a different one */
    {
      const std::lock_guard lock{mutex};
      unauthorized_access_token = settings.access_token;
    }
    throw std::runtime_error("JETProvider: access token unauthorized");
  }

  if (response.status != 200)
    throw FmtRuntimeError("JETProvider: HTTP status {}", response.status);

  RadarParser::Radar radar;
  if (!RadarParser::ParseRadarBuffer(response.body.c_str(), radar))
    throw std::runtime_error("JETProvider: malformed radar response");

  handler->OnJETTraffic(std::move(radar.traffics), now);
}

void
Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error)
    handler->OnJETProviderError(error);
}

} // namespace JETProvider
