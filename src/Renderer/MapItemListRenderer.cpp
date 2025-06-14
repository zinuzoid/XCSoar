// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapItemListRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "MapWindow/Items/MapItem.hpp"
#include "MapWindow/Items/OverlayMapItem.hpp"
#include "MapWindow/Items/RaspMapItem.hpp"
#include "Look/DialogLook.hpp"
#include "Look/MapLook.hpp"
#include "Renderer/AircraftRenderer.hpp"
#include "Renderer/AirspaceListRenderer.hpp"
#include "Renderer/WaypointListRenderer.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Formatter/UserUnits.hpp"
#include "Formatter/UserGeoPointFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Formatter/LocalTimeFormatter.hpp"
#include "Formatter/AngleFormatter.hpp"
#include "Dialogs/Task/dlgTaskHelpers.hpp"
#include "Renderer/OZPreviewRenderer.hpp"
#include "Language/Language.hpp"
#include "util/StringCompare.hxx"
#include "util/Macros.hpp"
#include "util/StaticString.hxx"
#include "MapSettings.hpp"
#include "Math/Screen.hpp"
#include "Look/FinalGlideBarLook.hpp"
#include "Renderer/TrafficRenderer.hpp"
#include "FLARM/Details.hpp"
#include "FLARM/FlarmNetRecord.hpp"
#include "Weather/Features.hpp"
#include "FLARM/List.hpp"
#include "time/RoughTime.hpp"
#include "time/BrokenDateTime.hpp"

#ifdef HAVE_NOAA
#include "Renderer/NOAAListRenderer.hpp"
#endif

using namespace std::chrono;

unsigned
MapItemListRenderer::CalculateLayout(const DialogLook &dialog_look)
{
  return row_renderer.CalculateLayout(*dialog_look.list.font_bold,
                                      dialog_look.small_font);
}

static void
Draw(Canvas &canvas, const PixelRect rc,
     const LocationMapItem &item,
     const TwoTextRowsRenderer &row_renderer)
{
  TCHAR info_buffer[256];
  if (item.vector.IsValid())
    StringFormatUnsafe(info_buffer, _T("%s: %s, %s: %s"),
                       _("Distance"),
                       FormatUserDistanceSmart(item.vector.distance).c_str(),
                       _("Direction"),
                       FormatBearing(item.vector.bearing).c_str());
  else
    StringFormatUnsafe(info_buffer, _T("%s: %s, %s: %s"),
                       _("Distance"), _T("???"), _("Direction"), _T("???"));

  row_renderer.DrawFirstRow(canvas, rc, info_buffer);

  StringFormatUnsafe(info_buffer, _T("%s: %s"), _("Elevation"),
                     item.HasElevation()
                     ? FormatUserAltitude(item.elevation).c_str()
                     : _T("???"));
  row_renderer.DrawSecondRow(canvas, rc, info_buffer);
}

