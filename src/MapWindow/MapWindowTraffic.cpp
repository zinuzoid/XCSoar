// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindow.hpp"
#include "ui/canvas/Icon.hpp"
#include "Screen/Layout.hpp"
#include "Formatter/UserUnits.hpp"
#include "Look/TrafficLook.hpp"
#include "Renderer/TextInBox.hpp"
#include "Renderer/TrafficRenderer.hpp"
#include "FLARM/Friends.hpp"
#include "Tracking/SkyLines/Data.hpp"
#include "Tracking/JETProvider/TrafficDecode.hpp"
#include "util/StringCompare.hxx"
#include "FLARM/TrafficClimbAltIndicators.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#include <cassert>
#include <map>

static void
DrawFlarmTraffic(Canvas &canvas, const WindowProjection &projection,
                 const TrafficLook &look, bool fading, bool vario_traffic,
                 const PixelPoint aircraft_pos,
                 const FlarmTraffic &traffic,
                 double set_mc, double current_30s_vario) noexcept
{
  assert(traffic.location_available);

  // Points for the screen coordinates for the icon, name and average climb
  PixelPoint sc;

  // If FLARM target not on the screen, move to the next one
  if (auto p = projection.GeoToScreenIfVisible(traffic.location))
    sc = *p;
  else
    return;

  TextInBoxMode mode;
  if (!fading)
    mode.shape = LabelShape::OUTLINED;

  // JMW TODO enhancement: decluttering of FLARM altitudes (sort by max lift)

  // only draw labels if not close to aircraft
  if ((sc - aircraft_pos).MagnitudeSquared() > Layout::Scale(30 * 30)) {
    // If FLARM callsign/name available draw it to the canvas
    if (traffic.HasName() && !StringIsEmpty(traffic.name)) {
      // Draw the name 16 points below the icon
      auto sc_name = sc;
      sc_name.y -= Layout::Scale(20);

      TextInBox(canvas, traffic.name, sc_name,
                mode, projection.GetScreenRect());
    }

    if (!fading && traffic.climb_rate_avg30s >= 0.1) {
      // If average climb data available draw it to the canvas

      // Draw the average climb value above the icon
      auto sc_av = sc;
      sc_av.y += Layout::Scale(5);

      TextInBox(canvas,
                FormatUserVerticalSpeed(traffic.climb_rate_avg30s, false),
                sc_av, mode,
                projection.GetScreenRect());
    }
  }

  auto color = FlarmFriends::GetFriendColor(traffic.id);

  const TrafficClimbAltIndicators indicators =
    TrafficClimbAltIndicators::GetClimbAltIndicators(traffic, set_mc, current_30s_vario);

  TrafficRenderer::Draw(canvas, look, fading, vario_traffic, traffic,
                        traffic.track - projection.GetScreenAngle(),
                        color, sc, indicators);
}

/**
 * Map scale beyond which FLARM traffic icons are hidden.
 * Extracted from the former magic literal so it can also gate the
 * JETProvider de-duplication check.
 */
static constexpr double TRAFFIC_MAP_SCALE_LIMIT = 7300;

/**
 * True when the FLARM drawing path will actually paint targets
 * (setting on AND zoomed in enough).
 */
static bool
IsFlarmTrafficDrawn(const MapSettings &settings,
                    const WindowProjection &projection) noexcept
{
  return settings.show_flarm_on_map &&
    projection.GetMapScale() <= TRAFFIC_MAP_SCALE_LIMIT;
}

/**
 * True when the given JETProvider traffic_id identifies a target that
 * is (or will be) drawn as FLARM traffic — either as a live target
 * with a valid position or as a fading ghost.
 */
static bool
IsShownAsFlarmTraffic(const char *traffic_id,
                      const TrafficList &flarm,
                      const std::map<FlarmId, FlarmTraffic> &fading) noexcept
{
  const FlarmId id = JETProvider::ParseTrafficId(traffic_id);
  if (!id.IsDefined())
    return false;

  const FlarmTraffic *t = flarm.FindTraffic(id);
  if (t != nullptr && t->location_available)
    return true;

  return fading.contains(id);
}

