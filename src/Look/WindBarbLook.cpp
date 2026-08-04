// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindBarbLook.hpp"
#include "Screen/Layout.hpp"
#include "Look/Colors.hpp"

namespace {

/** gust bands: calm, moderate, strong, dangerous */
constexpr Color band_colors[WindBarbLook::N_BANDS] = {
  Color(0x2e, 0xa0, 0x2e), // green
  Color(0xd9, 0xa0, 0x00), // amber
  Color(0xff, 0x6a, 0x00), // orange
  Color(0xe0, 0x20, 0x20), // red
};

} // namespace

void
WindBarbLook::Band::Initialise(Color color) noexcept
{
  pen.Create(Layout::ScalePenWidth(1), color);
  brush.Create(ColorWithAlpha(color, ALPHA_OVERLAY));
}

void
WindBarbLook::Initialise(const Font &_font)
{
  font = &_font;

  for (unsigned i = 0; i < N_BANDS; ++i) {
    const Color color = band_colors[i];
    bands[i].Initialise(color);
    stale_bands[i].Initialise(Desaturate(LightColor(color)));
  }
}
