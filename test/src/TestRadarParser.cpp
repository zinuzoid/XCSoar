// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Tracking/JETProvider/RadarParser.hpp"
#include "Units/System.hpp"
#include "TestUtil.hpp"

#include <cmath>

static void
TestValidBuffer()
{
  static constexpr char buffer[] =
    "2,5\n"
    "a1,Alpha One,AB12,50.869501,0.010864,42,1500,300,2,1615771825,744,259\n"
    "b2,Bravo,CD34,-33.865143,151.2099,180,3500,55,-450,1615771830,701,2\n";

  RadarParser::Radar radar;
  ok1(RadarParser::ParseRadarBuffer(buffer, radar));
  ok1(radar.count == 2);
  ok1(radar.total_count == 5);
  ok1(radar.traffics.size() == 2);

  const auto &t1 = radar.traffics[0];
  ok1(t1.traffic_id == "a1");
  ok1(t1.display == "Alpha One");
  ok1(t1.code == "AB12");
  ok1(equals(t1.location, 50.869501, 0.010864));
  ok1(t1.track == 42);
  ok1(t1.altitude == (int)round(Units::ToSysUnit(1500, Unit::FEET)));
  ok1(equals(t1.speed, Units::ToSysUnit(300, Unit::KNOTS)));
  ok1(equals(t1.vspeed, Units::ToSysUnit(2, Unit::FEET_PER_MINUTE)));
  ok1(t1.epoch == 1615771825u);
  ok1(t1.type == "744");
  ok1(t1.icon_type == 259);

  const auto &t2 = radar.traffics[1];
  ok1(t2.traffic_id == "b2");
  ok1(equals(t2.location, -33.865143, 151.2099));
  ok1(equals(t2.vspeed, Units::ToSysUnit(-450, Unit::FEET_PER_MINUTE)));
}

static void
TestCommentsAndBlankLines()
{
  static constexpr char buffer[] =
    "1,1\n"
    "\n"
    "# comment line\n"
    "c3,Charlie,EF56,10.5,-20.25,0,0,0,0,1615771840,700,1\n";

  RadarParser::Radar radar;
  ok1(RadarParser::ParseRadarBuffer(buffer, radar));
  ok1(radar.traffics.size() == 1);
  ok1(radar.traffics[0].traffic_id == "c3");
  ok1(equals(radar.traffics[0].location, 10.5, -20.25));
}

static void
TestHeaderOnly()
{
  RadarParser::Radar radar;
  ok1(RadarParser::ParseRadarBuffer("0,0\n", radar));
  ok1(radar.count == 0);
  ok1(radar.traffics.empty());
}

static void
TestMalformed()
{
  {
    /* empty buffer */
    RadarParser::Radar radar;
    ok1(!RadarParser::ParseRadarBuffer("", radar));
  }

  {
    /* header with too few fields */
    RadarParser::Radar radar;
    ok1(!RadarParser::ParseRadarBuffer("1\n", radar));
  }

  {
    /* header with too many fields */
    RadarParser::Radar radar;
    ok1(!RadarParser::ParseRadarBuffer("1,2,3\n", radar));
  }

  {
    /* traffic line with too few fields */
    RadarParser::Radar radar;
    ok1(!RadarParser::ParseRadarBuffer("1,1\n"
                                       "a1,Alpha,AB,1.0,2.0,3,4,5,6,7,8\n",
                                       radar));
  }
}

int main()
{
  plan_tests(29);

  TestValidBuffer();
  TestCommentsAndBlankLines();
  TestHeaderOnly();
  TestMalformed();

  return exit_status();
}
