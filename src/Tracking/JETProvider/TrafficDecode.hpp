// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "FLARM/Traffic.hpp"
#include "FLARM/Color.hpp"
#include "util/NumberParser.hxx"

#include <optional>

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

/**
 * Parse JETProvider::Traffic::type, the OGN/FLARM aircraft type code
 * as a decimal string.
 *
 * @return the decoded type, or std::nullopt if the value is empty or
 * is not a decimal number
 */
[[gnu::pure]]
inline std::optional<FlarmTraffic::AircraftType>
ParseAircraftType(const char *type) noexcept
{
  if (type == nullptr)
    return std::nullopt;

  const auto value = ParseInteger<uint8_t>(type);
  if (!value)
    return std::nullopt;

  return (FlarmTraffic::AircraftType)*value;
}

/**
 * Decode JETProvider::Traffic::type, the OGN/FLARM aircraft type code
 * as a decimal string.
 *
 * @return the type name, or nullptr if the value is empty, is not a
 * decimal number, or is not a known type code
 */
[[gnu::pure]]
inline const TCHAR *
DecodeAircraftType(const char *type) noexcept
{
  const auto value = ParseAircraftType(type);
  if (!value)
    return nullptr;

  /* GetTypeString() returns nullptr for codes outside the table */
  return FlarmTraffic::GetTypeString(*value);
}

} // namespace JETProvider
