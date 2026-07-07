// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Network.hpp"
#include "Components.hpp"
#include "InfoBoxes/Data.hpp"
#include "InfoBoxes/Panel/Panel.hpp"
#include "Interface.hpp"
#include "NMEA/Validity.hpp"
#include "NetComponents.hpp"
#include "UIGlobals.hpp"
#include "net/client/NetworkWidget/Glue.hpp"
#include "net/client/NetworkWidget/Settings.hpp"

#include <chrono>

void
InfoBoxContentNetwork::Update(InfoBoxData &data) noexcept
{
  if (net_components == nullptr || net_components->networkWidget == nullptr ||
      index >= NetworkWidgetSettings::NETWORK_WIDGET_SLOTS)
    return;

  NetworkWidget::Data &widget_data = net_components->networkWidget->GetData(index);
  const NMEAInfo &basic = CommonInterface::Basic();

  std::lock_guard lock(widget_data.mutex);

  widget_data.validity.Expire(basic.clock, std::chrono::hours(1));

  if (widget_data.line1.empty()) {
    // Never received any usable data (or it has expired away).
    data.SetInvalid();
    return;
  }

  data.SetValue(widget_data.line1.c_str());

  std::string subtitle;
  if (widget_data.failed) {
    // The most recent fetch failed: surface it rather than silently
    // showing stale data.
    subtitle = fmt::format("{} (!)", widget_data.line2);
  } else if (!widget_data.validity.IsValid()) {
    // Data is stale (older than the expiry window above).
    subtitle = fmt::format("{} (stale)", widget_data.line2);
  } else {
    const Validity now{basic.clock};
    subtitle = fmt::format(
        "{} ({}s)", widget_data.line2,
        round(now.GetTimeDifference(widget_data.validity).count()));
  }
  data.SetComment(subtitle.c_str());

  if (!widget_data.line3.empty())
    data.SetTitle(widget_data.line3.c_str());
}
