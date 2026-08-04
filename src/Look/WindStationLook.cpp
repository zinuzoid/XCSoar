// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindStationLook.hpp"
#include "Screen/Layout.hpp"
#include "Asset.hpp"
#include "ui/canvas/Color.hpp"
#include "Look/Colors.hpp"

namespace {

/** gust bands: calm, moderate, strong, dangerous */
constexpr Color band_colors[WindStationLook::N_BANDS] = {
  Color(0x2e, 0xa0, 0x2e), // green
  Color(0xd9, 0xa0, 0x00), // amber
  Color(0xff, 0x6a, 0x00), // orange
  Color(0xe0, 0x20, 0x20), // red
};

} // namespace

void
WindStationLook::Initialise(const Font &font)
{
  for (unsigned i = 0; i < N_BANDS; ++i) {
    WindArrowLook &look = bands[i];
    const Color color = band_colors[i];

    look.arrow_pen.Create(Layout::ScalePenWidth(1), DarkColor(color));
    look.shaft_pen.Create(Pen::DASH2, Layout::ScalePenWidth(1), color);
    look.arrow_brush.Create(IsDithered() ? color : ColorWithAlpha(color, ALPHA_OVERLAY));
    look.font = &font;

    WindArrowLook &stale = stale_bands[i];
    const Color stale_color = Desaturate(LightColor(color));

    stale.arrow_pen.Create(Layout::ScalePenWidth(1), DarkColor(stale_color));
    stale.shaft_pen.Create(Pen::DASH2, Layout::ScalePenWidth(1), stale_color);
    stale.arrow_brush.Create(IsDithered() ? stale_color : ColorWithAlpha(stale_color, ALPHA_OVERLAY));
    stale.font = &font;
  }
}