static void
Draw(Canvas &canvas, PixelRect rc,
     const ArrivalAltitudeMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const FinalGlideBarLook &look)
{
  const unsigned line_height = rc.GetHeight();

  bool reach_relevant = item.reach.IsReachRelevant();

  int arrival_altitude =
    item.reach.terrain_valid == ReachResult::Validity::VALID
    ? item.reach.terrain
    : item.reach.direct;
  if (item.HasElevation())
    arrival_altitude -= item.elevation;

  bool reachable =
    item.reach.terrain_valid != ReachResult::Validity::UNREACHABLE &&
    arrival_altitude >= item.safety_height;

  // Draw final glide arrow icon

  const PixelPoint pt(rc.left + line_height / 2, rc.top + line_height / 2);

  BulkPixelPoint arrow[] = {
      { -7, -3 }, { 0, 4 }, { 7, -3 }
  };

  Angle arrow_angle = reachable ? Angle::HalfCircle() : Angle::Zero();
  PolygonRotateShift({arrow, ARRAY_SIZE(arrow)}, pt, arrow_angle,
                     Layout::Scale(100U));

  if (reachable) {
    canvas.Select(look.brush_above);
    canvas.Select(look.pen_above);
  } else {
    canvas.Select(look.brush_below);
    canvas.Select(look.pen_below);
  }
  canvas.DrawPolygon(arrow, ARRAY_SIZE(arrow));

  const unsigned text_padding = Layout::GetTextPadding();
  rc.left += line_height + text_padding;

  // Format title row

  TCHAR altitude_buffer[32];
  StaticString<256> buffer;
  buffer.clear();

  if (item.HasElevation()) {
    int relative_arrival_altitude =
      item.reach.direct - item.elevation;

    FormatRelativeUserAltitude(relative_arrival_altitude, altitude_buffer);

    buffer.AppendFormat(_T("%s %s, "), altitude_buffer, _("AGL"));
  }

  buffer.AppendFormat(_T("%s %s"),
                      FormatUserAltitude(item.reach.direct).c_str(),
                      _("MSL"));

  // Draw title row

  row_renderer.DrawFirstRow(canvas, rc, buffer);

  // Format comment row

  if (reach_relevant) {
    buffer.Format(_T("%s: "), _("around terrain"));

    if (item.HasElevation()) {
      int relative_arrival_altitude =
          item.reach.terrain - item.elevation;

      FormatRelativeUserAltitude(relative_arrival_altitude,
                                 altitude_buffer);

     buffer.AppendFormat(_T("%s %s, "), altitude_buffer, _("AGL"));
    }

    buffer.AppendFormat(_T("%s %s, "),
                        FormatUserAltitude(item.reach.terrain).c_str(),
                        _("MSL"));
  } else if (item.HasElevation() &&
             item.reach.direct >= item.elevation + item.safety_height &&
             item.reach.terrain_valid == ReachResult::Validity::UNREACHABLE) {
    buffer.UnsafeFormat(_T("%s "), _("Unreachable due to terrain."));
  } else {
    buffer.clear();
  }

  buffer += _("Arrival altitude");

  // Draw comment row

  row_renderer.DrawSecondRow(canvas, rc, buffer);
}

static void
Draw(Canvas &canvas, PixelRect rc,
     const SelfMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const AircraftLook &look,
     const MapSettings &settings)
{
  const unsigned line_height = rc.GetHeight();
  const unsigned text_padding = Layout::GetTextPadding();

  const PixelPoint pt(rc.left + line_height / 2, rc.top + line_height / 2);
  AircraftRenderer::Draw(canvas, settings, look, item.bearing, pt);

  rc.left += line_height + text_padding;

  row_renderer.DrawFirstRow(canvas, rc, _("Your Position"));
  row_renderer.DrawSecondRow(canvas, rc, FormatGeoPoint(item.location));
}

static void
Draw(Canvas &canvas, const PixelRect rc,
     const AirspaceMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const AirspaceLook &look,
     const AirspaceRendererSettings &renderer_settings)
{
  AirspaceListRenderer::Draw(canvas, rc, *item.airspace, row_renderer, look,
                             renderer_settings);
}

static void
Draw(Canvas &canvas, const PixelRect rc,
     const WaypointMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const WaypointLook &look,
     const WaypointRendererSettings &renderer_settings)
{
  WaypointListRenderer::Draw(canvas, rc, *item.waypoint,
                             row_renderer, look, renderer_settings);
}

#ifdef HAVE_NOAA
static void
Draw(Canvas &canvas, const PixelRect rc,
     const WeatherStationMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const NOAALook &look)
{
  const NOAAStore::Item &station = *item.station;
  NOAAListRenderer::Draw(canvas, rc, station, look, row_renderer);
}
#endif

static void
Draw(Canvas &canvas, PixelRect rc,
     const ThermalMapItem &item,
     RoughTimeDelta utc_offset,
     const TwoTextRowsRenderer &row_renderer,
     const MapLook &look)
{
  const unsigned line_height = rc.GetHeight();
  const unsigned text_padding = Layout::GetTextPadding();

  const ThermalSource &thermal = item.thermal;

  const PixelPoint pt(rc.left + line_height / 2,
                      rc.top + line_height / 2);

  look.thermal_source_icon.Draw(canvas, pt);

  rc.left += line_height + text_padding;

  row_renderer.DrawFirstRow(canvas, rc, _("Thermal"));

  StaticString<256> buffer;

  auto timespan = TimeStamp{BrokenDateTime::NowUTC().DurationSinceMidnight()} - thermal.time;
  if (timespan.count() < 0)
    timespan += hours{24};

  buffer.Format(_T("%s: %s - left %s ago (%s)"),
                _("Avg. lift"),
                FormatUserVerticalSpeed(thermal.lift_rate).c_str(),
                FormatTimespanSmart(timespan).c_str(),
                FormatLocalTimeHHMM(thermal.time, utc_offset).c_str());
  row_renderer.DrawSecondRow(canvas, rc, buffer);
}

