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

#include "TraceParser.hpp"
#include "Task/PolylineDecoder.hpp"

namespace JETProvider {

static constexpr bool
IsWhitespace(char ch) noexcept
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

/**
 * The polyline decoder rejects every character below '?', so the line
 * terminators the body carries have to go first.
 */
static std::string_view
StripWhitespace(std::string_view src) noexcept
{
  while (!src.empty() && IsWhitespace(src.front()))
    src.remove_prefix(1);
  while (!src.empty() && IsWhitespace(src.back()))
    src.remove_suffix(1);
  return src;
}

/**
 * Consume the next line from #remaining.  Returns false once the buffer
 * is exhausted.  The line is returned without its terminator; a trailing
 * carriage return is left for StripWhitespace() to remove.
 */
static bool
NextLine(std::string_view &remaining, std::string_view &line) noexcept
{
  if (remaining.empty())
    return false;

  if (const auto nl = remaining.find('\n'); nl == std::string_view::npos) {
    line = remaining;
    remaining = {};
  } else {
    line = remaining.substr(0, nl);
    remaining = remaining.substr(nl + 1);
  }

  return true;
}

std::string
JoinPilotIds(std::string_view pilot_ids) noexcept
{
  std::string result;

  while (!pilot_ids.empty()) {
    std::string_view id;

    if (const auto comma = pilot_ids.find(','); comma == std::string_view::npos) {
      id = pilot_ids;
      pilot_ids = {};
    } else {
      id = pilot_ids.substr(0, comma);
      pilot_ids = pilot_ids.substr(comma + 1);
    }

    id = StripWhitespace(id);
    if (id.empty())
      continue;

    if (!result.empty())
      result.push_back(',');
    result.append(id);
  }

  return result;
}

bool
ParseTraceResponse(std::string_view body,
                   std::map<std::string, PilotTrace> &traces) noexcept
{
  bool success = true;

  while (true) {
    std::string_view name;

    /* tolerate blank lines between records and at the end of the body */
    do {
      if (!NextLine(body, name)) {
        name = {};
        break;
      }

      name = StripWhitespace(name);
    } while (name.empty());

    if (name.empty())
      /* end of body */
      break;

    std::string_view encoded;
    if (!NextLine(body, encoded))
      /* the last pilot's trace line is empty and the body simply ends
         after the name; that is a pilot without a trace, not an error */
      break;

    encoded = StripWhitespace(encoded);
    if (encoded.empty())
      /* known pilot, no trace yet */
      continue;

    PilotTrace trace;
    trace.id = name;

    try {
      /* DecodePolyline() throws; keep it contained so one bad record
         does not discard the other pilots in the same response */
      trace.points = DecodePolyline(encoded);
    } catch (...) {
      success = false;
      continue;
    }

    if (!trace.points.empty())
      traces.insert_or_assign(std::string{name}, std::move(trace));
  }

  return success;
}

}
