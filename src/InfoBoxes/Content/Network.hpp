// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/Content/Base.hpp"

class InfoBoxContentNetwork : public InfoBoxContent
{
public:
  InfoBoxContentNetwork(const unsigned _index) noexcept
    :index(_index) {}

  void Update(InfoBoxData &data) noexcept override;

private:
  unsigned index;
};
