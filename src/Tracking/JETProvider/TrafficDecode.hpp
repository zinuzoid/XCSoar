// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "FLARM/Id.hpp"
#include "FLARM/Traffic.hpp"
#include "FLARM/Color.hpp"
#include "util/CharUtil.hxx"
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

/**
 * Parse a JETProvider traffic_id (hex device address string) into
 * a FlarmId for de-duplication against FLARM traffic.
 *
 * The validation is deliberately strict: FlarmId::Parse() is a bare
 * strtol(input, endptr, 16) that accepts leading whitespace, a sign,
 * an 0x prefix, and saturates on overflow — every one of those is a
 * false positive that would silently delete a real aircraft from the
 * map.  This function requires 1–8 pure hex digits and nothing else.
 *
 * OGN-prefixed ids (e.g. "FLRDDA5BA") contain non-hex characters and
 * safely degrade to FlarmId::Undefined() — today's no-dedup behaviour.
 *
 * @return the parsed FlarmId, or FlarmId::Undefined() on any failure
 */
[[gnu::pure]]
inline FlarmId
ParseTrafficId(const char *traffic_id) noexcept
{
  if (traffic_id == nullptr)
    return FlarmId::Undefined();

  /* count characters; require 1–8 hex digits, nothing else */
  unsigned len = 0;
  for (const char *p = traffic_id; *p != '\0'; ++p) {
    if (!IsHexDigit(*p))
      return FlarmId::Undefined();
    ++len;
  }

  if (len == 0 || len > 8)
    return FlarmId::Undefined();

  char *endptr;
  FlarmId id = FlarmId::Parse(traffic_id, &endptr);

  /* belt-and-suspenders: the whole string must have been consumed */
  if (*endptr != '\0')
    return FlarmId::Undefined();

  return id;
}

} // namespace JETProvider
