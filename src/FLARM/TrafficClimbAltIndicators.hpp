// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "FLARM/Traffic.hpp"
#include <cstdint>

class TrafficClimbAltIndicators
{
public:
  TrafficClimbAltIndicators() noexcept
    : climb(Climb::DOWN),
      rel_alt(RelAlt::SAME)
  {}

  static TrafficClimbAltIndicators
  GetClimbAltIndicators(const FlarmTraffic &traffic) noexcept;

  static TrafficClimbAltIndicators
  GetClimbAltIndicators(const FlarmTraffic &traffic, double set_mc,
                        double current_30s_vario) noexcept;

  uint8_t get_climb_indicator() const noexcept { return (uint8_t)climb; }
  uint8_t get_rel_alt_indicator() const noexcept { return (uint8_t)rel_alt; }

  enum class Climb : uint8_t {
    GOOD,
    UP,
    DOWN,
  };

  enum class RelAlt : uint8_t {
    ABOVE,
    SAME,
    BELOW,
  };

private:
  Climb climb;
  RelAlt rel_alt;

  static constexpr int similar_altitude_threshold_meters = 50;
};
