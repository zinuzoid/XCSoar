// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindBarbRenderer.hpp"
#include "Look/WindBarbLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Math/Angle.hpp"
#include "Math/Screen.hpp"
#include "Screen/Layout.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#include <algorithm>

namespace WindBarb {

namespace {

/* template geometry, unscaled pixels */
constexpr int CIRCLE_R  = 3;   // station circle radius
constexpr int STAFF_MIN = 15;  // staff length outside the circle
constexpr int SLOT      = 3;   // spacing between barbs
constexpr int PENNANT   = 4;   // staff consumed by one pennant
constexpr int FULL_DX   = 6, FULL_DY = 3;
constexpr int HALF_DX   = 3, HALF_DY = 2;

} // namespace

void
Build(Barb &b, unsigned speed_kt) noexcept
{
  b = Barb();

  const unsigned speed = ((speed_kt + 2) / 5) * 5;

  const unsigned pennants = speed / 50;
  unsigned rem            = speed % 50;
  const unsigned fulls    = rem / 10;
  const unsigned halves   = (rem % 10) / 5;

  /* grow the staff only if the feathers do not fit the default length */
  const int used = int(pennants) * (PENNANT + 1)
                 + int(fulls + halves) * SLOT;
  const int staff = std::max(STAFF_MIN, used);
  const int tip   = -(CIRCLE_R + staff);

  b.tip_y = tip;

  int y = tip;

  /* convention: a lone half barb is set in one slot from the tip, so it
     cannot be mistaken for a full barb that failed to draw */
  if (pennants == 0 && fulls == 0 && halves == 1)
    y += SLOT;

  for (unsigned i = 0; i < pennants; ++i) {
    b.pen[b.n_pen++] = { 0,           y };
    b.pen[b.n_pen++] = { FULL_DX + 1, y + PENNANT / 2 };
    b.pen[b.n_pen++] = { 0,           y + PENNANT };
    y += PENNANT + 1;
  }

  for (unsigned i = 0; i < fulls; ++i) {
    b.seg[b.n_seg++] = { 0,       y };
    b.seg[b.n_seg++] = { FULL_DX, y + FULL_DY };
    y += SLOT;
  }

  for (unsigned i = 0; i < halves; ++i) {
    b.seg[b.n_seg++] = { 0,       y };
    b.seg[b.n_seg++] = { HALF_DX, y + HALF_DY };
    y += SLOT;
  }

  /* the staff */
  b.seg[b.n_seg++] = { 0, tip };
  b.seg[b.n_seg++] = { 0, -CIRCLE_R };
}

/**
 * Assumes the desired pen and (for the pennants) brush are already
 * selected on #canvas.
 */
static void
Draw(Canvas &canvas, const Barb &b, PixelPoint at, Angle wind_from) noexcept
{
  /* both pen and brush carry alpha (see WindBarbLook::Band::Initialise);
     without this, GL_BLEND is off by default and the alpha channel is
     silently ignored, rendering everything fully opaque */
#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
#endif

  const int r = Layout::Scale(CIRCLE_R);

  canvas.DrawCircle(at, r);

  BulkPixelPoint pts[MAX_POINTS];

  std::copy_n(b.seg, b.n_seg, pts);
  PolygonRotateShift({pts, b.n_seg}, at, wind_from, Layout::Scale(100));
  for (unsigned i = 0; i + 1 < b.n_seg; i += 2)
    canvas.DrawLine(pts[i], pts[i + 1]);

  if (b.n_pen > 0) {
    std::copy_n(b.pen, b.n_pen, pts);
    PolygonRotateShift({pts, b.n_pen}, at, wind_from, Layout::Scale(100));
    for (unsigned i = 0; i + 2 < b.n_pen; i += 3)
      canvas.DrawTriangleFan(pts + i, 3);
  }
}

} // namespace WindBarb

WindBarb::Barb
WindBarbRenderer::Draw(Canvas &canvas, PixelPoint at, Angle wind_from,
                       unsigned speed_kt) const noexcept
{
  /* guard the fixed-size Barb arrays: the WMO symbol was never meant
     to encode anything beyond a handful of pennants, and MAX_POINTS
     assumes so */
  speed_kt = std::min(speed_kt, 300u);

  WindBarb::Barb barb;
  WindBarb::Build(barb, speed_kt);

  canvas.Select(look.pen);
  canvas.Select(look.brush);
  WindBarb::Draw(canvas, barb, at, wind_from);

  return barb;
}
