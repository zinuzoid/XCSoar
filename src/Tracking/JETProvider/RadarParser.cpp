// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RadarParser.hpp"
#include "Units/Units.hpp"
#include "LogFile.hpp"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace RadarParser {

static bool
ParseHeader(const std::string &line, Radar &radar)
{
  std::vector<std::string> items;
  boost::split(items, line, boost::is_any_of(","));
  if (items.size() != 2)
    return false;

  radar.count = atoi(items[0].c_str());
  radar.total_count = atoi(items[1].c_str());

  return true;
}

static bool
ParseTraffic(const std::string &line, Radar &radar)
{
  std::vector<std::string> items;
  boost::split(items, line, boost::is_any_of(","));
  if (items.size() < 12) {
    LogFormat("RadarParser received invalid items.size()=%d",
              (int)items.size());
    return false;
  }

  // a1,deadbeef,ab12,50.869501,0.010864,42,1500,300,2,1615771825,744,1
  // uid,display,code,lat,long,track,alt,spd,vspd,epoch,type,icon_type

  JETProvider::Traffic traffic;
  traffic.traffic_id = items[0];
  traffic.display = items[1];
  traffic.code = items[2];
  double latitude = atof(items[3].c_str());
  double longitude = atof(items[4].c_str());
  traffic.location = GeoPoint(Angle::Degrees(longitude),
                              Angle::Degrees(latitude));
  traffic.track = atoi(items[5].c_str());
  traffic.altitude = atoi(items[6].c_str());
  traffic.speed = atof(items[7].c_str());
  traffic.vspeed = atof(items[8].c_str());
  traffic.epoch = atoi(items[9].c_str());
  traffic.type = items[10];
  traffic.icon_type = atoi(items[11].c_str());

  traffic.altitude = round(Units::ToSysUnit(traffic.altitude, Unit::FEET));
  traffic.speed = Units::ToSysUnit(traffic.speed, Unit::KNOTS);
  traffic.vspeed = Units::ToSysUnit(traffic.vspeed, Unit::FEET_PER_MINUTE);

  radar.traffics.push_back(std::move(traffic));

  return true;
}

bool
ParseRadarBuffer(const char *buffer, Radar &radar)
{
  std::istringstream istr{buffer};

  std::string line;
  std::getline(istr, line);

  if (!ParseHeader(line, radar))
    return false;

  while (std::getline(istr, line)) {
    if (line.size() == 0)
      continue;
    if (line[0] == '#')
      continue;
    if (!ParseTraffic(line, radar))
      return false;
  }

  return true;
}

} // namespace RadarParser
