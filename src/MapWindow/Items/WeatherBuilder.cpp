// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Builder.hpp"
#include "MapItem.hpp"
#include "WindStationMapItem.hpp"
#include "List.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "net/client/tim/Thermal.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"

#ifdef HAVE_HTTP
#include "Weather/WindsMobi/Glue.hpp"
#endif

#ifdef HAVE_NOAA
#include "Weather/NOAAStore.hpp"
#endif

#ifdef HAVE_NOAA
void
MapItemListBuilder::AddWeatherStations(NOAAStore &store)
{
  for (auto it = store.begin(), end = store.end(); it != end; ++it) {
    if (list.full())
      break;

    if (it->parsed_metar_available &&
        it->parsed_metar.location_available &&
        location.DistanceS(it->parsed_metar.location) < range)
      list.checked_append(new WeatherStationMapItem(it));
  }
}
#endif

void
MapItemListBuilder::AddWindStations()
{
#ifdef HAVE_HTTP
  if (net_components == nullptr || !net_components->wind_stations)
    return;

  const auto lock = net_components->wind_stations->Lock();

  for (const auto &station : net_components->wind_stations->Get()) {
    if (list.full())
      break;

    /* a station with no current wind data draws no arrow, so it isn't
       tappable on the map either */
    if (!station.wind_available)
      continue;

    if (location.DistanceS(station.location) < range)
      list.append(new WindStationMapItem(station));
  }
#endif
}

void
MapItemListBuilder::AddThermals(const ThermalLocatorInfo &thermals,
                                const MoreData &basic,
                                const DerivedInfo &calculated)
{
  for (const auto &t : thermals.sources) {
    if (list.full())
      break;

    // find height difference
    if (basic.nav_altitude < t.ground_height)
      continue;

    GeoPoint loc = calculated.wind_available
      ? t.CalculateAdjustedLocation(basic.nav_altitude, calculated.wind)
      : t.location;

    if (location.DistanceS(loc) < range)
      list.append(new ThermalMapItem(t));
  }
}

void
MapItemListBuilder::AddThermals(std::span<const TIM::Thermal> thermals) noexcept
{
  for (const auto &i : thermals) {
    if (list.full())
      break;

    if (location.DistanceS(i.location) > range)
      continue;

    ThermalSource source;
    source.location = i.location;
    source.ground_height = 0; // TODO
    source.lift_rate = i.climb_rate;
    // TODO source.time = i.time;

    list.append(new ThermalMapItem(source));
  }
}
