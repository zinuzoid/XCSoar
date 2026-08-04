// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/dim/BulkPoint.hpp"
#include "Look/WindBarbLook.hpp"

class Canvas;
class Angle;

/**
 * WMO wind barb glyph for a wind station: a station circle, a staff
 * pointing towards the direction the wind blows FROM, and
 * barbs/pennants encoding speed in 5 kt steps (half barb = 5 kt, full
 * barb = 10 kt, pennant = 50 kt).
 *
 * The WMO calm ring is not used: a bare circle on a moving map is too
 * easily read as a waypoint or an airspace boundary.  Calm is instead
 * the staff with no barbs on it, so every wind station keeps the same
 * circle-and-staff silhouette whatever the wind is doing.
 */
namespace WindBarb {

static constexpr unsigned MAX_POINTS = 32;

struct Barb {
  /** consecutive pairs of points, each pair one line segment */
  BulkPixelPoint seg[MAX_POINTS];
  unsigned n_seg = 0;

  /** consecutive triples of points, each triple one filled pennant */
  BulkPixelPoint pen[MAX_POINTS];
  unsigned n_pen = 0;

  /**
   * The y coordinate of the staff tip, in the same unscaled,
   * unrotated template space as #seg and #pen (+x right, +y down,
   * staff pointing towards -y).  Lets a caller position a label past
   * the tip without redoing the speed-dependent length calculation.
   */
  int tip_y = 0;
};

/**
 * Build the barb template for a wind speed in knots, as unscaled
 * pixel offsets from the origin.  Speed is rounded to the nearest
 * 5 kt, the finest the symbol can express; below 3 kt this rounds to
 * zero and the result is a bare staff - still clearly a wind station,
 * but claiming no measurable wind.
 */
void
Build(Barb &b, unsigned speed_kt) noexcept;

} // namespace WindBarb

/**
 * Draws a #WindBarb::Barb at a screen position, scaled and rotated to
 * the meteorological wind bearing.  Mirrors WindArrowRenderer's
 * calling convention: construct with the #WindBarbLook::Band to draw
 * with, then call Draw() once per station.
 */
class WindBarbRenderer {
  const WindBarbLook::Band &look;

public:
  explicit WindBarbRenderer(const WindBarbLook::Band &_look) noexcept
    :look(_look) {}

  /**
   * Builds and draws the barb.  Returns the built geometry so the
   * caller can position an adjoining label past WindBarb::Barb::tip_y
   * without redoing the staff-length calculation itself.
   *
   * @param at station position on screen
   * @param wind_from meteorological wind bearing (direction the wind
   * blows FROM); on a rotated map pass
   * wind.bearing - projection.GetScreenAngle()
   * @param speed_kt wind speed in knots
   */
  WindBarb::Barb Draw(Canvas &canvas, PixelPoint at, Angle wind_from,
                      unsigned speed_kt) const noexcept;
};
