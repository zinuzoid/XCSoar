// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TrafficLook.hpp"
#include "Colors.hpp"
#include "Screen/Layout.hpp"
#include "Resources.hpp"

constexpr Color TrafficLook::team_color_green;
constexpr Color TrafficLook::team_color_magenta;
constexpr Color TrafficLook::team_color_blue;
constexpr Color TrafficLook::team_color_yellow;

void
TrafficLook::Initialise(const Font &_font)
{
  vario_traffic_brushes.above.climb_good.Create(vario_traffic_colors::above::climb_good);
  vario_traffic_brushes.above.climb_up.Create(vario_traffic_colors::above::climb_up);
  vario_traffic_brushes.above.climb_down.Create(vario_traffic_colors::above::climb_down);

  vario_traffic_brushes.same.climb_good.Create(vario_traffic_colors::same::climb_good);
  vario_traffic_brushes.same.climb_up.Create(vario_traffic_colors::same::climb_up);
  vario_traffic_brushes.same.climb_down.Create(vario_traffic_colors::same::climb_down);

  vario_traffic_brushes.below.climb_good.Create(vario_traffic_colors::below::climb_good);
  vario_traffic_brushes.below.climb_up.Create(vario_traffic_colors::below::climb_up);
  vario_traffic_brushes.below.climb_down.Create(vario_traffic_colors::below::climb_down);

  basic_traffic_brushes.above.Create(TrafficLookColor::above);
  basic_traffic_brushes.same.Create(TrafficLookColor::same);
  basic_traffic_brushes.below.Create(TrafficLookColor::below);

  warning_brush.Create(warning_color);
  alarm_brush.Create(alarm_color);
  offline_brush.Create(offline_color);

  fading_pen.Create(Pen::Style::DASH1, Layout::ScalePenWidth(1), fading_outline_color);

#ifdef ENABLE_OPENGL
  fading_brush.Create(fading_fill_color);
#endif

  unsigned width = Layout::ScalePenWidth(1);
  team_pen_green.Create(width, team_color_green);
  team_pen_blue.Create(width, team_color_blue);
  team_pen_yellow.Create(width, team_color_yellow);
  team_pen_magenta.Create(width, team_color_magenta);

  unsigned trace_width = Layout::ScalePenWidth(2);
  for (unsigned i = 0; i < NUM_TRACE_PENS; ++i)
    trace_pens[i].Create(trace_width, ColorWithAlpha(trace_colors[i], ALPHA_OVERLAY));

  teammate_icon.LoadResource(IDB_TEAMMATE_POS_ALL);

  font = &_font;
}

Brush
TrafficLook::GetBasicTrafficBrush(const TrafficClimbAltIndicators &indicators) const noexcept
{
  switch ((TrafficClimbAltIndicators::RelAlt)indicators.get_rel_alt_indicator()) {
  case TrafficClimbAltIndicators::RelAlt::ABOVE:
    return basic_traffic_brushes.above;
  case TrafficClimbAltIndicators::RelAlt::SAME:
    return basic_traffic_brushes.same;
  case TrafficClimbAltIndicators::RelAlt::BELOW:
    return basic_traffic_brushes.below;
  default:
    return basic_traffic_brushes.same;
  }
}

Brush
TrafficLook::GetVarioTrafficBrush(const TrafficClimbAltIndicators &indicators) const noexcept
{
  const Climb_Indication_t &climb_brushes =
    ((TrafficClimbAltIndicators::RelAlt)indicators.get_rel_alt_indicator() == TrafficClimbAltIndicators::RelAlt::ABOVE)
      ? vario_traffic_brushes.above
      : ((TrafficClimbAltIndicators::RelAlt)indicators.get_rel_alt_indicator() == TrafficClimbAltIndicators::RelAlt::BELOW)
          ? vario_traffic_brushes.below
          : vario_traffic_brushes.same;

  switch ((TrafficClimbAltIndicators::Climb)indicators.get_climb_indicator()) {
  case TrafficClimbAltIndicators::Climb::GOOD:
    return climb_brushes.climb_good;
  case TrafficClimbAltIndicators::Climb::UP:
    return climb_brushes.climb_up;
  case TrafficClimbAltIndicators::Climb::DOWN:
    return climb_brushes.climb_down;
  default:
    return climb_brushes.climb_down;
  }
}
