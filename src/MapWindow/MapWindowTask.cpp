// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindow.hpp"
#include "Geo/Math.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Engine/Task/TaskManager.hpp"
#include "Engine/Task/Ordered/OrderedTask.hpp"
#include "Renderer/TaskRenderer.hpp"
#include "Renderer/TaskPointRenderer.hpp"
#include "Renderer/OZRenderer.hpp"
#include "Screen/Layout.hpp"
#include "Math/Screen.hpp"
#include "Look/MapLook.hpp"
#include "LogFile.hpp"
#include "Task/Points/TaskWaypoint.hpp"
#include "Task/Ordered/Points/OrderedTaskPoint.hpp"
#include "Interface.hpp"
#include "util/Macros.hpp"

PixelPoint
MapWindow::CalculatePixelPoint(
  PixelPoint p1,
  PixelPoint p2,
  double percent)
{
  return PixelPoint(
    iround(((p1.x-p2.x) * percent) + p2.x),
    iround(((p1.y-p2.y) * percent) + p2.y));
}

void
MapWindow::DrawTask(Canvas &canvas) noexcept
{
  if (task == nullptr)
    return;

  /* RLD bearing is invalid if GPS not connected and in non-sim mode,
   but we can still draw targets */
  bool draw_bearing = Basic().track_available;
  bool draw_route = draw_bearing;

  if (draw_bearing) {
    if (Calculated().planned_route.size()>2) {
      draw_bearing = false;
    } else {
      draw_route = false;
    }
  }

  ProtectedTaskManager::Lease task_manager(*task);
  const AbstractTask *task = task_manager->GetActiveTask();
  if (task && !IsError(task->CheckTask())) {
    const auto target_visibility = IsNearSelf()
      ? TaskPointRenderer::TargetVisibility::ACTIVE
      : TaskPointRenderer::TargetVisibility::ALL;

    /* the FlatProjection parameter is used by class TaskPointRenderer
       only when drawing an OrderedTask, so it's okay to pass an
       uninitialized dummy reference when this is not an
       OrderedTask */
    const FlatProjection dummy_flat_projection{};
    const auto &flat_projection = task->GetType() == TaskType::ORDERED
      ? ((const OrderedTask *)task)->GetTaskProjection()
      : dummy_flat_projection;

    OZRenderer ozv(look.task, airspace_renderer.GetLook(),
                   GetMapSettings().airspace);
    TaskPointRenderer tpv(canvas, render_projection, look.task,
                          flat_projection,
                          ozv, draw_bearing, target_visibility,
                          Basic().GetLocationOrInvalid());
    tpv.SetTaskFinished(Calculated().task_stats.task_finished);
    TaskRenderer dv(tpv, render_projection.GetScreenBounds());
    dv.Draw(*task);

    DrawFuelBurnTask(canvas, *task);
  }

  if (draw_route)
    DrawRoute(canvas);
}

void
MapWindow::DrawRoute(Canvas &canvas) noexcept
{
  const auto &route = Calculated().planned_route;

  const auto r_size = route.size();
  constexpr std::size_t capacity = std::decay_t<decltype(route)>::capacity();
  BulkPixelPoint p[capacity];
  std::transform(route.begin(), route.end(), p,
                 [this](const auto &i) {
                   return render_projection.GeoToScreen(i);
                 });

  p[r_size - 1] = ScreenClosestPoint(p[r_size-1], p[r_size-2], p[r_size-1], Layout::Scale(20));

  canvas.Select(look.task.bearing_pen);
  canvas.DrawPolyline(p, r_size);
}

