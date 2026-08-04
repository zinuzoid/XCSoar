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

#include "WindParser.hpp"
#include "Units/System.hpp"
#include "LogFile.hpp"

#include <boost/algorithm/string.hpp>

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace WindParser {

static bool ParseHeader(const std::string &line, Wind &wind);
static bool ParseStation(std::vector<std::string> &items,
                         JETProvider::WindStation &station);

bool ParseWindBuffer(TimeStamp clock, const char *buffer, Wind &wind) {
  std::istringstream istr{buffer};

  std::string line;
  std::getline(istr, line);

  if (!ParseHeader(line, wind))
    return false;

  while (std::getline(istr, line)) {
    if (line.size() == 0) continue;
    if (line[0] == '#') continue;

    std::vector<std::string> items;
    boost::split(items, line, boost::is_any_of(","));

    JETProvider::WindStation station;
    if (!ParseStation(items, station)) {
      /* skip the row, keep the response: see the note in WindParser.hpp */
      LogFormat("WindParser skipping invalid row, items.size()=%d",
                (int)items.size());
      continue;
    }

    wind.stations.push_back(std::move(station));
  }

  wind.validity.Update(clock);
  return true;
}

static bool ParseHeader(const std::string &line, Wind &wind) {
  std::vector<std::string> items;
  boost::split(items, line, boost::is_any_of(","));
  if (items.size() != 2)
    return false;

  wind.count = atoi(items[0].c_str());
  wind.total_count = atoi(items[1].c_str());

  return true;
}

static bool ParseStation(std::vector<std::string> &items,
                         JETProvider::WindStation &station) {
  // holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728
  // id,name,provider,lat,lon,alt,wind_dir,wind_avg,wind_max,epoch
  if (items.size() < 10)
    return false;

  station.id = std::move(items[0]);
  station.name = std::move(items[1]);
  station.provider = std::move(items[2]);

  /* atof() turns an unparseable coordinate into 0, which would pin the
     station in the Gulf of Guinea instead of dropping it - and 0,0 is
     itself a valid, in-range GeoPoint, so GeoPoint::Check() alone
     cannot catch that case.  strtod()'s endptr tells them apart. */
  char *lat_end, *lon_end;
  const double latitude = strtod(items[3].c_str(), &lat_end);
  const double longitude = strtod(items[4].c_str(), &lon_end);
  if (lat_end == items[3].c_str() || lon_end == items[4].c_str())
    return false;

  station.location = GeoPoint(Angle::Degrees(longitude),
                              Angle::Degrees(latitude));
  if (!station.location.Check())
    return false;

  station.altitude = (int)std::lround(
    Units::ToSysUnit(atof(items[5].c_str()), Unit::FEET));

  /* the endpoint reports the direction the wind blows FROM, which is
     the same convention as SpeedVector::bearing and the own-ship wind
     arrow */
  station.wind = SpeedVector(Angle::Degrees(atof(items[6].c_str())),
                             Units::ToSysUnit(atof(items[7].c_str()),
                                              Unit::KNOTS));
  station.wind_max = Units::ToSysUnit(atof(items[8].c_str()), Unit::KNOTS);

  station.measured_at = std::chrono::system_clock::from_time_t(
    (time_t)atoll(items[9].c_str()));

  return true;
}

}
