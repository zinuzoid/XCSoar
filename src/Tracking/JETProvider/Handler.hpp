// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Data.hpp"
#include "time/Stamp.hpp"

#include <exception>
#include <vector>

namespace JETProvider {

/**
 * Callback interface for #JETProvider::Glue.  All methods except
 * OnJETProviderReset() are invoked from the I/O event loop thread;
 * OnJETProviderReset() is invoked from the thread calling
 * Glue::SetSettings() (the UI thread).
 */
class Handler {
public:
  /**
   * A poll succeeded; @param traffics replaces the previous list.
   */
  virtual void OnJETTraffic(std::vector<Traffic> &&traffics,
                            TimeStamp now) = 0;

  /**
   * A poll failed (network error, bad HTTP status or malformed
   * response body).
   */
  virtual void OnJETProviderError(std::exception_ptr e) = 0;

  /**
   * The provider was disabled or the access token changed; discard
   * all traffic.
   */
  virtual void OnJETProviderReset() = 0;
};

} // namespace JETProvider
