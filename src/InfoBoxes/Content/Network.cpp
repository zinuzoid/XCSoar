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

#include <chrono>

void
InfoBoxContentNetwork::Update(InfoBoxData &data) noexcept
{
  if (net_components != nullptr && net_components->tracking != nullptr)
  {
    NetworkWidget::Data &widget_data = net_components->networkWidget->data;
    const NMEAInfo &basic = CommonInterface::Basic();

    std::lock_guard lock(widget_data.mutex);

    widget_data.validity.Expire(basic.clock, std::chrono::hours(1));
    std::string subtitle;
    if (!widget_data.validity.IsValid())
    {
      subtitle = widget_data.line2;
    }
    else
    {
      Validity validity{basic.clock};
      subtitle = fmt::format(
          "{} ({}s)", widget_data.line2,
          round(validity.GetTimeDifference(widget_data.validity).count()));
    }
    data.SetValue(widget_data.line1.c_str());
    data.SetComment(subtitle.c_str());
    if (!widget_data.line3.empty()) data.SetTitle(widget_data.line3.c_str());
  }
}
