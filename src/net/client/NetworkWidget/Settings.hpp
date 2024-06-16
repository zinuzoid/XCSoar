// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"

#include <chrono>

struct NetworkWidgetSettings
{

  std::chrono::duration<unsigned> interval;
  StaticString<256> url;

  void SetDefaults()
  {
    interval = std::chrono::seconds(60);
    url.clear();
  }
};
