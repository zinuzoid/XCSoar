// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/Content/Base.hpp"

class InfoBoxContentNetwork : public InfoBoxContent
{
public:
  void Update(InfoBoxData &data) noexcept override;
};