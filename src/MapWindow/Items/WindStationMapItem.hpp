// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "MapItem.hpp"
#include "Tracking/JETProvider/WindParser.hpp"

/**
 * A #MapItem describing a wind station.  Holds a
 * #JETProvider::WindStation by value, because the station vector inside
 * #JETProvider::WindData is replaced wholesale on every poll.
 */
struct WindStationMapItem : public MapItem
{
  JETProvider::WindStation station;

  explicit WindStationMapItem(const JETProvider::WindStation &_station) noexcept
    :MapItem(Type::WIND_STATION), station(_station) {}
};
