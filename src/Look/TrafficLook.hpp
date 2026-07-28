// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Color.hpp"
#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Icon.hpp"
#include "FLARM/TrafficClimbAltIndicators.hpp"

class Font;

struct TrafficLook
{
  static constexpr uint8_t num_alt_based_colour_options = 3;
  static constexpr uint8_t num_vario_based_colour_options = 3;

  struct vario_traffic_colors
  {
    struct above
    {
      static constexpr Color climb_good = {0xff, 0x66, 0xff}; // light pink
      static constexpr Color climb_up   = {0xff, 0xff, 0x66}; // light yellow
      static constexpr Color climb_down = {0x66, 0x66, 0xff}; // light blue
    };

    struct same
    {
      static constexpr Color climb_good = {0xff, 0x00, 0xff}; // pink
      static constexpr Color climb_up   = {0xff, 0xff, 0x00}; // yellow
      static constexpr Color climb_down = {0x00, 0x00, 0xff}; // blue
    };

    struct below
    {
      static constexpr Color climb_good = {0x99, 0x00, 0x66}; // dark pink
      static constexpr Color climb_up   = {0x99, 0x99, 0x00}; // dark yellow
      static constexpr Color climb_down = {0x00, 0x00, 0x99}; // dark blue
    };
  };

  struct TrafficLookColor
  {
    static constexpr Color above = {0x1d, 0x9b, 0xc5};
    static constexpr Color same  = {0xff, 0x00, 0xff};
    static constexpr Color below = {0x1d, 0xc5, 0x10};
  };

  static constexpr Color warning_color{0xfe,0x84,0x38};
  static constexpr Color warning_in_altitude_range_color{0xff,0x00,0xff};
  static constexpr Color alarm_color{0xfb,0x35,0x2f};
  static constexpr Color offline_color{0x00,0x00,0x00};

  struct basic_traffic_brushes_t
  {
    Brush above;
    Brush same;
    Brush below;
  } basic_traffic_brushes;

  typedef struct Climb_Indication_s {
    Brush climb_good;
    Brush climb_up;
    Brush climb_down;
  } Climb_Indication_t;

  struct vario_traffic_brushes_t
  {
    Climb_Indication_t above;
    Climb_Indication_t same;
    Climb_Indication_t below;
  } vario_traffic_brushes;

  Brush warning_brush;
  Brush alarm_brush;
  Brush offline_brush;

  static constexpr Color fading_outline_color = ColorWithAlpha({0x60, 0x60, 0x60}, 0xa0);
  Pen fading_pen;

#ifdef ENABLE_OPENGL
  static constexpr Color fading_fill_color = ColorWithAlpha({0xc0, 0xc0, 0xc0}, 0x60);
  Brush fading_brush;
#endif

  static constexpr Color team_color_green = Color(0x74, 0xff, 0);
  static constexpr Color team_color_magenta = Color(0xff, 0, 0xcb);
  static constexpr Color team_color_blue = Color(0, 0x90, 0xff);
  static constexpr Color team_color_yellow = Color(0xff, 0xe8, 0);

  Pen team_pen_green;
  Pen team_pen_blue;
  Pen team_pen_yellow;
  Pen team_pen_magenta;

  /**
   * Pens for the live flight traces of other pilots; one per followed
   * pilot, cycled so neighbouring traces are told apart.
   */
  static constexpr unsigned NUM_TRACE_PENS = 12;
  static constexpr Color trace_colors[NUM_TRACE_PENS] = {
    Color(0x74, 0xff, 0x00), // green
    Color(0x00, 0x90, 0xff), // blue
    Color(0xff, 0x00, 0xcb), // magenta
    Color(0xff, 0xe8, 0x00), // yellow
    Color(0xff, 0x30, 0x30), // red
    Color(0x00, 0xe5, 0xd0), // cyan
    Color(0xff, 0x8c, 0x00), // orange
    Color(0x9d, 0x4e, 0xff), // purple
    Color(0x00, 0x94, 0x7a), // teal
    Color(0xff, 0x8f, 0xb0), // pink
    Color(0x9b, 0x63, 0x2a), // brown
    Color(0x28, 0x3c, 0xc8), // navy
  };
  Pen trace_pens[NUM_TRACE_PENS];

  MaskedIcon teammate_icon;

  const Font *font;

  void Initialise(const Font &font);

  Brush GetBasicTrafficBrush(const TrafficClimbAltIndicators &indicators) const noexcept;
  Brush GetVarioTrafficBrush(const TrafficClimbAltIndicators &indicators) const noexcept;
};
