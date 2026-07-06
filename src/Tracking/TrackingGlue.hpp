// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Tracking/Features.hpp"

#ifdef HAVE_TRACKING

#include "Tracking/SkyLines/Handler.hpp"
#include "Tracking/SkyLines/Glue.hpp"
#include "Tracking/SkyLines/Data.hpp"
#include "Tracking/LiveTrack24/Glue.hpp"
#include "Tracking/JETProvider/Data.hpp"
#include "Tracking/JETProvider/Handler.hpp"
#include "Tracking/JETProvider/Glue.hpp"
#include "Computer/ClimbAverageCalculator.hpp"
#include "thread/StandbyThread.hpp"
#include "time/PeriodClock.hpp"
#include "Geo/GeoPoint.hpp"
#include "Geo/GeoBounds.hpp"

#include <map>
#include <string>

struct TrackingSettings;
struct MoreData;
struct DerivedInfo;
class CurlGlobal;

class TrackingGlue final
  : private SkyLinesTracking::Handler,
    private JETProvider::Handler
{
  SkyLinesTracking::Glue skylines;

  SkyLinesTracking::Data skylines_data;

  LiveTrack24::Glue livetrack24;

  JETProvider::Glue jet_provider;

  JETProvider::Data jet_provider_data;

  /**
   * 30 second climb average per JET traffic id.  Protected by
   * #jet_provider_data's mutex.
   */
  std::map<std::string, ClimbAverageCalculator> climb_avg_map;

  /**
   * The Unix UTC time stamp that was last submitted to the tracking
   * server.  This attribute is used to detect time warps.
   */
  std::chrono::system_clock::time_point last_timestamp{};

public:
  TrackingGlue(EventLoop &event_loop, CurlGlobal &curl) noexcept;

  void SetSettings(const TrackingSettings &_settings);

  /**
   * @param visible_bounds the geographic area currently visible on
   * the map (queried for JET traffic), or GeoBounds::Invalid() if no
   * map is available; must be obtained on the calling (UI) thread
   */
  void OnTimer(const MoreData &basic, const DerivedInfo &calculated,
               const GeoBounds &visible_bounds);

private:
  /* virtual methods from SkyLinesTracking::Handler */
  virtual void OnTraffic(uint32_t pilot_id, unsigned time_of_day_ms,
                         const GeoPoint &location, int altitude) override;
    virtual void OnUserName(uint32_t user_id, const TCHAR *name) override;
  void OnWave(unsigned time_of_day_ms,
              const GeoPoint &a, const GeoPoint &b) override;
  void OnThermal(unsigned time_of_day_ms,
                 const AGeoPoint &bottom, const AGeoPoint &top,
                 double lift) override;
  void OnSkyLinesError(std::exception_ptr e) override;

  /* virtual methods from JETProvider::Handler */
  void OnJETTraffic(std::vector<JETProvider::Traffic> &&traffics,
                    TimeStamp now) override;
  void OnJETProviderReset() override;
  void OnJETProviderError(std::exception_ptr e) override;

public:
  const SkyLinesTracking::Data &GetSkyLinesData() const {
    return skylines_data;
  }
  const JETProvider::Data &GetJETProviderData() const {
    return jet_provider_data;
  }
};

#endif /* HAVE_TRACKING */
