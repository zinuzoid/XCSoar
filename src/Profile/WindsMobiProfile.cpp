// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindsMobiProfile.hpp"
#include "Keys.hpp"

void
Profile::Load(const ProfileMap &map, WindStationSettings &settings)
{
  map.Get(ProfileKeys::WindStationsEnabled, settings.enabled);
}
