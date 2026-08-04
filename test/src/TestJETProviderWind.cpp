// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Tracking/JETProvider/WindParser.hpp"
#include "TestUtil.hpp"

using namespace JETProvider;
using namespace std::chrono;

static constexpr TimeStamp CLOCK{FloatDuration{3600}};

int main()
{
  plan_tests(50);

  /* an empty area returns just the header */
  {
    WindParser::Wind wind;
    ok1(WindParser::ParseWindBuffer(CLOCK, "0,0\n", wind));
    ok1(wind.count == 0);
    ok1(wind.total_count == 0);
    ok1(wind.stations.empty());
    ok1(wind.validity.IsValid());
  }

  /* no trailing newline on the header-only body */
  {
    WindParser::Wind wind;
    ok1(WindParser::ParseWindBuffer(CLOCK, "0,0", wind));
    ok1(wind.stations.empty());
  }

  /* malformed headers fail the whole parse, and validity is left
     untouched; Wind{} is value-initialised here (unlike the other
     blocks) specifically so that "untouched" is a deterministic zero
     rather than Validity's intentionally-uninitialised default */
  {
    WindParser::Wind wind{};
    ok1(!WindParser::ParseWindBuffer(CLOCK, "", wind));
    ok1(!wind.validity.IsValid());

    WindParser::Wind wind2{};
    ok1(!WindParser::ParseWindBuffer(CLOCK, "3", wind2));

    WindParser::Wind wind3{};
    ok1(!WindParser::ParseWindBuffer(CLOCK, "3,3,3", wind3));
  }

  /* a single complete row decodes every field */
  {
    WindParser::Wind wind;
    const char *body =
      "1,1\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 1);

    const auto &s = wind.stations[0];
    ok1(s.id == "holfuy-1175");
    ok1(s.name == "Long Mynd");
    ok1(s.provider == "holfuy.com");
    /* field 3 is latitude, field 4 is longitude - assert both
       separately to catch a silent hemisphere swap */
    ok1(equals(s.location.latitude.Degrees(), 52.51847));
    ok1(equals(s.location.longitude.Degrees(), -2.88154));
    ok1(s.altitude == 430); // 1411 ft
    ok1(equals(s.wind.bearing.Degrees(), 271));
    ok1(equals(s.wind.norm, 9 / 1.94384449));
    ok1(equals(s.wind_max, 11 / 1.94384449));
    ok1(s.measured_at == system_clock::from_time_t(1786129728));
  }

  /* multiple rows, order preserved */
  {
    WindParser::Wind wind;
    const char *body =
      "3,3\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n"
      "holfuy-295,The Lawley,holfuy.com,52.57287,-2.74705,1214,278,9,10,1786129751\n"
      "metar-EGBB,Birmingham Intl,aviationweather.gov,52.46,-1.758,299,270,3,3,1786128600\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 3);
    ok1(wind.stations[2].id == "metar-EGBB");
  }

  /* a capped response is trusted by rows returned, not by the header
     counts */
  {
    WindParser::Wind wind;
    const char *body =
      "80,96\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n"
      "holfuy-295,The Lawley,holfuy.com,52.57287,-2.74705,1214,278,9,10,1786129751\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.count == 80);
    ok1(wind.total_count == 96);
    ok1(wind.stations.size() == 2);
  }

  /* a malformed row (too few fields) is skipped, its neighbours survive:
     unlike RadarParser, one bad station must not blank the overlay */
  {
    WindParser::Wind wind;
    const char *body =
      "3,3\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n"
      "broken,row,only,six,fields,here\n"
      "metar-EGBB,Birmingham Intl,aviationweather.gov,52.46,-1.758,299,270,3,3,1786128600\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 2);
    ok1(wind.stations[0].id == "holfuy-1175");
    ok1(wind.stations[1].id == "metar-EGBB");
  }

  /* unparseable / out-of-range coordinates are skipped, not turned into
     a phantom station at 0N 0E */
  {
    WindParser::Wind wind;
    const char *body =
      "3,3\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n"
      "bad-lat,Bad Lat,test,999,-2.88154,1411,271,9,11,1786129728\n"
      "bad-latlon,Bad LatLon,test,abc,xyz,1411,271,9,11,1786129728\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 1);
    ok1(wind.stations[0].id == "holfuy-1175");
  }

  /* extra trailing fields are tolerated - the check is "at least 10" */
  {
    WindParser::Wind wind;
    const char *body =
      "1,1\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728,extra\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 1);
    ok1(wind.stations[0].id == "holfuy-1175");
  }

  /* blank lines and comment lines are skipped */
  {
    WindParser::Wind wind;
    const char *body =
      "1,1\n"
      "\n"
      "# a comment\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 1);
  }

  /* CRLF line endings: the header still splits into 2 items and the
     stray \r lands only on the last (epoch) field */
  {
    WindParser::Wind wind;
    std::string body =
      "1,1\r\n"
      "holfuy-1175,Long Mynd,holfuy.com,52.51847,-2.88154,1411,271,9,11,1786129728\r\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body.c_str(), wind));
    ok1(wind.stations.size() == 1);
    ok1(wind.stations[0].id == "holfuy-1175");
    ok1(equals(wind.stations[0].wind.norm, 9 / 1.94384449));
  }

  /* southern / western hemisphere signs */
  {
    WindParser::Wind wind;
    const char *body =
      "1,1\n"
      "test-1,Test,test,-33.9,18.6,100,180,5,7,1786129728\n";
    ok1(WindParser::ParseWindBuffer(CLOCK, body, wind));
    ok1(wind.stations.size() == 1);
    ok1(equals(wind.stations[0].location.latitude.Degrees(), -33.9));
    ok1(equals(wind.stations[0].location.longitude.Degrees(), 18.6));
  }

  return exit_status();
}
