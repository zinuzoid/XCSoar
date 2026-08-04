// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindow.hpp"
#include "Look/MapLook.hpp"
#include "MapSettings.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Renderer/WindBarbRenderer.hpp"
#include "Renderer/TextInBox.hpp"
#include "Weather/WindsMobi/Glue.hpp"
#include "Units/Units.hpp"
#include "Math/Screen.hpp"
#include "Math/Util.hpp"
#include "Screen/Layout.hpp"

#include <chrono>

void
MapWindow::DrawWindStations(Canvas &canvas) noexcept
{
  if (wind_stations == nullptr || !GetComputerSettings().wind_station.enabled)
    return;

  const unsigned scale = Layout::Scale(100U);
  const Angle screen_angle = render_projection.GetScreenAngle();
  const auto now = std::chrono::system_clock::now();

  const auto lock = wind_stations->Lock();

  for (const auto &station : wind_stations->Get()) {
    if (!station.wind_available)
      /* nothing meaningful to draw */
      continue;

    const auto p = render_projection.GeoToScreenIfVisible(station.location);
    if (!p)
      continue;

    const bool stale = now - station.measured_at > std::chrono::minutes(20);
    const unsigned band = WindStationLook::BandIndex(station.wind_max);
    const WindBarbLook &barb_look =
      (stale ? look.wind_station.stale_bands : look.wind_station.bands)[band];

    /* same convention as the own-ship wind arrow: SpeedVector::bearing
       is the direction the wind blows FROM */
    const Angle wind_from = station.wind.bearing - screen_angle;

    /* the barb's staff/feather count is a fixed meteorological
       convention in knots, independent of the user's display unit
       (which only governs the avg/gust label text below) */
    const unsigned speed_kt =
      uround(Units::ToUserUnit(station.wind.norm, Unit::KNOTS));

    const auto barb =
      WindBarbRenderer(barb_look).Draw(canvas, *p, wind_from, speed_kt);

    StaticString<16> buffer;
    buffer.Format(_T("%d/%d"),
                 iround(Units::ToUserWindSpeed(station.wind.norm)),
                 iround(Units::ToUserWindSpeed(station.wind_max)));

    /* tip_y already accounts for however long the staff grew to fit
       this station's barbs and pennants */
    BulkPixelPoint label[] = {
      { 0, barb.tip_y - 2 },
    };
    PolygonRotateShift(label, *p, wind_from, scale);

    canvas.SetTextColor(COLOR_BLACK);
    canvas.Select(*look.wind_station.font);

    TextInBoxMode style;
    style.align = TextInBoxMode::Alignment::CENTER;
    style.vertical_position = TextInBoxMode::VerticalPosition::CENTERED;
    style.shape = LabelShape::OUTLINED;

    /* go through the shared LabelBlock so a crowded cluster of
       stations drops overlapping labels instead of smearing them */
    TextInBox(canvas, buffer, label[0], style,
             render_projection.GetScreenRect(), &label_block);
  }
}
