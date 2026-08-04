// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <boost/json/fwd.hpp>

#include <vector>

namespace WindsMobi {

struct Station;

/**
 * Parse a winds.mobi "/api/2.3/stations/" JSON array response into a
 * list of #Station.  A station missing required fields (location,
 * last measurement) is silently skipped rather than failing the
 * whole batch, because the response can legitimately contain
 * stations without a recent measurement.
 */
std::vector<Station>
ParseStations(const boost::json::value &root);

} // namespace WindsMobi
