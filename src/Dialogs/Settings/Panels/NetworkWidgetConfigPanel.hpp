// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Widget/Widget.hpp"

#include <memory>

class NetworkWidgetConfigPanel final : public RowFormWidget
{
public:
  NetworkWidgetConfigPanel() : RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  /* methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

enum NetworkWidgetControlIndex
{
  TEXT1,
  INTERVAL,
  URL,
};

std::unique_ptr<Widget> CreateNetworkWidgetConfigPanel();
