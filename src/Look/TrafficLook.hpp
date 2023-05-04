// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Color.hpp"
#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Icon.hpp"

class Font;

struct TrafficLook {
  static constexpr Color safe_above_color{0x1d,0x9b,0xc5};
  static constexpr Color safe_below_color{0x1d,0xc5,0x10};
  static constexpr Color warning_color{0xfe,0x84,0x38};
  static constexpr Color warning_in_altitude_range_color{0xff,0x00,0xff};
  static constexpr Color alarm_color{0xfb,0x35,0x2f};
  static constexpr Color offline_color{0x00,0x00,0x00};

  Brush safe_above_brush;
  Brush safe_below_brush;
  Brush warning_brush;
  Brush warning_in_altitude_range_brush;
  Brush alarm_brush;
  Brush offline_brush;

  static constexpr Color team_color_green = Color(0x74, 0xff, 0);
  static constexpr Color team_color_magenta = Color(0xff, 0, 0xcb);
  static constexpr Color team_color_blue = Color(0, 0x90, 0xff);
  static constexpr Color team_color_yellow = Color(0xff, 0xe8, 0);

  Pen team_pen_green;
  Pen team_pen_blue;
  Pen team_pen_yellow;
  Pen team_pen_magenta;

  MaskedIcon teammate_icon;

  const Font *font;

  void Initialise(const Font &font);
};
