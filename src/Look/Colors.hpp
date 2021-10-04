// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Color.hpp"

#ifdef XCSOAR_TESTING
static constexpr Color COLOR_XCSOAR_LIGHT = Color(0x57, 0xff, 0xd8);
static constexpr Color COLOR_XCSOAR = Color(0x00, 0xf0, 0xb8);
static constexpr Color COLOR_XCSOAR_DARK = Color(0x00, 0xa3, 0x7d);
// New palette 00f0b8,ff3d51,ffe97a,004be0,8d85ff
#else
static constexpr Color COLOR_XCSOAR_LIGHT = Color(0xaa, 0xc9, 0xe4);
static constexpr Color COLOR_XCSOAR = Color(0x3f, 0x76, 0xa8);
static constexpr Color COLOR_XCSOAR_DARK = Color(0x00, 0x31, 0x5e);
#endif

static constexpr uint8_t ALPHA_OVERLAY = 0xA0;
