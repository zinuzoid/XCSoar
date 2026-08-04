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

#include "JETProviderConfigPanel.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Profile/Keys.hpp"
#include "Widget/RowFormWidget.hpp"

#include "Profile/Profile.hpp"
#include "Form/Edit.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Base.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"
#include "Tracking/Features.hpp"
#include "util/StaticString.hxx"

#ifdef HAVE_TRACKING
#include "Tracking/TrackingGlue.hpp"
#endif


void JETProviderConfigPanel::Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept {
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;

  RowFormWidget::Prepare(parent, rc);

  AddMultiLine(_("Warning This is a BETA service! No service guarantee will be provided. Use at your own risk!"));
  AddMultiLine(_("\"Radar\" will request external traffic information from internet and show in XCSOAR map to increase your situation awareness!"));
  AddMultiLine(_("Please request Access Token via\nbit.ly/jetprovider"));

  AddBoolean(_("Radar Enabled"),
    nullptr,
    settings.radar.enabled);
  SetExpertRow(RADAR_ENABLED);

  if(settings.radar.access_token.Contains("JIM") || settings.radar.access_token.Contains("DEV")) {
    AddDuration(_("Interval"), nullptr,
      std::chrono::seconds{1},
      std::chrono::seconds{15},
      std::chrono::seconds{1},
      settings.radar.interval,
      2);
  } else {
    AddDuration(_("Interval"), nullptr,
      std::chrono::seconds{15},
      std::chrono::seconds{60},
      std::chrono::seconds{5},
      settings.radar.interval,
      2);
  }
  SetExpertRow(RADAR_INTERVAL);
 
  AddText(_("Access Token"),
    nullptr,
    settings.radar.access_token);
  SetExpertRow(RADAR_ACCESS_TOKEN);

  AddReadOnly(_("Token Status"),
    _("Result of the most recent radar request. Reflects the saved "
      "token, so close this dialog after editing it."));
  SetExpertRow(RADAR_STATUS);

  AddSpacer();
  SetExpertRow(SPACER);

  AddBoolean(_("Trace Enabled"),
    _("Overlay the live flight trace of the pilots listed below."),
    settings.trace.enabled);
  SetExpertRow(TRACE_ENABLED);

  AddDuration(_("Trace Interval"), nullptr,
    std::chrono::seconds{15},
    std::chrono::seconds{300},
    std::chrono::seconds{15},
    settings.trace.interval,
    2);
  SetExpertRow(TRACE_INTERVAL);

  AddText(_("Trace Source"),
    _("The tracking network the pilot ids belong to, e.g. \"ogn\"."),
    settings.trace.src);
  SetExpertRow(TRACE_SRC);

  AddText(_("Follow Pilot IDs"),
    _("Comma separated list of pilot ids whose trace will be drawn."),
    settings.trace.pilot_ids);
  SetExpertRow(TRACE_PILOT_IDS);

  AddSpacer();
  SetExpertRow(WIND_STATION_SPACER);

  AddBoolean(_("Wind Stations"),
    _("Show nearby wind stations (winds.mobi) on the map, with direction, "
      "average and gust speed."),
    CommonInterface::GetComputerSettings().wind_station.enabled);
  SetExpertRow(WIND_STATION_ENABLED);

  if(!settings.trace.enabled) {
    SetRowVisible(SPACER, false);
    SetRowVisible(TRACE_ENABLED, false);
    SetRowVisible(TRACE_INTERVAL, false);
    SetRowVisible(TRACE_SRC, false);
    SetRowVisible(TRACE_PILOT_IDS, false);
  }
}

void
JETProviderConfigPanel::UpdateStatus() noexcept
{
  const JETProviderSettings &settings =
    CommonInterface::GetComputerSettings().jet_provider_setting;

  if (!settings.radar.enabled) {
    SetText(RADAR_STATUS, _("Radar disabled"));
    return;
  }

  /* the same guard JETProvider::Glue::OnTimer() uses to skip the
     request */
  if (settings.radar.access_token.length() <= 2) {
    SetText(RADAR_STATUS, _("No access token"));
    return;
  }

#ifdef HAVE_TRACKING
  if (net_components != nullptr && net_components->tracking) {
    StaticString<128> status;

    {
      const JETProvider::Data &data =
        net_components->tracking->GetJETProviderData();
      const std::lock_guard lock{data.mutex};
      /* copy it out and drop the lock before touching the window */
      status = data.status;
    }

    if (!status.empty()) {
      SetText(RADAR_STATUS, status);
      return;
    }
  }
#endif

  SetText(RADAR_STATUS, _("Waiting for first request"));
}

void
JETProviderConfigPanel::Show(const PixelRect &rc) noexcept
{
  RowFormWidget::Show(rc);

  UpdateStatus();
  timer.Schedule(std::chrono::seconds(1));
}

void
JETProviderConfigPanel::Hide() noexcept
{
  timer.Cancel();

  RowFormWidget::Hide();
}

bool JETProviderConfigPanel::Save(bool &_changed) noexcept {
  bool changed = false;

  JETProviderSettings &settings =
    CommonInterface::SetComputerSettings().jet_provider_setting;

  changed |= SaveValue(RADAR_ENABLED,
    ProfileKeys::JETProviderRadarEnabled, settings.radar.enabled);

  changed |= SaveValue(RADAR_INTERVAL,
    ProfileKeys::JETProviderRadarInterval, settings.radar.interval);
  
  changed |= SaveValue(RADAR_ACCESS_TOKEN,
    ProfileKeys::JETProviderRadarAccessToken, settings.radar.access_token);

  changed |= SaveValue(TRACE_ENABLED,
    ProfileKeys::JETProviderTraceEnabled, settings.trace.enabled);

  changed |= SaveValue(TRACE_INTERVAL,
    ProfileKeys::JETProviderTraceInterval, settings.trace.interval);

  changed |= SaveValue(TRACE_SRC,
    ProfileKeys::JETProviderTraceSrc, settings.trace.src);

  changed |= SaveValue(TRACE_PILOT_IDS,
    ProfileKeys::JETProviderTracePilotIds, settings.trace.pilot_ids);

  changed |= SaveValue(WIND_STATION_ENABLED,
    ProfileKeys::WindStationsEnabled,
    CommonInterface::SetComputerSettings().wind_station.enabled);

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget> CreateJETProviderConfigPanel() {
  return std::make_unique<JETProviderConfigPanel>();
}
