// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Parser.hpp"
#include "Station.hpp"
#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"

#include <boost/json.hpp>

#include <cstdint>
#include <stdexcept>

namespace WindsMobi {

/**
 * winds.mobi wind speeds are reported in km/h; XCSoar's internal unit
 * is m/s.
 */
static constexpr double
KmhToMs(double kmh) noexcept
{
  return kmh / 3.6;
}

/**
 * Parse the "loc" GeoJSON point.  Its "coordinates" array is
 * [longitude, latitude], the opposite order of the (latitude,
 * longitude) most humans expect - getting this backwards silently
 * teleports every station to the wrong hemisphere.
 */
static GeoPoint
ParseLocation(const boost::json::object &json)
{
  const auto &coordinates = json.at("loc").at("coordinates").as_array();
  GeoPoint location{
    Angle::Degrees(coordinates.at(0).to_number<double>()),
    Angle::Degrees(coordinates.at(1).to_number<double>()),
  };
  if (!location.Check())
    throw std::runtime_error("Invalid location");

  return location;
}

/**
 * Parse the "last" measurement sub-object.  Throws if "last" itself
 * or its wind fields are missing; the caller treats that as "no wind
 * data available" rather than discarding the whole station, since
 * name/location/altitude may still be worth showing.
 */
static void
ParseLastMeasure(const boost::json::object &json, Station &station)
{
  const auto &last = json.at("last").as_object();

  const auto epoch = last.at("_id").to_number<int64_t>();
  station.measured_at = std::chrono::system_clock::from_time_t(epoch);

  const auto bearing = last.at("w-dir").to_number<double>();
  const auto avg_kmh = last.at("w-avg").to_number<double>();
  station.wind = SpeedVector(Angle::Degrees(bearing), KmhToMs(avg_kmh));

  if (const auto *max = last.if_contains("w-max"))
    station.wind_max = KmhToMs(max->to_number<double>());

  station.wind_available = true;
}

static Station
ParseStation(const boost::json::object &json)
{
  Station station;

  station.id = json.at("_id").as_string().c_str();

  if (const auto *name = json.if_contains("short"))
    station.name = name->as_string().c_str();

  /* credit winds.mobi alongside the original data provider, since it
     is a free community service aggregating everyone else's stations */
  if (const auto *provider = json.if_contains("pv-name"))
    station.provider.Format("%s via winds.mobi", provider->as_string().c_str());

  if (const auto *alt = json.if_contains("alt"))
    station.altitude = alt->to_number<int>();

  /* required: without a location there is nothing to draw */
  station.location = ParseLocation(json);

  if (json.if_contains("last")) {
    try {
      ParseLastMeasure(json, station);
    } catch (...) {
      /* malformed or incomplete "last" sub-object: keep the station,
        just without wind data */
    }
  }

  return station;
}

std::vector<Station>
ParseStations(const boost::json::value &root)
{
  std::vector<Station> stations;

  const auto &array = root.as_array();
  stations.reserve(array.size());

  for (const auto &item : array) {
    try {
      stations.emplace_back(ParseStation(item.as_object()));
    } catch (...) {
      /* one malformed station (missing id/location) must not discard
        the rest of the response */
    }
  }

  return stations;
}

} // namespace WindsMobi
