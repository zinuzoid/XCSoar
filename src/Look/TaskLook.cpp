// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TaskLook.hpp"
#include "Screen/Layout.hpp"
#include "Resources.hpp"
#include "Colors.hpp"
#include "Asset.hpp"

void
TaskLook::Initialise()
{
  // Magenta ICAO color is 0x65,0x23,0x1c
  const Color task_color = Color(0x62, 0x4e, 0x90);
  const Color bearing_color = Color(0x3e, 0x30, 0x5f);
  const Color isoline_color = bearing_color;

  oz_current_pen.Create(Pen::SOLID, Layout::ScalePenWidth(2), task_color);
  oz_active_pen.Create(Pen::SOLID, Layout::ScalePenWidth(1), task_color);
  oz_inactive_pen.Create(Pen::SOLID, Layout::ScalePenWidth(1),
                      DarkColor(task_color));

  leg_active_pen.Create(Pen::DASH2, Layout::ScalePenWidth(2), task_color);
  leg_inactive_pen.Create(Pen::DASH2, Layout::ScalePenWidth(1), task_color);
  arrow_active_pen.Create(Layout::ScalePenWidth(2), task_color);
  arrow_inactive_pen.Create(Layout::ScalePenWidth(1), task_color);

  isoline_pen.Create(Pen::DASH2, Layout::ScalePenWidth(1), isoline_color);

  bearing_pen.Create(Layout::ScalePenWidth(2),
                  HasColors() ? bearing_color : COLOR_BLACK);
  best_cruise_track_brush.Create(ColorWithAlpha(bearing_color, ALPHA_OVERLAY));
  best_cruise_track_pen.Create(Layout::ScalePenWidth(1),
                               HasColors()
                               ? DarkColor(bearing_color)
                               : COLOR_BLACK);

  highlight_pen.Create(Layout::ScalePenWidth(4), COLOR_BLACK);

  fuel_range_pen.Create(Layout::ScalePenWidth(2), COLOR_WHITE);
  fuel_range_brush.Create(ColorWithAlpha(COLOR_WHITE, 0));

  target_icon.LoadResource(IDB_TARGET_ALL);

  hbGray.Create(COLOR_GRAY);
  hbGreen.Create(COLOR_GREEN);
  hbOrange.Create(COLOR_ORANGE);
  hbLightGray.Create(COLOR_LIGHT_GRAY);
  hbNotReachableTerrain.Create(LightColor(COLOR_RED));
}
