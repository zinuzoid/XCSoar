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

#ifndef JET_PROVIDER_WIND_PARSER_HPP
#define JET_PROVIDER_WIND_PARSER_HPP

#include "Geo/GeoPoint.hpp"
#include "Geo/SpeedVector.hpp"
#include "NMEA/Validity.hpp"
#include "time/Stamp.hpp"

#include <chrono>
#include <string>
#include <vector>

/**
 * A measurement older than this is drawn in the desaturated "stale"
 * colours.  Defined here rather than in Wind.hpp so the map overlay and
 * the map item list - which only ever see #WindStation, never the glue -
 * cannot drift apart.
 */
#define JET_PROVIDER_WIND_STALE_THRESHOLD_SECS 1200

namespace JETProvider {

/**
 * One weather station and its last measurement, as returned by the
 * /api/2/wind endpoint.
 *
 * This is a self-contained value (no pointers into #WindData), because
 * the station vector is replaced wholesale on every poll while copies of
 * this struct live on inside map items and the details dialog.
 */
struct WindStation {
  /** "{provider}-{provider id}", e.g. "holfuy-1175" */
  std::string id;

  /** the station's short name, e.g. "Long Mynd" */
  std::string name;

  /** the data provider's name, e.g. "holfuy.com" */
  std::string provider;

  GeoPoint location = GeoPoint::Invalid();

  /** station altitude [m]; negative if unknown */
  int altitude = -1;

  /** time of the measurement */
  std::chrono::system_clock::time_point measured_at{};

  /** wind bearing = the direction the wind blows FROM, norm in [m/s] */
  SpeedVector wind = SpeedVector::Zero();

  /** wind gust [m/s] */
  double wind_max = 0;
};

}

namespace WindParser {

struct Wind {
  /** rows in this response */
  unsigned count = 0;

  /** rows the server had before it capped the response */
  unsigned total_count = 0;

  Validity validity;

  std::vector<JETProvider::WindStation> stations;
};

/**
 * Parse an /api/2/wind response body.
 *
 * Unlike RadarParser, a single malformed row is logged and skipped
 * rather than failing the parse: one bad station must not discard the
 * eighty good ones alongside it, and the overlay only refreshes once a
 * minute.  Only a malformed header - which means the body is not a wind
 * response at all - fails outright.
 *
 * @return false if the header could not be parsed
 */
bool ParseWindBuffer(TimeStamp clock, const char *buffer, Wind &wind);

}

#endif
