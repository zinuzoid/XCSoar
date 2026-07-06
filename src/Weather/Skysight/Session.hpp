// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <ctime>
#include <string>

/**
 * A Skysight API session key obtained from the "/auth" endpoint.
 * Plain value type; safe to copy across threads.
 */
struct SkysightSession {
  std::string key;

  /** UNIX timestamp at which #key stops being accepted */
  std::time_t expiry = 0;

  bool IsValid(std::time_t now) const noexcept {
    /* one minute of margin so the key does not expire while a
       request using it is still in flight */
    return !key.empty() && now + 60 < expiry;
  }

  void Clear() noexcept {
    key.clear();
    expiry = 0;
  }
};
