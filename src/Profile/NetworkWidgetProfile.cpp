// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "NetworkWidgetProfile.hpp"
#include "Keys.hpp"

#include <string_view>

/**
 * Profile keys for each slot's URL, indexed by slot.  Keep in sync with
 * NetworkWidgetSettings::NETWORK_WIDGET_SLOTS.
 */
static constexpr std::string_view network_widget_url_keys[] = {
  ProfileKeys::NetworkWidgetUrl,
  ProfileKeys::NetworkWidgetUrl2,
};

static_assert(std::size(network_widget_url_keys) ==
                  NetworkWidgetSettings::NETWORK_WIDGET_SLOTS,
              "NetworkWidget URL profile keys out of sync with slot count");

void
Profile::Load(const ProfileMap &map, NetworkWidgetSettings &settings)
{
  map.Get(ProfileKeys::NetworkWidgetInterval, settings.interval);

  for (unsigned i = 0; i < NetworkWidgetSettings::NETWORK_WIDGET_SLOTS; ++i)
    map.Get(network_widget_url_keys[i], settings.urls[i]);
}
