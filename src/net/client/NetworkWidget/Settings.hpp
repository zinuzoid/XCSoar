// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"

#include <chrono>

struct NetworkWidgetSettings
{
  static constexpr unsigned NETWORK_WIDGET_SLOTS = 2;

  std::chrono::duration<unsigned> interval;
  StaticString<256> urls[NETWORK_WIDGET_SLOTS];

  void SetDefaults()
  {
    interval = std::chrono::seconds(60);
    for (auto &u : urls)
      u.clear();
  }
};
