// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindow.hpp"
#include "Look/MapLook.hpp"
#include "MapSettings.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Renderer/WindArrowRenderer.hpp"
#include "Renderer/TextInBox.hpp"
#include "Weather/WindsMobi/Glue.hpp"
#include "Units/Units.hpp"
#include "Math/Screen.hpp"
#include "Math/Util.hpp"
#include "Screen/Layout.hpp"

#include <chrono>

void
MapWindow::DrawWindStations(Canvas &canvas) const noexcept
{
  if (wind_stations == nullptr || !GetComputerSettings().wind_station.enabled)
    return;

  /* fixed arrow size: with up to ~30 stations on screen at once, a
     size that scales with wind strength (like the own-ship arrow)
     would make strong-wind clusters unreadable.  Strength is instead
     conveyed by colour band and the avg/gust label. */
  constexpr unsigned ARROW_WIDTH = 7;
  constexpr unsigned ARROW_TAIL_LENGTH = 2;
  constexpr unsigned ARROW_LENGTH = 12;
  constexpr unsigned ARROW_OFFSET = 0;

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
    const WindArrowLook &arrow_look =
      (stale ? look.wind_station.stale_bands : look.wind_station.bands)[band];

    /* same convention as the own-ship wind arrow: SpeedVector::bearing
       is the direction the wind blows FROM */
    const Angle angle = station.wind.bearing - screen_angle;

    WindArrowRenderer(arrow_look)
      .DrawArrow(canvas, *p, angle,
                ARROW_WIDTH, ARROW_LENGTH, ARROW_TAIL_LENGTH,
                WindArrowStyle::FULL_ARROW, ARROW_OFFSET, scale);

    StaticString<16> buffer;
    buffer.Format(_T("%d/%d"),
                 iround(Units::ToUserWindSpeed(station.wind.norm)),
                 iround(Units::ToUserWindSpeed(station.wind_max)));

    BulkPixelPoint label[] = {
      { 0, -int(ARROW_OFFSET + ARROW_LENGTH + ARROW_TAIL_LENGTH + 2) },
    };
    PolygonRotateShift(label, *p, angle, scale);

    canvas.SetTextColor(COLOR_BLACK);
    canvas.Select(*arrow_look.font);

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
