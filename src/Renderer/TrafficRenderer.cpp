// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TrafficRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Look/TrafficLook.hpp"
#include "FLARM/Traffic.hpp"
#include "GliderLink/Traffic.hpp"
#include "Math/Screen.hpp"
#include "util/Macros.hpp"
#include "Asset.hpp"

#include <span>

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

void
TrafficRenderer::Draw(Canvas &canvas, const TrafficLook &traffic_look,
                      bool fading, bool vario_traffic,
                      const FlarmTraffic &traffic, const Angle angle,
                      const FlarmColor color, const PixelPoint pt,
                      const TrafficClimbAltIndicators indicators) noexcept
{
  // Create point array that will form that arrow polygon
  BulkPixelPoint arrow[] = {
    { -4, 6 },
    { 0, -8 },
    { 4, 6 },
    { 0, 3 },
  };

  // Create point array that will form the paraglider wing polygon
  BulkPixelPoint wing[] = {
    { -9, 2 }, { -5, -3 }, { 0, -5 }, { 5, -3 }, { 9, 2 },
    {  6, 2 }, {  0, -1 }, { -5, 2 },
  };

  // Create point array that will form the sailplane polygon
  BulkPixelPoint sailplane_ar15[] = {
    {   0, -7 }, {   1, -3 }, {   9, -2 }, {  17, -1 }, { 17,  0 },
    {   9,  0 }, {   1,  1 }, {   1,  7 }, {   5,  8 }, {  5,  9 },
    {   1,  9 },
    {  -1,  9 }, {  -5,  9 }, {  -5,  8 }, {  -1,  7 }, { -1,  1 },
    {  -9,  0 }, { -17,  0 }, { -17, -1 }, {  -9, -2 }, { -1, -3 },
  };

  std::span<BulkPixelPoint> shape;
  switch (traffic.type) {
  case FlarmTraffic::AircraftType::PARA_GLIDER:
    shape = wing;
    break;

  case FlarmTraffic::AircraftType::GLIDER:
    shape = sailplane_ar15;
    break;

  default:
    shape = arrow;
    break;
  }

  // Rotate and shift the shape to the right position and angle
  PolygonRotateShift(shape, pt, angle, Layout::Scale(100U));

  if (fading) {
    canvas.Select(traffic_look.fading_pen);

#ifdef ENABLE_OPENGL
    canvas.Select(traffic_look.fading_brush);
#else
    /* we have no alpha blending - don't fill the shape */
    canvas.SelectHollowBrush();
#endif

    // Draw the shape
#ifdef ENABLE_OPENGL
    const ScopeAlphaBlend alpha_blend;
#endif
    canvas.DrawPolygon(shape.data(), shape.size());
  } else {
    // Select brush depending on AlarmLevel
    switch (traffic.alarm_level) {
    case FlarmTraffic::AlarmType::LOW:
    case FlarmTraffic::AlarmType::INFO_ALERT:
      canvas.Select(traffic_look.warning_brush);
      break;
    case FlarmTraffic::AlarmType::IMPORTANT:
    case FlarmTraffic::AlarmType::URGENT:
      canvas.Select(traffic_look.alarm_brush);
      break;
    case FlarmTraffic::AlarmType::NONE:
      canvas.Select(vario_traffic
        ? traffic_look.GetVarioTrafficBrush(indicators)
        : traffic_look.GetBasicTrafficBrush(indicators));
      break;
    case FlarmTraffic::AlarmType::OFFLINE:
      canvas.Select(traffic_look.offline_brush);
      break;
    }

    // Select black pen
    canvas.SelectBlackPen();

    // Draw the shape
    canvas.DrawPolygon(shape.data(), shape.size());
  }

  switch (color) {
  case FlarmColor::GREEN:
    canvas.Select(traffic_look.team_pen_green);
    break;
  case FlarmColor::BLUE:
    canvas.Select(traffic_look.team_pen_blue);
    break;
  case FlarmColor::YELLOW:
    canvas.Select(traffic_look.team_pen_yellow);
    break;
  case FlarmColor::MAGENTA:
    canvas.Select(traffic_look.team_pen_magenta);
    break;
  default:
    return;
  }

  canvas.SelectHollowBrush();
  canvas.DrawCircle(pt, Layout::FastScale(11u));
}



void
TrafficRenderer::Draw(Canvas &canvas, const TrafficLook &traffic_look,
                      [[maybe_unused]] const GliderLinkTraffic &traffic,
                      const Angle angle, const PixelPoint pt) noexcept
{
  // Create point array that will form that arrow polygon
  BulkPixelPoint arrow[] = {
    { -4, 6 },
    { 0, -8 },
    { 4, 6 },
    { 0, 3 },
  };

  canvas.Select(traffic_look.basic_traffic_brushes.above);

  // Select black pen
  if (IsDithered())
    canvas.Select(Pen(Layout::FastScale(2), COLOR_BLACK));
  else
    canvas.SelectBlackPen();

  // Rotate and shift the arrow to the right position and angle
  PolygonRotateShift(arrow, pt, angle, Layout::Scale(100U));

  // Draw the arrow
  canvas.DrawPolygon(arrow, ARRAY_SIZE(arrow));

  canvas.SelectHollowBrush();
  canvas.DrawCircle(pt, Layout::FastScale(11u));
}
