/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2021 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef JET_PROVIDER_TRACE_PARSER_HPP
#define JET_PROVIDER_TRACE_PARSER_HPP

#include "Geo/GeoPoint.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace JETProvider
{

/**
 * The flight trace of one pilot, as returned by the /api/2/trace
 * endpoint.
 *
 * The server currently sends geographic positions only.  When it starts
 * sending altitude and time as well, add them here as parallel arrays and
 * the renderer can colour the trace by climb rate.
 */
struct PilotTrace {
  std::string id;
  std::vector<GeoPoint> points;
};

/**
 * Turn the configured pilot id list into the comma separated value the
 * request needs, dropping blanks and surrounding whitespace.
 */
std::string
JoinPilotIds(std::string_view pilot_ids) noexcept;

/**
 * Parse a trace response body.
 *
 * The body holds two lines per pilot: a name/id, then that pilot's
 * encoded polyline.  The polyline line is empty for a pilot that has no
 * trace yet.  Pilots without any point are not added.
 *
 * @param traces receives the traces, keyed by the name/id line
 * @return false if the body was malformed, in which case #traces still
 * holds every record that could be read
 */
bool
ParseTraceResponse(std::string_view body,
                   std::map<std::string, PilotTrace> &traces) noexcept;

}

#endif
