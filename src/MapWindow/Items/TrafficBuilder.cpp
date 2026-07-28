// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Builder.hpp"
#include "MapItem.hpp"
#include "List.hpp"
#include "FLARM/List.hpp"
#include "FLARM/Friends.hpp"
#include "Tracking/SkyLines/Data.hpp"
#include "Tracking/JETProvider/TrafficDecode.hpp"
#include "Tracking/TrackingGlue.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"

void
MapItemListBuilder::AddTraffic(const TrafficList &flarm)
{
  for (const auto &t : flarm.list) {
    if (list.full())
      break;

    if (location.DistanceS(t.location) < range) {
      auto color = FlarmFriends::GetFriendColor(t.id);
      list.append(new TrafficMapItem(t.id, color));
    }
  }
}

void
MapItemListBuilder::AddSkyLinesTraffic()
{
#ifdef HAVE_SKYLINES_TRACKING
  if (net_components == nullptr || !net_components->tracking)
    return;

  const auto &data = net_components->tracking->GetSkyLinesData();
  const std::lock_guard lock{data.mutex};

  StaticString<32> buffer;

  for (const auto &i : data.traffic) {
    if (list.full())
      break;

    if (i.second.location.IsValid() &&
        location.DistanceS(i.second.location) < range) {
      const uint32_t id = i.first;
      auto name_i = data.user_names.find(id);
      const TCHAR *name;
      if (name_i == data.user_names.end()) {
        /* no name found */
        buffer.UnsafeFormat(_T("SkyLines %u"), (unsigned)id);
        name = buffer;
      } else
        /* we know the name */
        name = name_i->second.c_str();

      list.append(new SkyLinesTrafficMapItem(id, i.second.time_of_day,
                                             i.second.altitude,
                                             name));
    }
  }
#endif
}

void
MapItemListBuilder::AddJETProviderTrace()
{
#ifdef HAVE_TRACKING
  if (net_components == nullptr || !net_components->tracking)
    return;

  const auto &data = net_components->tracking->GetJETProviderTraceData();
  const std::lock_guard lock{data.mutex};

  /* assign colours exactly like MapWindow::DrawJETProviderTrace() does,
     so the list entry matches the line drawn on the map */
  unsigned color_index = 0;

  for (const auto &i : data.traces) {
    if (list.full())
      break;

    const auto &trace = i.second;
    if (trace.points.empty())
      continue;

    const unsigned this_color = color_index++;

    /* pick up the trace if any part of it passes near the location the
       user tapped */
    for (const auto &point : trace.points) {
      if (point.IsValid() && location.DistanceS(point) < range) {
        list.append(new TraceMapItem(trace.id.c_str(), trace.points.size(),
                                     this_color));
        break;
      }
    }
  }
#endif
}

void
MapItemListBuilder::AddJETProviderTraffic()
{
#ifdef HAVE_TRACKING
  if (net_components == nullptr || !net_components->tracking)
    return;

  const auto &data = net_components->tracking->GetJETProviderData();
  const std::lock_guard lock{data.mutex};

  const bool online = data.validity.IsValid() && data.success;

  for (const auto &i : data.traffics) {
    if (list.full())
      break;

    const auto &traffic = i.second;

    if (!traffic.location.IsValid() ||
        location.DistanceS(traffic.location) >= range)
      continue;

    const auto icon = JETProvider::DecodeIconType(traffic.icon_type, online);

    list.append(new JETProviderTrafficMapItem(traffic.display, traffic.code,
                                              traffic.type, traffic.altitude,
                                              traffic.speed, traffic.vspeed,
                                              traffic.climb_rate_avg30s,
                                              traffic.track,
                                              icon.alarm_level, icon.circle));
  }
#endif
}
