// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Color.hpp"

/**
 * Pen and brush for one #WindBarbRenderer colour band: the pen draws
 * the station circle, staff and barb lines; the brush fills the
 * pennants.
 */
struct WindBarbLook {
  Pen pen;
  Brush brush;

  void Initialise(Color color) noexcept;
};