/**
 * Draws the FLARM traffic icons onto the given canvas
 * @param canvas Canvas for drawing
 */
void
MapWindow::DrawFLARMTraffic(Canvas &canvas,
                            const PixelPoint aircraft_pos) const noexcept
{
  if (!IsFlarmTrafficDrawn(GetMapSettings(), render_projection))
    return;

  // Return if FLARM data is not available
  const TrafficList &flarm = Basic().flarm.traffic;

  const WindowProjection &projection = render_projection;

  canvas.Select(*traffic_look.font);

  const bool vario_traffic = GetMapSettings().use_vario_traffic_colours;
  const double set_mc = GetComputerSettings().polar.glide_polar_task.GetMC();
  const double current_30s_vario = Calculated().average;

  // Circle through the FLARM targets
  for (const auto &traffic : flarm.list) {
    if (!traffic.location_available)
      continue;

    DrawFlarmTraffic(canvas, projection, traffic_look, false, vario_traffic,
                     aircraft_pos, traffic, set_mc, current_30s_vario);
  }

  if (const auto &fading = GetFadingFlarmTraffic(); !fading.empty()) {
    for (const auto &[id, traffic] : fading) {
      assert(traffic.location_available);

      DrawFlarmTraffic(canvas, projection, traffic_look, true, vario_traffic,
                       aircraft_pos, traffic, set_mc, current_30s_vario);
    }
  }
}


/**
 * Draws the GliderLink traffic icons onto the given canvas
 * @param canvas Canvas for drawing
 */
void
MapWindow::DrawGLinkTraffic([[maybe_unused]] Canvas &canvas) const noexcept
{
#ifdef ANDROID

  // Return if FLARM icons on moving map are disabled
  if (!GetMapSettings().show_flarm_on_map)
    return;

  const GliderLinkTrafficList &traffic = Basic().glink_data.traffic;
  if (traffic.IsEmpty())
    return;

  const MoreData &basic = Basic();

  const WindowProjection &projection = render_projection;

  canvas.Select(*traffic_look.font);

  // Circle through the GliderLink targets
  for (const auto &traf : traffic.list) {

    // Points for the screen coordinates for the icon, name and average climb
    PixelPoint sc;

    // If FLARM target not on the screen, move to the next one
    if (auto p = projection.GeoToScreenIfVisible(traf.location))
      sc = *p;
    else
      continue;

    TextInBoxMode mode;
    mode.shape = LabelShape::OUTLINED;
    mode.align = TextInBoxMode::Alignment::RIGHT;

    // If callsign/name available draw it to the canvas
    if (traf.HasName() && !StringIsEmpty(traf.name)) {
      // Draw the callsign above the icon
      auto sc_name = sc;
      sc_name.x -= Layout::Scale(10);
      sc_name.y -= Layout::Scale(15);

      TextInBox(canvas, traf.name, sc_name,
                mode, GetClientRect());
    }

    if (traf.climb_rate_received) {

      // If average climb data available draw it to the canvas
      mode.align = TextInBoxMode::Alignment::LEFT;

      // Draw the average climb to the right of the icon
      auto sc_av = sc;
      sc_av.x += Layout::Scale(10);
      sc_av.y -= Layout::Scale(8);

      TextInBox(canvas,
                FormatUserVerticalSpeed(traf.climb_rate, false),
                sc_av, mode, GetClientRect());
    }

    // use GPS altitude to be consistent with GliderLink
    if(basic.gps_altitude_available && traf.altitude_received
        && fabs(double(traf.altitude) - basic.gps_altitude) >= 100.0) {
      // If average climb data available draw it to the canvas
      TCHAR label_alt[100];
      double alt = (double(traf.altitude) - basic.gps_altitude) / 100.0;
      FormatRelativeUserAltitude(alt, label_alt, false);

      // Location of altitude label
      auto sc_alt = sc;
      sc_alt.x -= Layout::Scale(10);
      sc_alt.y -= Layout::Scale(0);

      mode.align = TextInBoxMode::Alignment::RIGHT;
      TextInBox(canvas, label_alt, sc_alt, mode, GetClientRect());
    }

    TrafficRenderer::Draw(canvas, traffic_look, traf,
                          traf.track - projection.GetScreenAngle(), sc);
  }
#endif
}

