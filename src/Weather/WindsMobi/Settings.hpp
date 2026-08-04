// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Settings for the winds.mobi wind station map overlay.
 */
struct WindStationSettings {
  bool enabled;

  void SetDefaults() noexcept {
    enabled = false;
  }
};
