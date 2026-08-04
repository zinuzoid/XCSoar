// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "MapItem.hpp"
#include "Weather/WindsMobi/Station.hpp"

/**
 * A #MapItem describing a winds.mobi wind station.  Holds a
 * #WindsMobi::Station by value, because the station vector inside
 * WindsMobi::Glue is replaced wholesale on every poll.
 */
struct WindStationMapItem : public MapItem
{
  WindsMobi::Station station;

  explicit WindStationMapItem(const WindsMobi::Station &_station) noexcept
    :MapItem(Type::WIND_STATION), station(_station) {}
};
