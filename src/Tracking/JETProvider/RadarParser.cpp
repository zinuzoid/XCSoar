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

#include "RadarParser.hpp"
#include "JETProvider.hpp"
#include "Interface.hpp"
#include "Units/Units.hpp"
#include "LogFile.hpp"

#include <boost/algorithm/string.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace RadarParser {

bool ParseHeader(std::string, Radar &radar);
bool ParseTraffic(std::string line, Radar &radar);
const char *to_c_str(std::string str);

bool ParseRadarBuffer(const NMEAInfo &basic, const char *buffer, Radar &radar) {
  std::istringstream istr{buffer};

  std::string line;
  std::getline(istr, line);

  if (!ParseHeader(line, radar)) {
      return false;
  }

  while(std::getline(istr, line)) {
      if (line.size() == 0) continue;
      if (line[0] == '#') continue;
      if (!ParseTraffic(line, radar)) {
          return false;
      }
  }

  radar.validity.Update(basic.clock);
  return true;
}

bool ParseHeader(std::string line, Radar &radar) {
    std::vector<std::string> items;
    boost::split(items, line, boost::is_any_of(","));
    if (items.size() != 2) {
        return false;
    }
    radar.count = atoi(items[0].c_str());
    radar.total_count = atoi(items[1].c_str());

    return true;
}

bool ParseTraffic(std::string line, Radar &radar) {
    std::vector<std::string> items;
    boost::split(items, line, boost::is_any_of(","));
    if (items.size() != 11) {
        LogFormat("RadarParser received invalid items.size()=%d", (int) items.size());
        return false;
    }
    // a1,deadbeef,50.869501,0.010864,42,1500,300,2,1615771825,744
    // uid,name,lat,long,track,alt,spd,vspd,epoch,type

    JETProvider::Traffic traffic;
    traffic.traffic_id = to_c_str(items[0]);
    traffic.display = to_c_str(items[1]);
    traffic.code = to_c_str(items[2]);
    double latitude = atof(items[3].c_str());
    double longitude = atof(items[4].c_str());
    traffic.location = GeoPoint(Angle::Degrees(longitude), Angle::Degrees(latitude));
    traffic.track = atoi(items[5].c_str());
    traffic.altitude = atoi(items[6].c_str());
    traffic.speed = atof(items[7].c_str());
    traffic.vspeed = atof(items[8].c_str());
    traffic.epoch = atoi(items[9].c_str());
    traffic.type = to_c_str(items[10]);

    traffic.altitude = round(Units::ToSysUnit(traffic.altitude, Unit::FEET));
    traffic.speed = Units::ToSysUnit(traffic.speed, Unit::KNOTS);
    traffic.vspeed = Units::ToSysUnit(traffic.vspeed, Unit::FEET_PER_MINUTE);

    radar.traffics.push_back(traffic);

    return true;
}

const char *to_c_str(std::string str) {
    char *buf = new char[str.size() +1];
    std::copy(str.begin(), str.end(), buf);
    buf[str.size()] = '\0';
    return buf;
}

}
