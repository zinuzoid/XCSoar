// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindStationDetails.hpp"
#include "Weather/WindsMobi/Station.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "Formatter/AngleFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Units/Group.hpp"

using namespace std::chrono;

/**
 * A read-only snapshot of a wind station's last measurement.  There
 * is no live update: winds.mobi is polled every few minutes, and
 * re-opening this dialog after the next poll is enough to see a
 * fresher reading.
 */
class WindStationDetailsWidget final : public RowFormWidget {
  enum Controls {
    STATION,
    PROVIDER,
    ALTITUDE,
    DIRECTION,
    AVERAGE,
    GUST,
    MEASURED,
  };

  const WindsMobi::Station station;

public:
  explicit WindStationDetailsWidget(const WindsMobi::Station &_station) noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()), station(_station) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
WindStationDetailsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                                  [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddReadOnly(_("Station"), nullptr,
             !station.name.empty() ? station.name.c_str() : station.id.c_str());

  AddReadOnly(_("Provider"), nullptr,
             !station.provider.empty() ? station.provider.c_str() : _T("--"));

  if (station.altitude >= 0)
    AddReadOnly(_("Altitude"), nullptr, _T("%.0f"),
               UnitGroup::ALTITUDE, station.altitude);
  else
    AddReadOnly(_("Altitude"), nullptr, _T("--"));

  AddReadOnly(_("Wind direction"), nullptr,
             FormatBearing(station.wind.bearing).c_str());

  AddReadOnly(_("Average"), nullptr, _T("%.0f"),
             UnitGroup::WIND_SPEED, station.wind.norm);

  AddReadOnly(_("Gust"), nullptr, _T("%.0f"),
             UnitGroup::WIND_SPEED, station.wind_max);

  const auto age = duration_cast<seconds>(
      system_clock::now() - station.measured_at);
  StaticString<64> measured;
  measured.Format(_T("%s %s"), FormatTimespanSmart(age).c_str(), _("ago"));
  AddReadOnly(_("Measured"), nullptr, measured);
}

void
dlgWindStationDetailsShowModal(const WindsMobi::Station &station)
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<WindStationDetailsWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(), look,
          !station.name.empty() ? station.name.c_str() : station.id.c_str());
  dialog.SetWidget(station);
  dialog.AddButton(_("Close"), mrOK);
  dialog.ShowModal();
}