/**
 * Draws the teammate icon to the given canvas
 * @param canvas Canvas for drawing
 */
void
MapWindow::DrawTeammate(Canvas &canvas) const noexcept
{
  const TeamInfo &teamcode_info = Calculated();

  if (teamcode_info.teammate_available) {
    if (auto p = render_projection.GeoToScreenIfVisible(teamcode_info.teammate_location))
      traffic_look.teammate_icon.Draw(canvas, *p);
  }
}

#ifdef HAVE_SKYLINES_TRACKING

void
MapWindow::DrawSkyLinesTraffic(Canvas &canvas) const noexcept
{
  if (DisplaySkyLinesTrafficMapMode::OFF == GetMapSettings().skylines_traffic_map_mode ||
      skylines_data == nullptr)
    return;

  canvas.Select(*traffic_look.font);

  const std::lock_guard lock{skylines_data->mutex};
  for (auto &i : skylines_data->traffic) {
    if (auto p = render_projection.GeoToScreenIfVisible(i.second.location)) {
      traffic_look.teammate_icon.Draw(canvas, *p);
      if (DisplaySkyLinesTrafficMapMode::SYMBOL_NAME == GetMapSettings().skylines_traffic_map_mode) {
        const auto name_i = skylines_data->user_names.find(i.first);
        const TCHAR *name = name_i != skylines_data->user_names.end()
          ? name_i->second.c_str()
          : _T("");

        StaticString<128> buffer;
        buffer.Format(_T("%s [%um]"), name, i.second.altitude);

        TextInBoxMode mode;
        mode.shape = LabelShape::OUTLINED;

        // Draw the name 16 points below the icon
        p->y -= Layout::Scale(10);
        TextInBox(canvas, buffer, *p, mode, GetClientRect());
      }
    }
  }
}

#endif

/**
 * Draw the live flight trace of the pilots we follow, so the thermals
 * they used and the lines they flew are visible on the map.
 *
 * The server currently sends positions only; once it also sends altitude
 * and time, the polyline can be coloured by climb rate like the own snail
 * trail (see TrailRenderer).
 */
void
MapWindow::DrawJETProviderTrace(Canvas &canvas) const noexcept
{
  if (jet_provider_trace_data == nullptr)
    return;

  const std::lock_guard<Mutex> lock(jet_provider_trace_data->mutex);

  if (jet_provider_trace_data->traces.empty())
    return;

  const WindowProjection &projection = render_projection;
  /* generous bounds so a trace leaving the screen still joins up */
  const GeoBounds bounds = projection.GetScreenBounds().Scale(4);

  /* trace pens carry alpha (ALPHA_OVERLAY); enable blending so the
     alpha channel is honoured rather than silently ignored */
#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
#endif

  unsigned pen_index = 0;

  for (const auto &i : jet_provider_trace_data->traces) {
    const auto &trace = i.second;
    if (trace.points.empty())
      continue;

    canvas.Select(traffic_look.trace_pens[pen_index % TrafficLook::NUM_TRACE_PENS]);
    ++pen_index;

    PixelPoint last_point(0, 0);
    bool last_valid = false;

    for (const auto &location : trace.points) {
      if (!bounds.IsInside(location)) {
        /* outside the map window; don't paint it */
        last_valid = false;
        continue;
      }

      const auto pt = projection.GeoToScreen(location);

      if (last_valid)
        canvas.DrawLinePiece(last_point, pt);

      last_point = pt;
      last_valid = true;
    }
  }
}

