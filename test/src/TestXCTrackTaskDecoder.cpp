// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Task/XCTrackTaskDecoder.hpp"
#include "Engine/Task/Ordered/OrderedTask.hpp"
#include "Engine/Task/Ordered/Points/OrderedTaskPoint.hpp"
#include "Engine/Task/ObservationZones/CylinderZone.hpp"
#include "Engine/Task/Points/Type.hpp"
#include "Engine/Task/TaskBehaviour.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "util/PrintException.hxx"

#include "TestUtil.hpp"

#include <boost/json/parse.hpp>

#include <string_view>

#include <tchar.h>

using std::string_view_literals::operator""sv;

static TaskBehaviour task_behaviour;

static std::unique_ptr<OrderedTask>
Decode(std::string_view json)
{
  return DecodeXCTrackTask(boost::json::parse(json), task_behaviour);
}

/**
 * Decode the task and return true if it was rejected with an
 * exception.
 */
static bool
IsRejected(std::string_view json) noexcept
try {
  Decode(json);
  return false;
} catch (...) {
  return true;
}

[[gnu::pure]]
static double
GetRadius(const OrderedTaskPoint &tp) noexcept
{
  const auto &oz = tp.GetObservationZone();
  if (oz.GetShape() != ObservationZone::Shape::CYLINDER)
    return -1;

  return static_cast<const CylinderZone &>(oz).GetRadius();
}

/**
 * A real-world QR code payload, as emitted by XCTrack.  Only the first
 * turn point is named; the other two carry an empty "n", which used to
 * make the decoder throw "Name is empty" (and left the Android
 * ReceiveTaskActivity stuck on a blank screen).
 */
static constexpr auto unnamed_json = R"({"taskType":"CLASSIC","version":2,)"
  R"("t":[{"z":"lygMev_}HkY_|B","n":"Worcestershire Beacon","d":"","t":2,"o":{"a1":180}},)"
  R"({"z":"ta|Cicx|HqC_X","n":"","d":"","o":{"a1":180}},)"
  R"({"z":"napC}kt|HyD_X","n":"","d":"","t":3,"o":{"a1":180}}],)"
  R"("s":{"g":[],"d":1,"t":1},"o":{"v":2}})"sv;

static void
TestUnnamedTurnPoints()
{
  const auto task = Decode(unnamed_json);
  ok1(task);
  ok1(task->TaskSize() == 3);

  const auto &tp1 = task->GetTaskPoint(0);
  const auto &tp2 = task->GetTaskPoint(1);
  const auto &tp3 = task->GetTaskPoint(2);

  /* the named turn point keeps its name, the unnamed ones get the
     same "T<n>" label XCTrack displays */
  ok1(tp1.GetWaypoint().name == _T("Worcestershire Beacon"));
  ok1(tp2.GetWaypoint().name == _T("T2"));
  ok1(tp3.GetWaypoint().name == _T("T3"));

  ok1(tp1.GetType() == TaskPointType::START);
  ok1(tp2.GetType() == TaskPointType::AST);
  ok1(tp3.GetType() == TaskPointType::FINISH);

  /* XCTrack encodes longitude before latitude, unlike Google's
     polyline specification */
  ok1(equals(tp1.GetWaypoint().location,
             GeoPoint{Angle::Degrees(-2.33895), Angle::Degrees(52.10483)}));
  ok1(equals(tp2.GetWaypoint().location,
             GeoPoint{Angle::Degrees(-0.80427), Angle::Degrees(52.06597)}));
  ok1(equals(tp3.GetWaypoint().location,
             GeoPoint{Angle::Degrees(-0.74280), Angle::Degrees(52.04687)}));

  ok1(tp1.GetWaypoint().has_elevation);
  ok1(equals(tp1.GetWaypoint().elevation, 422));
  ok1(equals(tp2.GetWaypoint().elevation, 73));
  ok1(equals(tp3.GetWaypoint().elevation, 93));

  ok1(equals(GetRadius(tp1), 2000));
  ok1(equals(GetRadius(tp2), 400));
  ok1(equals(GetRadius(tp3), 400));
}

static void
TestNamedTurnPoints()
{
  const auto task = Decode(R"({"taskType":"CLASSIC","version":2,"e":0,)"
                           R"("t":[{"z":"lygMev_}HkY_|B","n":"Alpha","d":"the hill"},)"
                           R"({"z":"ta|Cicx|HqC_X","n":"Bravo"},)"
                           R"({"z":"napC}kt|HyD_X","n":"Charlie"}]})"sv);
  ok1(task);
  ok1(task->TaskSize() == 3);
  ok1(task->GetTaskPoint(0).GetWaypoint().name == _T("Alpha"));
  ok1(task->GetTaskPoint(1).GetWaypoint().name == _T("Bravo"));
  ok1(task->GetTaskPoint(2).GetWaypoint().name == _T("Charlie"));

  /* "d" is a free-text description, imported as comment */
  ok1(task->GetTaskPoint(0).GetWaypoint().comment == _T("the hill"));
  ok1(task->GetTaskPoint(1).GetWaypoint().comment.empty());
}

static void
TestMissingName()
{
  /* "n" absent altogether, not just empty */
  const auto task = Decode(R"({"taskType":"CLASSIC","version":2,)"
                           R"("t":[{"z":"lygMev_}HkY_|B"},)"
                           R"({"z":"ta|Cicx|HqC_X"},)"
                           R"({"z":"napC}kt|HyD_X"}]})"sv);
  ok1(task);
  ok1(task->TaskSize() == 3);
  ok1(task->GetTaskPoint(0).GetWaypoint().name == _T("T1"));
  ok1(task->GetTaskPoint(1).GetWaypoint().name == _T("T2"));
  ok1(task->GetTaskPoint(2).GetWaypoint().name == _T("T3"));
}

static void
TestRejected()
{
  /* unsupported task type */
  ok1(IsRejected(R"({"taskType":"WAYPOINTS","version":2,)"
                 R"("t":[{"z":"lygMev_}HkY_|B","n":"a"},)"
                 R"({"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* unsupported format version */
  ok1(IsRejected(R"({"taskType":"CLASSIC","version":1,)"
                 R"("t":[{"z":"lygMev_}HkY_|B","n":"a"},)"
                 R"({"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* unsupported earth model */
  ok1(IsRejected(R"({"taskType":"CLASSIC","version":2,"e":1,)"
                 R"("t":[{"z":"lygMev_}HkY_|B","n":"a"},)"
                 R"({"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* zero radius */
  ok1(IsRejected(R"({"taskType":"CLASSIC","version":2,)"
                 R"("t":[{"z":"lygMev_}HkY?","n":"a"},)"
                 R"({"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* garbage after the polyline */
  ok1(IsRejected(R"({"taskType":"CLASSIC","version":2,)"
                 R"("t":[{"z":"lygMev_}HkY_|B_|B","n":"a"},)"
                 R"({"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* "z" missing */
  ok1(IsRejected(R"({"taskType":"CLASSIC","version":2,)"
                 R"("t":[{"n":"a"},{"z":"napC}kt|HyD_X","n":"b"}]})"sv));

  /* not an object */
  ok1(IsRejected("[]"sv));
}

int main()
try {
  plan_tests(18 + 7 + 5 + 7);

  task_behaviour.SetDefaults();

  TestUnnamedTurnPoints();
  TestNamedTurnPoints();
  TestMissingName();
  TestRejected();

  return exit_status();
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
