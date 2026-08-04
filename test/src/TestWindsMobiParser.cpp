// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/WindsMobi/Parser.hpp"
#include "Weather/WindsMobi/Station.hpp"
#include "TestUtil.hpp"

#include <boost/json.hpp>

using namespace WindsMobi;

int main()
{
  plan_tests(21);

  /* empty response */
  {
    auto stations = ParseStations(boost::json::parse("[]"));
    ok1(stations.empty());
  }

  /* a complete, valid station */
  {
    auto stations = ParseStations(boost::json::parse(R"([{
      "_id": "holfuy-1636",
      "short": "Pormenaz",
      "pv-name": "holfuy.com",
      "alt": 1000,
      "loc": {"type": "Point", "coordinates": [6.63, 46.78]},
      "last": {"_id": 1700000000, "w-dir": 270, "w-avg": 18.0, "w-max": 27.0}
    }])"));

    ok1(stations.size() == 1);
    const auto &s = stations[0];
    ok1(s.id == "holfuy-1636");
    ok1(s.name == "Pormenaz");
    ok1(s.provider == "holfuy.com");
    ok1(s.altitude == 1000);

    /* "coordinates" is [longitude, latitude]; getting this backwards
       would put the station in the wrong hemisphere */
    ok1(equals(s.location, 46.78, 6.63));

    ok1(s.wind_available);
    ok1(equals(s.wind.bearing.Degrees(), 270));
    /* 18 km/h -> 5 m/s, 27 km/h -> 7.5 m/s */
    ok1(equals(s.wind.norm, 5.0));
    ok1(equals(s.wind_max, 7.5));
  }

  /* a station with no recent measurement: "last" is entirely absent,
     which is normal for a station outside the requested time window */
  {
    auto stations = ParseStations(boost::json::parse(R"([{
      "_id": "quiet-1",
      "short": "Quiet",
      "loc": {"type": "Point", "coordinates": [7.0, 47.0]}
    }])"));

    ok1(stations.size() == 1);
    ok1(!stations[0].wind_available);
  }

  /* "last" present but without a gust value: still usable, just no
     gust figure */
  {
    auto stations = ParseStations(boost::json::parse(R"([{
      "_id": "avg-only",
      "loc": {"type": "Point", "coordinates": [7.0, 47.0]},
      "last": {"_id": 1700000000, "w-dir": 90, "w-avg": 10.0}
    }])"));

    ok1(stations.size() == 1);
    ok1(stations[0].wind_available);
    ok1(equals(stations[0].wind.norm, 10.0 / 3.6));
    ok1(stations[0].wind_max < 0);
  }

  /* a station with no location cannot be placed on the map and must
     be skipped, without discarding the rest of the response */
  {
    auto stations = ParseStations(boost::json::parse(R"([
      {"_id": "no-location"},
      {"_id": "has-location",
       "loc": {"type": "Point", "coordinates": [1.0, 2.0]}}
    ])"));

    ok1(stations.size() == 1);
    ok1(stations[0].id == "has-location");
  }

  /* a station missing even "_id" must not crash the parser or take
     down the rest of the batch */
  {
    auto stations = ParseStations(boost::json::parse(R"([
      {"loc": {"type": "Point", "coordinates": [1.0, 2.0]}},
      {"_id": "ok",
       "loc": {"type": "Point", "coordinates": [1.0, 2.0]}}
    ])"));

    ok1(stations.size() == 1);
    ok1(stations[0].id == "ok");
  }

  return exit_status();
}
