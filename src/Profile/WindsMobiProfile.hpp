// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Map.hpp"
#include "Weather/WindsMobi/Settings.hpp"

namespace Profile
{
void Load(const ProfileMap &map, WindStationSettings &settings);
};