void
MapWindow::DrawFuelBurnTask(Canvas &canvas, const TaskInterface &task) noexcept
{
  const auto &plane = CommonInterface::GetComputerSettings().plane;

  if(
    !plane.is_powered ||
    !Basic().airspeed_available ||
    task.GetType() != TaskType::ORDERED) {
    return;
  }
  
  canvas.Select(look.task.fuel_range_pen);
  canvas.Select(look.task.fuel_range_brush);

  auto &ordered_task = (const OrderedTask &)task;
  auto current_pos = Basic().location;
  auto true_airspeed = plane.average_tas;
  auto fuel_burn_time_remain = Calculated().fuel_burn_time_remain;
  auto wind = Calculated().wind;

  auto active_index = ordered_task.GetActiveTaskPointIndex();
 for (auto i = active_index; i < ordered_task.TaskSize(); i++) {
    const OrderedTaskPoint &tp = ordered_task.GetTaskPoint(i);
    auto effective_wind_angle = wind.bearing.Reciprocal() - tp.GetVectorRemaining(current_pos).bearing;
    auto head_wind = -wind.norm * effective_wind_angle.cos();
    auto estimated_gs = true_airspeed - head_wind;
    if (fabs(estimated_gs) <= std::numeric_limits<double>::epsilon()) {
      LogFormat("DrawFuelBurnTask estimated_gs: %f", estimated_gs);
      return;
    }
    double route_distance;
    if (i == active_index) {
      route_distance = tp.Distance(current_pos);
    } else {
      route_distance = tp.Distance(ordered_task.GetTaskPoint(i - 1).GetLocation());
    }
    auto t = route_distance / estimated_gs;
    // LogFormat("%d/%d: estimated_gs: %f, dis: %f, t: %f", i, ordered_task->TaskSize(), estimated_gs, route_distance, t);
    if (fuel_burn_time_remain - t <= 0) {
      auto waypoint_location = tp.GetLocation();
      auto to_pixel_point = render_projection.GeoToScreen(waypoint_location);
      PixelPoint from_pixel_point;
      if (i == active_index) {
        from_pixel_point = render_projection.GeoToScreen(current_pos);
      } else {
        from_pixel_point = render_projection.GeoToScreen(tp.GetPrevious()->GetLocation());
      }

      double dd = estimated_gs * fuel_burn_time_remain;

      auto mid = CalculatePixelPoint(to_pixel_point, from_pixel_point, dd / route_distance);
      BulkPixelPoint line[] = {
        { (int) +render_projection.GeoToScreenDistance(1000), (int) -render_projection.GeoToScreenDistance(2500) },
        { 0, (int) -render_projection.GeoToScreenDistance(2500) },
        { 0, (int) +render_projection.GeoToScreenDistance(2500) },
        { (int) +render_projection.GeoToScreenDistance(1000), (int) +render_projection.GeoToScreenDistance(2500) },
      };

      PolygonRotateShift({line, ARRAY_SIZE(line)}, mid, tp.GetVectorRemaining(current_pos).bearing + Angle::Degrees(90));

      canvas.DrawCircle(mid, render_projection.GeoToScreenDistance(2000));
      canvas.DrawPolyline(line, 4);

      fuel_burn_time_remain = 0;
      break;
    }
    fuel_burn_time_remain -= t;
  }

  if (fuel_burn_time_remain > 0) {
    const OrderedTaskPoint &tp = ordered_task.GetTaskPoint(ordered_task.TaskSize() - 1);
    auto waypoint_location = tp.GetLocation();
    auto to_pixel_point = render_projection.GeoToScreen(waypoint_location);

    canvas.DrawCircle(to_pixel_point, render_projection.GeoToScreenDistance(1000 + fuel_burn_time_remain * (1000.0 / 300.0)));

    LogFormat("Too much fuel %d", fuel_burn_time_remain);
  }
}

void
MapWindow::DrawTaskOffTrackIndicator(Canvas &canvas) noexcept
{
  if (Calculated().circling 
      || !Basic().location_available
      || !Basic().track_available
      || !GetMapSettings().detour_cost_markers_enabled)
    return;

  const TaskStats &task_stats = Calculated().task_stats;
  const ElementStat &current_leg = task_stats.current_leg;

  if (!task_stats.task_valid || !current_leg.location_remaining.IsValid())
    return;

  const GeoPoint target = current_leg.location_remaining;
  GeoVector vec(Basic().location, target);

  if ((Basic().track - vec.bearing).AsDelta().Absolute() < Angle::Degrees(10))
    // insignificant error
    return;

  auto distance_max =
    std::min(vec.distance,
             render_projection.GetScreenDistanceMeters() * 0.7);

  // too short to bother
  if (distance_max < 5000)
    return;

  GeoPoint start = Basic().location;
  
  canvas.Select(*look.overlay.overlay_font);
  canvas.SetTextColor(COLOR_BLACK);
  canvas.SetBackgroundTransparent();
  
  GeoPoint dloc;
  int ilast = 0;
  for (double d = 0.25; d <= 1; d += 0.25) {
    dloc = FindLatitudeLongitude(start, Basic().track, distance_max * d);
    
    double distance0 = start.DistanceS(dloc);
    double distance1 = target.DistanceS(dloc);
    double distance = (distance0 + distance1) / vec.distance;
    int idist = iround((distance - 1) * 100);
    
    if ((idist != ilast) && (idist > 0) && (idist < 1000)) {
      TCHAR Buffer[5];
      _stprintf(Buffer, _T("%d"), idist);
      auto sc = render_projection.GeoToScreen(dloc);
      PixelSize tsize = canvas.CalcTextSize(Buffer);
      canvas.DrawText(sc - tsize / 2u, Buffer);
      ilast = idist;
    }
  }
}
