// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindBarbLook.hpp"
#include "Screen/Layout.hpp"

void
WindBarbLook::Initialise(Color color) noexcept
{
  pen.Create(Layout::ScalePenWidth(1), color);
  brush.Create(color);
}
