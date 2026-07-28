// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "FLARM/Traffic.hpp"
#include "FLARM/Color.hpp"

namespace JETProvider
{

/**
 * The visual state of a target, decoded from
 * JETProvider::Traffic::icon_type.
 */
struct TrafficIconState {
  FlarmTraffic::AlarmType alarm_level = FlarmTraffic::AlarmType::NONE;
  FlarmColor circle = FlarmColor::NONE;
};

/**
 * Decode the packed colour nibbles of
 * JETProvider::Traffic::icon_type.
 *
 * @param online false if the radar feed is stale or the last request
 * failed; such targets are drawn as AlarmType::OFFLINE
 */
constexpr TrafficIconState
DecodeIconType(int icon_type, bool online) noexcept
{
  TrafficIconState state;

  if (!online) {
    state.alarm_level = FlarmTraffic::AlarmType::OFFLINE;
    return state;
  }

  switch (icon_type & 0xf) {
  case 2:
    state.alarm_level = FlarmTraffic::AlarmType::LOW;
    break;

  case 3:
    state.alarm_level = FlarmTraffic::AlarmType::URGENT;
    break;
  }

  switch (icon_type >> 8 & 0xf) {
  case 2:
    state.circle = FlarmColor::GREEN;
    break;

  case 3:
    state.circle = FlarmColor::BLUE;
    break;

  case 4:
    state.circle = FlarmColor::YELLOW;
    break;

  case 5:
    state.circle = FlarmColor::MAGENTA;
    break;
  }

  return state;
}

} // namespace JETProvider
