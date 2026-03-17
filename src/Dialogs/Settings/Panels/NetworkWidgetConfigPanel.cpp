// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "NetworkWidgetConfigPanel.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Profile/Keys.hpp"
#include "Widget/RowFormWidget.hpp"

#include "Form/DataField/Base.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/Edit.hpp"
#include "Profile/Profile.hpp"

void
NetworkWidgetConfigPanel::Prepare(ContainerWindow &parent,
                                  const PixelRect &rc) noexcept
{
  const JETProviderSettings &jet_settings =
      CommonInterface::GetComputerSettings().jet_provider_setting;
  const NetworkWidgetSettings &settings =
      CommonInterface::GetComputerSettings().network_widget;

  RowFormWidget::Prepare(parent, rc);

  AddMultiLine(_("Network Widget InfoBoxes periodically fetch data from URLs and display the response. "
                  "Each line of the response maps to: line 1 = value, line 2 = subtitle, line 3 = title. "
                  "Configure up to 2 independent URLs below.\n\n"
                  "** EXPERIMENTAL: This feature is subject to change. **"));

  if (jet_settings.radar.access_token.Contains("JIM") ||
      jet_settings.radar.access_token.Contains("DEV"))
  {
    AddDuration(_("Interval"), nullptr, std::chrono::seconds{1},
                std::chrono::seconds{300}, std::chrono::seconds{1},
                settings.interval, 2);
  }
  else
  {
    AddDuration(_("Interval"), nullptr, std::chrono::seconds{60},
                std::chrono::seconds{300}, std::chrono::seconds{30},
                settings.interval, 2);
  }
  SetExpertRow(INTERVAL);

  AddText(_("Url 1"), nullptr, settings.urls[0]);
  SetExpertRow(URL_0);

  AddText(_("Url 2"), nullptr, settings.urls[1]);
  SetExpertRow(URL_1);
}

bool
NetworkWidgetConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  NetworkWidgetSettings &settings =
      CommonInterface::SetComputerSettings().network_widget;

  changed |= SaveValue(INTERVAL, ProfileKeys::NetworkWidgetInterval,
                       settings.interval);

  changed |= SaveValue(URL_0, ProfileKeys::NetworkWidgetUrl, settings.urls[0]);
  changed |= SaveValue(URL_1, ProfileKeys::NetworkWidgetUrl2, settings.urls[1]);

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateNetworkWidgetConfigPanel()
{
  return std::make_unique<NetworkWidgetConfigPanel>();
}
