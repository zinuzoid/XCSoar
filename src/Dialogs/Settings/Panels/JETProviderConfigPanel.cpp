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
#include "Profile/ProfileKeys.hpp"
#include "Widget/RowFormWidget.hpp"

#include "Profile/Profile.hpp"
#include "Form/Edit.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Base.hpp"


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
      std::chrono::seconds{settings.radar.interval},
      2);
  } else {
    AddDuration(_("Interval"), nullptr,
      std::chrono::seconds{5},
      std::chrono::seconds{60},
      std::chrono::seconds{5},
      std::chrono::seconds{settings.radar.interval},
      2);
  }
  SetExpertRow(RADAR_INTERVAL);
 
  AddText(_("Access Token"),
    nullptr,
    settings.radar.access_token);
  SetExpertRow(RADAR_ACCESS_TOKEN);
}

bool JETProviderConfigPanel::Save(bool &_changed) noexcept {
  bool changed = false;

  JETProviderSettings &settings =
    CommonInterface::SetComputerSettings().jet_provider_setting;

  changed |= SaveValue(RADAR_ENABLED,
    ProfileKeys::JETProviderRadarEnabled, settings.radar.enabled);
 
  changed |= SaveValueInteger(RADAR_INTERVAL,
    ProfileKeys::JETProviderRadarInterval, settings.radar.interval);
  
  changed |= SaveValue(RADAR_ACCESS_TOKEN,
    ProfileKeys::JETProviderRadarAccessToken, settings.radar.access_token);
  
  _changed |= changed;

  return true;
}

std::unique_ptr<Widget> CreateJETProviderConfigPanel() {
  return std::make_unique<JETProviderConfigPanel>();
}
