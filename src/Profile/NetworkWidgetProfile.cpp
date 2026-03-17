// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "NetworkWidgetProfile.hpp"
#include "Keys.hpp"

void
Profile::Load(const ProfileMap &map, NetworkWidgetSettings &settings)
{
  map.Get(ProfileKeys::NetworkWidgetInterval, settings.interval);
  map.Get(ProfileKeys::NetworkWidgetUrl, settings.urls[0]);
  map.Get(ProfileKeys::NetworkWidgetUrl2, settings.urls[1]);
}