static void
Draw(Canvas &canvas, PixelRect rc,
     const TaskOZMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const TaskLook &look, const AirspaceLook &airspace_look,
     const AirspaceRendererSettings &airspace_settings)
{
  const unsigned line_height = rc.GetHeight();
  const unsigned text_padding = Layout::GetTextPadding();

  const ObservationZonePoint &oz = *item.oz;
  const Waypoint &waypoint = *item.waypoint;

  const PixelPoint pt(rc.left + line_height / 2,
                      rc.top + line_height / 2);
  const unsigned radius = line_height / 2 - text_padding;
  OZPreviewRenderer::Draw(canvas, oz, pt, radius, look,
                          airspace_settings, airspace_look);

  rc.left += line_height + text_padding;

  TCHAR buffer[256];

  // Draw details line
  OrderedTaskPointRadiusLabel(*item.oz, buffer);
  if (!StringIsEmpty(buffer))
    row_renderer.DrawSecondRow(canvas, rc, buffer);

  // Draw waypoint name
  OrderedTaskPointLabel(item.tp_type, waypoint.name.c_str(),
                        waypoint.shortname.c_str(), item.index, buffer);
  row_renderer.DrawFirstRow(canvas, rc, buffer);
}

static void
Draw(Canvas &canvas, PixelRect rc,
     const TrafficMapItem &item,
     const TwoTextRowsRenderer &row_renderer,
     const TrafficLook &traffic_look,
     const TrafficList *traffic_list)
{
  const unsigned line_height = rc.GetHeight();
  const unsigned text_padding = Layout::GetTextPadding();

  const FlarmTraffic *traffic = traffic_list == nullptr
    ? nullptr
    : traffic_list->FindTraffic(item.id);

  const PixelPoint pt(rc.left + line_height / 2, rc.top + line_height / 2);

  // Render the representation of the traffic icon
  if (traffic != nullptr)
    TrafficRenderer::Draw(canvas, traffic_look, false,
                          *traffic, traffic->track,
                          item.color, pt);

  rc.left += line_height + text_padding;

  // Now render the text information
  const FlarmNetRecord *record = FlarmDetails::LookupRecord(item.id);

  StaticString<256> title_string;
  if (record && !StringIsEmpty(record->pilot))
    title_string = record->pilot.c_str();
  else
    title_string = _("FLARM Traffic");

  // Append name to the title, if it exists
  const TCHAR *callsign = FlarmDetails::LookupCallsign(item.id);
  if (callsign != nullptr && !StringIsEmpty(callsign)) {
    title_string.append(_T(", "));
    title_string.append(callsign);
  }

  row_renderer.DrawFirstRow(canvas, rc, title_string);

  StaticString<256> info_string;
  if (record && !StringIsEmpty(record->plane_type))
    info_string = record->plane_type;
  else if (traffic != nullptr)
    info_string = FlarmTraffic::GetTypeString(traffic->type);
  else
    info_string = _("Unknown");

  // Generate the line of info about the target, if it's available
  if (traffic != nullptr) {
    if (traffic->altitude_available)
      info_string.AppendFormat(_T(", %s: %s"), _("Altitude"),
                               FormatUserAltitude(traffic->altitude).c_str());

    if (traffic->climb_rate_avg30s_available) {
      info_string.AppendFormat(_T(", %s: %s"), _("Vario"),
                               FormatUserVerticalSpeed(traffic->climb_rate_avg30s).c_str());
    }
  }

  row_renderer.DrawSecondRow(canvas, rc, info_string);
}

#ifdef HAVE_SKYLINES_TRACKING

/**
 * Calculate how many minutes have passed since #past_ms.
 */
