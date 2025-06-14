// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Plane/Plane.hpp"
#include "Plane/PlaneFileGlue.hpp"
#include "io/FileLineReader.hpp"
#include "io/KeyValueFileReader.hpp"
#include "io/KeyValueFileWriter.hpp"
#include "Units/System.hpp"
#include "TestUtil.hpp"
#include "util/StringAPI.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>
#include <tchar.h>

static void
TestReader()
{
  Plane plane;
  PlaneGlue::ReadFile(plane, Path(_T("test/data/D-4449.xcp")));

  ok1(plane.registration == _T("D-4449"));
  ok1(plane.competition_id == _T("TH"));
  ok1(plane.type == _T("Hornet"));
  ok1(plane.handicap == 100);
  ok1(plane.polar_name == _T("Hornet"));
  ok1(equals(plane.polar_shape[0].v,
             Units::ToSysUnit(80, Unit::KILOMETER_PER_HOUR)));
  ok1(equals(plane.polar_shape[1].v,
             Units::ToSysUnit(120, Unit::KILOMETER_PER_HOUR)));
  ok1(equals(plane.polar_shape[2].v,
             Units::ToSysUnit(160, Unit::KILOMETER_PER_HOUR)));
  ok1(equals(plane.polar_shape[0].w, -0.606));
  ok1(equals(plane.polar_shape[1].w, -0.99));
  ok1(equals(plane.polar_shape[2].w, -1.918));
  ok1(equals(plane.polar_shape.reference_mass, 318));
  ok1(equals(plane.dry_mass_obsolete, 302));
  ok1(equals(plane.empty_mass, 212));
  ok1(equals(plane.max_ballast, 100));
  ok1(plane.dump_time == 90);
  ok1(equals(plane.max_speed, 41.666));
  ok1(equals(plane.wing_area, 9.8));
  ok1(equals(plane.weglide_glider_type, 261));

  plane = Plane();
  PlaneGlue::ReadFile(plane, Path(_T("test/data/D-4449dry.xcp")));
  ok1(equals(plane.empty_mass, 212));
}

static void
TestWriter()
{
  Plane plane;
  plane.registration = _T("D-4449");
  plane.competition_id = _T("TH");
  plane.type = _T("Hornet");
  plane.handicap = 100;
  plane.polar_name = _T("Hornet");
  plane.polar_shape[0].v = Units::ToSysUnit(80, Unit::KILOMETER_PER_HOUR);
  plane.polar_shape[1].v = Units::ToSysUnit(120, Unit::KILOMETER_PER_HOUR);
  plane.polar_shape[2].v = Units::ToSysUnit(160, Unit::KILOMETER_PER_HOUR);
  plane.polar_shape[0].w = -0.606;
  plane.polar_shape[1].w = -0.99;
  plane.polar_shape[2].w = -1.918;
  plane.polar_shape.reference_mass = 318;
  plane.dry_mass_obsolete = 302;
  plane.empty_mass = 212;
  plane.max_ballast = 100;
  plane.dump_time = 90;
  plane.max_speed = 41.666;
  plane.wing_area = 9.8;
  plane.weglide_glider_type = 160;
  plane.is_powered = true;
  plane.average_tas = 1.2;
  plane.fuel_onboard = 2.3;
  plane.fuel_consumption = 3.4;

  PlaneGlue::WriteFile(plane, Path(_T("output/D-4449.xcp")));

  FileLineReaderA reader(Path(_T("output/D-4449.xcp")));

  unsigned count = 0;
  bool found1 = false, found2 = false, found3 = false, found4 = false;
  bool found5 = false, found6 = false, found7 = false, found8 = false;
  bool found9 = false, found10 = false, found11 = false, found12 = false;
  bool found13 = false, found14 = false, found15 = false, found16 = false;
  bool found17 = false, found18 = false;

  char *line;
  while ((line = reader.ReadLine()) != NULL) {
    if (StringIsEqual(line, "Registration=\"D-4449\""))
      found1 = true;
    if (StringIsEqual(line, "CompetitionID=\"TH\""))
      found2 = true;
    if (StringIsEqual(line, "Type=\"Hornet\""))
      found3 = true;
    if (StringIsEqual(line, "Handicap=\"100\""))
      found4 = true;
    if (StringIsEqual(line, "PolarName=\"Hornet\""))
      found5 = true;
    if (StringIsEqual(line, "PolarInformation=\"80.000,-0.606,120.000,-0.990,160.000,-1.918\""))
      found6 = true;
    if (StringIsEqual(line, "PolarReferenceMass=\"318.000000\""))
      found7 = true;
    if (StringIsEqual(line, "PlaneEmptyMass=\"212.000000\""))
      found8 = true;
    if (StringIsEqual(line, "MaxBallast=\"100.000000\""))
      found9 = true;
    if (StringIsEqual(line, "DumpTime=\"90.000000\""))
      found10 = true;
    if (StringIsEqual(line, "MaxSpeed=\"41.666000\""))
      found11 = true;
    if (StringIsEqual(line, "WingArea=\"9.800000\""))
      found12 = true;
    if (StringIsEqual(line, "WeGlideAircraftType=\"160\""))
      found13 = true;
    if (StringIsEqual(line, "PolarDryMass=\"302.000000\""))
      found14 = true;
    if (StringIsEqual(line, _T("IsPowered=\"1\"")))
      found15 = true;
    if (StringIsEqual(line, _T("AverageTAS=\"1.200000\"")))
      found16 = true;
    if (StringIsEqual(line, _T("FuelOnboard=\"2.300000\"")))
      found17 = true;
    if (StringIsEqual(line, _T("FuelConsumption=\"3.400000\"")))
      found18 = true;

    count++;
  }

  ok1(count == 18);
  ok1(found1);
  ok1(found2);
  ok1(found3);
  ok1(found4);
  ok1(found5);
  ok1(found6);
  ok1(found7);
  ok1(found8);
  ok1(found9);
  ok1(found10);
  ok1(found11);
  ok1(found12);
  ok1(found13);
  ok1(found14);
  ok1(found15);
  ok1(found16);
  ok1(found17);
  ok1(found18);
}

int main()
try {
  plan_tests(39);

  TestReader();
  TestWriter();

  return exit_status();
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
