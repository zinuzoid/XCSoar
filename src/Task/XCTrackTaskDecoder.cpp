// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "XCTrackTaskDecoder.hpp"
#include "PolylineDecoder.hpp"
#include "Geo/GeoPoint.hpp"
#include "Engine/Task/ObservationZones/CylinderZone.hpp"
#include "Engine/Task/Factory/AbstractTaskFactory.hpp"
#include "Engine/Task/Ordered/OrderedTask.hpp"
#include "Engine/Task/Ordered/Points/StartPoint.hpp"
#include "Engine/Task/Ordered/Points/FinishPoint.hpp"
#include "Engine/Task/Ordered/Points/ASTPoint.hpp"
#include "Engine/Waypoint/Ptr.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "util/ConvertString.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <stdexcept>
#include <string>

using std::string_view_literals::operator""sv;

struct XCTrackZ {
  GeoPoint location;
  int_least32_t altitude, radius;
};

static XCTrackZ
DecodeXCTrackZ(std::string_view src)
{
  XCTrackZ z;

  /* note: XCTrack encodes the location in reverse order (longitude
     then latitude), unlike Google's Polyline specification where
     latitude comes first */
  z.location = ReadPolylineLonLat(src);

  z.altitude = ReadPolylineInt(src);
  if (z.altitude < -1000 || z.altitude > 9000)
    throw std::invalid_argument{"Invalid altitude"};

  z.radius = ReadPolylineInt(src);
  if (z.radius <= 0 || z.radius > 500000)
    throw std::invalid_argument{"Invalid radius"};

  if (!src.empty())
    throw std::invalid_argument{"Garbage after XCTrac 'z'"};

  return z;
}

static WaypointPtr
MakeWaypoint(GeoPoint location, double elevation, const TCHAR *name,
             const TCHAR *comment)
{
  Waypoint *wp = new Waypoint(location);
  wp->name = name;
  wp->comment = comment;
  wp->elevation = elevation;
  wp->has_elevation = true;
  return WaypointPtr{wp};
}

/**
 * Obtain a string field from an XCTrack turn point object; returns an
 * empty string_view if the field is missing or not a string.
 */
static std::string_view
GetOptionalString(const boost::json::object &j, std::string_view name) noexcept
{
  if (const auto *value = j.if_contains(name))
    if (const auto *s = value->if_string())
      return {s->data(), s->size()};

  return {};
}

/**
 * XCTrack omits the name of plain turn points; synthesize the same
 * label XCTrack displays for those.
 */
static std::string
MakeTurnPointName(std::string_view name, std::size_t i)
{
  if (!name.empty())
    return std::string{name};

  return "T" + std::to_string(i + 1);
}

std::unique_ptr<OrderedTask>
DecodeXCTrackTask(const boost::json::value &_j,
                  const TaskBehaviour &task_behaviour)
{
  const auto &j = _j.as_object();

  if (j.at("taskType"sv).as_string() != "CLASSIC"sv)
    throw std::invalid_argument{"Unrecognized XCTrack taskType"};

  if (j.at("version"sv).as_int64() != 2)
    throw std::invalid_argument{"Unsupported XCTrack task format version"};

  if (const auto *e = j.if_contains("e"))
    if (e->as_int64() != 0)
      throw std::invalid_argument{"Unsupported XCTrack earthModel"};

  const auto &t = j.at("t").as_array();

  auto task = std::make_unique<OrderedTask>(task_behaviour);
  task->SetFactory(TaskFactoryType::RACING);

  AbstractTaskFactory &fact = task->GetFactory();

  // TODO how to import the first turn point ("TAKEOFF")?
  // TODO use s.g
  // TODO use s.d
  // TODO use s.t
  // TODO use g.d
  // TODO use g.t

  for (std::size_t i = 0, n = t.size(); i < n; ++i) {
    const auto &j = t[i].as_object();

    const auto z = DecodeXCTrackZ(j.at("z"sv).as_string());

    /* the name is optional: XCTrack leaves it empty for plain turn
       points, and only the takeoff/named ones carry one */
    const auto name = MakeTurnPointName(GetOptionalString(j, "n"sv), i);
    const UTF8ToWideConverter name_t{name.c_str()};
    if (!name_t.IsValid())
      throw std::invalid_argument{"Malformed name"};

    /* "d" is a free-text description, not a name; keep it as comment */
    const std::string comment{GetOptionalString(j, "d"sv)};
    const UTF8ToWideConverter comment_t{comment.c_str()};
    if (!comment_t.IsValid())
      throw std::invalid_argument{"Malformed description"};

    auto oz = std::make_unique<CylinderZone>(z.location, z.radius);
    auto wp = MakeWaypoint(z.location, z.altitude, name_t.c_str(),
                           comment_t.c_str());

    std::unique_ptr<OrderedTaskPoint> tp;

    if (i == 0)
      tp = fact.CreateStart(std::move(oz), std::move(wp));
    else if (i == n - 1)
      tp = fact.CreateFinish(std::move(oz), std::move(wp));
    else
      tp = fact.CreateASTPoint(std::move(oz), std::move(wp));

    if (!tp)
      throw std::invalid_argument{"Failed to create turn point"};

    fact.Append(*tp);
  }

  return task;
}
