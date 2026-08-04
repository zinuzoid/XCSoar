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

#ifndef JET_PROVIDER_SETTINGS_HPP
#define JET_PROVIDER_SETTINGS_HPP

#include <chrono>
#include "util/StaticString.hxx"

struct JETProviderSettings {

  struct Radar {
    bool enabled;
    std::chrono::duration<unsigned> interval;
    StaticString<64> access_token;

    void SetDefaults() {
      enabled = false;
      interval = std::chrono::seconds(5);
      access_token.clear();
    }
  };

  /**
   * Overlay the live flight trace of selected pilots on the map.
   */
  struct Trace {
    bool enabled;
    std::chrono::duration<unsigned> interval;
    /** the tracking network the ids belong to, e.g. "ogn" */
    StaticString<16> src;
    /** comma separated list of pilot ids to follow */
    StaticString<256> pilot_ids;

    void SetDefaults() {
      enabled = false;
      interval = std::chrono::seconds(30);
      src = "ogn";
      pilot_ids.clear();
    }
  };

  /**
   * Overlay nearby weather stations' wind measurements on the map.
   */
  struct Wind {
    bool enabled;

    void SetDefaults() {
      enabled = false;
    }
  };

  Radar radar;

  Trace trace;

  Wind wind;

  void SetDefaults() {
    radar.SetDefaults();
    trace.SetDefaults();
    wind.SetDefaults();
  }

  /**
   * The wind overlay rides on the radar's access token and status row,
   * so it is only ever active while the radar is.
   */
  constexpr bool IsWindEnabled() const noexcept {
    return radar.enabled && wind.enabled;
  }
};

#endif
