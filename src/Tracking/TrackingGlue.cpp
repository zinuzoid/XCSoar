// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TrackingGlue.hpp"
#include "Tracking/TrackingSettings.hpp"
#include "NMEA/MoreData.hpp"
#include "LogFile.hpp"
#include "util/Macros.hpp"
#include "time/Stamp.hpp"

TrackingGlue::TrackingGlue(EventLoop &event_loop,
                           CurlGlobal &curl) noexcept
  :skylines(event_loop, this),
   livetrack24(curl),
   jet_provider(curl, this),
   jet_trace(curl, this)
{
}

void
TrackingGlue::SetSettings(const TrackingSettings &_settings)
{
  skylines.SetSettings(_settings.skylines);
  livetrack24.SetSettings(_settings.livetrack24);
}

void
TrackingGlue::OnTimer(const MoreData &basic, const DerivedInfo &calculated)
{
  try {
    skylines.Tick(basic, calculated);
  } catch (...) {
    LogError(std::current_exception(), "SkyLines error");
  }

  jet_provider.OnTimer(basic, calculated);
  jet_provider_data.validity.Expire(basic.clock, std::chrono::seconds(JET_PROVIDER_TRAFFIC_OFFLINE_THRESHOLD_SECS));

  jet_trace.OnTimer(basic, calculated);
  jet_trace_data.validity.Expire(basic.clock, std::chrono::seconds(JET_PROVIDER_TRACE_OFFLINE_THRESHOLD_SECS));

  livetrack24.OnTimer(basic, calculated);
}

void
TrackingGlue::OnTraffic(uint32_t pilot_id, unsigned time_of_day_ms,
                        const GeoPoint &location, int altitude)
{
  bool user_known;

  {
    const std::lock_guard lock{skylines_data.mutex};
    const SkyLinesTracking::Data::Traffic traffic(SkyLinesTracking::Data::Time{time_of_day_ms},
                                                  location, altitude);
    skylines_data.traffic[pilot_id] = traffic;

    user_known = skylines_data.IsUserKnown(pilot_id);
  }

  if (!user_known)
    /* we don't know this user's name yet - try to find it out by
       asking the server */
    skylines.RequestUserName(pilot_id);
}

void TrackingGlue::OnJETTraffic(std::vector<JETProvider::Traffic> traffics, Validity validity, bool success, TimeStamp now)
{
  const std::lock_guard<Mutex> lock(jet_provider_data.mutex);

  jet_provider_data.validity = validity;
  jet_provider_data.success = success;
  if (success) {
    jet_provider_data.traffics.clear();
    for (JETProvider::Traffic traffic : traffics) {
      ClimbAverageCalculator &calc =
        climb_avg_map[std::string(traffic.traffic_id)];
      traffic.climb_rate_avg30s =
        calc.GetAverage(now, traffic.altitude, std::chrono::seconds{30});
      jet_provider_data.traffics[traffic.traffic_id] = traffic;
    }

    // Prune stale calculators for targets not seen in a while
    constexpr FloatDuration MAX_AGE = std::chrono::minutes{1};
    for (auto it = climb_avg_map.begin(); it != climb_avg_map.end();) {
      if (it->second.Expired(now, MAX_AGE))
        it = climb_avg_map.erase(it);
      else
        ++it;
    }
  }

  LogFormat("OnJETTraffic size:%d success:%d",
    (int) jet_provider_data.traffics.size(), success);
}

void
TrackingGlue::OnJETTrace(std::map<std::string, JETProvider::PilotTrace> traces,
                         Validity validity, bool success)
{
  const std::lock_guard<Mutex> lock(jet_trace_data.mutex);

  jet_trace_data.validity = validity;
  jet_trace_data.success = success;

  /* keep the previous traces when a poll failed outright, so a single
     hiccup does not blank the overlay */
  if (!traces.empty() || success)
    jet_trace_data.traces = std::move(traces);

  LogFormat("OnJETTrace pilots:%d success:%d",
    (int) jet_trace_data.traces.size(), success);
}

void TrackingGlue::OnJETProviderReset() {
  if (jet_provider_data.traffics.size() > 0) {
    OnJETTraffic(std::vector<JETProvider::Traffic>(), Validity(), true, TimeStamp::Undefined());
  }
  climb_avg_map.clear();
}

void
TrackingGlue::OnUserName(uint32_t user_id, const TCHAR *name)
{
  const std::lock_guard lock{skylines_data.mutex};
  skylines_data.user_names[user_id] = name;
}

void
TrackingGlue::OnWave(unsigned time_of_day_ms,
                     const GeoPoint &a, const GeoPoint &b)
{
  const std::lock_guard lock{skylines_data.mutex};

  /* garbage collection - hard-coded upper limit */
  auto n = skylines_data.waves.size();
  while (n-- >= 64)
    skylines_data.waves.pop_front();

  // TODO: replace existing item?
  skylines_data.waves.emplace_back(SkyLinesTracking::Data::Time{time_of_day_ms},
                                   a, b);
}

void
TrackingGlue::OnThermal([[maybe_unused]] unsigned time_of_day_ms,
                        const AGeoPoint &bottom, const AGeoPoint &top,
                        double lift)
{
  const std::lock_guard lock{skylines_data.mutex};

  /* garbage collection - hard-coded upper limit */
  auto n = skylines_data.thermals.size();
  while (n-- >= 64)
    skylines_data.thermals.pop_front();

  // TODO: replace existing item?
  skylines_data.thermals.emplace_back(bottom, top, lift);
}

void
TrackingGlue::OnSkyLinesError(std::exception_ptr e)
{
  LogError(e, "SkyLines error");
}

void
TrackingGlue::OnJETProviderError(std::exception_ptr e)
{
  LogError(e, "JETProvider error");
}

void
TrackingGlue::OnJETProviderStatus(const char *status) noexcept
{
  const std::lock_guard<Mutex> lock(jet_provider_data.mutex);

  jet_provider_data.status = status;
}