[[gnu::const]]
static constexpr auto
SinceInMinutes(TimeStamp now,
               duration<uint32_t, milliseconds::period> past_ms) noexcept
{
  using Minutes = duration<unsigned, minutes::period>;
  constexpr Minutes ONE_DAY = hours{24};
  auto now_minutes = now.Cast<Minutes>() % ONE_DAY;
  auto past_minutes = duration_cast<Minutes>(past_ms) % ONE_DAY;

  if (past_minutes >= hours{20} && now_minutes < hours{4})
    /* midnight rollover */
    now_minutes += ONE_DAY;

  if (past_minutes > now_minutes)
    return Minutes{};

  return now_minutes - past_minutes;
}

#include "Interface.hpp"

static void
Draw(Canvas &canvas, PixelRect rc,
     const SkyLinesTrafficMapItem &item,
     const TwoTextRowsRenderer &row_renderer)
{
  rc.right = row_renderer.DrawRightFirstRow(canvas, rc,
                                            FormatUserAltitude(item.altitude));

  row_renderer.DrawFirstRow(canvas, rc, item.name);

  if (CommonInterface::Basic().time_available) {
    StaticString<64> buffer;
    buffer.UnsafeFormat(_("%u minutes ago"),
                        SinceInMinutes(CommonInterface::Basic().time,
                                       duration<uint32_t, milliseconds::period>{item.time_of_day}).count());
    row_renderer.DrawSecondRow(canvas, rc, buffer);
  }
}

#endif /* HAVE_SKYLINES_TRACKING */

static void
Draw(Canvas &canvas, PixelRect rc,
     const OverlayMapItem &item,
     const TwoTextRowsRenderer &row_renderer)
{
  row_renderer.DrawFirstRow(canvas, rc, item.label.c_str());
}

static void
Draw(Canvas &canvas, PixelRect rc,
     const RaspMapItem &item,
     const TwoTextRowsRenderer &row_renderer)
{
  row_renderer.DrawFirstRow(canvas, rc, item.label.c_str());
}

void
MapItemListRenderer::Draw(Canvas &canvas, const PixelRect rc,
                          const MapItem &item,
                          const TrafficList *traffic_list)
{
  switch (item.type) {
  case MapItem::Type::LOCATION:
    ::Draw(canvas, rc, (const LocationMapItem &)item, row_renderer);
    break;
  case MapItem::Type::ARRIVAL_ALTITUDE:
    ::Draw(canvas, rc, (const ArrivalAltitudeMapItem &)item,
           row_renderer, final_glide_look);
    break;
  case MapItem::Type::SELF:
    ::Draw(canvas, rc, (const SelfMapItem &)item,
           row_renderer, look.aircraft, settings);
    break;
  case MapItem::Type::AIRSPACE:
    ::Draw(canvas, rc, (const AirspaceMapItem &)item,
           row_renderer, look.airspace,
           settings.airspace);
    break;
  case MapItem::Type::WAYPOINT:
    ::Draw(canvas, rc, (const WaypointMapItem &)item,
           row_renderer, look.waypoint,
           settings.waypoint);
    break;
  case MapItem::Type::TASK_OZ:
    ::Draw(canvas, rc, (const TaskOZMapItem &)item,
           row_renderer, look.task, look.airspace,
           settings.airspace);
    break;

#ifdef HAVE_NOAA
  case MapItem::Type::WEATHER:
    ::Draw(canvas, rc, (const WeatherStationMapItem &)item,
           row_renderer, look.noaa);
    break;
#endif

  case MapItem::Type::TRAFFIC:
    ::Draw(canvas, rc, (const TrafficMapItem &)item,
           row_renderer, traffic_look, traffic_list);
    break;

#ifdef HAVE_SKYLINES_TRACKING
  case MapItem::Type::SKYLINES_TRAFFIC:
    ::Draw(canvas, rc, (const SkyLinesTrafficMapItem &)item, row_renderer);
    break;
#endif

  case MapItem::Type::THERMAL:
    ::Draw(canvas, rc, (const ThermalMapItem &)item, utc_offset,
           row_renderer, look);
    break;

  case MapItem::Type::OVERLAY:
    ::Draw(canvas, rc, (const OverlayMapItem &)item, row_renderer);
    break;

  case MapItem::Type::RASP:
    ::Draw(canvas, rc, (const RaspMapItem &)item, row_renderer);
    break;
  }
}