void
MapWindow::DrawJETProviderTraffic(Canvas &canvas,
  const PixelPoint) const noexcept
{
  if (jet_provider_data == nullptr) {
    return;
  }

  const MoreData &basic = Basic();

  const WindowProjection &projection = render_projection;

  canvas.Select(*traffic_look.font);

  const bool vario_traffic_jet = GetMapSettings().use_vario_traffic_colours;
  const double jet_set_mc = GetComputerSettings().polar.glide_polar_task.GetMC();
  const double jet_30s_vario = Calculated().average;

  const std::lock_guard lock{jet_provider_data->mutex};

  if(jet_provider_data->traffics.empty()) {
    return;
  }

  const bool online = jet_provider_data->validity.IsValid() &&
    jet_provider_data->success;

  /* When FLARM traffic is being drawn, suppress JET targets whose
     traffic_id matches a live or fading FLARM target.  The check is
     gated on the same predicate so that disabling FLARM display or
     zooming out past the scale limit does not erase both copies. */
  const bool skip_flarm_duplicates =
    IsFlarmTrafficDrawn(GetMapSettings(), projection);
  const TrafficList &flarm = basic.flarm.traffic;
  const auto &fading = GetFadingFlarmTraffic();

  // Circle through the FLARM targets
  for (auto it = jet_provider_data->traffics.begin(),
      end = jet_provider_data->traffics.end();
      it != end; ++it) {
    const auto &traffic = (*it).second;

    // Save the location of the FLARM target
    GeoPoint target_loc = traffic.location;

    // Points for the screen coordinates for the icon, name and average climb
    PixelPoint sc, sc_name, sc_bottom;

    // If FLARM target not on the screen, move to the next one
    if (auto p = projection.GeoToScreenIfVisible(target_loc))
      sc = *p;
    else
      continue;

    if (skip_flarm_duplicates &&
        IsShownAsFlarmTraffic(traffic.traffic_id.c_str(), flarm, fading))
      continue;

    // Draw the name 16 points below the icon
    sc_name = sc;
    sc_name.y -= Layout::Scale(18);
    sc_name.x -= Layout::Scale(10);

    // Draw the average climb value above the icon
    sc_bottom = sc;
    sc_bottom.y += Layout::Scale(8);
    sc_bottom.x -= Layout::Scale(10);

    TextInBoxMode mode;
    mode.shape = LabelShape::OUTLINED;

    if (!traffic.display.empty())
      TextInBox(canvas, traffic.display.c_str(), sc_name,
                mode, GetClientRect());

    char second_text[32];
    TCHAR altitude_text[16];
    FormatUserAltitude(traffic.altitude, altitude_text, false);
    if (abs(traffic.vspeed) >= 0.1) {
      // If average climb data available draw it to the canvas
      TCHAR vspeed_text[16];
      FormatUserVerticalSpeed(traffic.vspeed,vspeed_text, false, true);
      StringFormat(second_text, 32, "%s %s", altitude_text , vspeed_text);
    } else {
      StringFormat(second_text, 32, "%s", altitude_text);
    }
    TextInBox(canvas, second_text, sc_bottom, mode, GetClientRect());

    const auto icon = JETProvider::DecodeIconType(traffic.icon_type, online);

    FlarmTraffic t;
    t.alarm_level = icon.alarm_level;
    t.type = JETProvider::ParseAircraftType(traffic.type.c_str())
      .value_or(FlarmTraffic::AircraftType::UNKNOWN);
    t.relative_altitude = (RoughAltitude) 100;
    t.climb_rate_avg30s = traffic.climb_rate_avg30s;
    if (online && basic.gps_altitude_available) {
      t.relative_altitude = (RoughAltitude) (traffic.altitude - basic.gps_altitude);
    }

    const TrafficClimbAltIndicators jet_indicators =
      TrafficClimbAltIndicators::GetClimbAltIndicators(t, jet_set_mc, jet_30s_vario);
    TrafficRenderer::Draw(canvas, traffic_look, false, vario_traffic_jet, t,
                          Angle::Degrees(traffic.track) - projection.GetScreenAngle(),
                          icon.circle, sc, jet_indicators);
  }

}