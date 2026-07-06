// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Settings.hpp"
#include "Geo/GeoBounds.hpp"
#include "co/InjectTask.hxx"
#include "thread/Mutex.hxx"
#include "time/PeriodClock.hpp"
#include "time/Stamp.hpp"
#include "util/StaticString.hxx"

class CurlGlobal;
struct NMEAInfo;

namespace JETProvider {

class Handler;

/**
 * Polls the JET radar API for traffic in the visible map area and
 * forwards the results to a #Handler.  SetSettings() and OnTimer()
 * must be called from the UI thread; the HTTP request and the
 * #Handler callbacks (except OnJETProviderReset()) run on the I/O
 * event loop thread.
 */
class Glue final {
  CurlGlobal &curl;
  Handler *const handler;
  Co::InjectTask inject_task;
  PeriodClock clock;

  /**
   * The current configuration.  Written only by SetSettings() on the
   * UI thread; a running request works on its own copy passed by
   * value into CoTick().
   */
  JETProviderSettings::Radar settings;

  /**
   * Protects #unauthorized_access_token, which is written by the I/O
   * thread (CoTick()) and read/cleared by the UI thread.
   */
  mutable Mutex mutex;

  /**
   * The last access token rejected by the server (HTTP 401).  While
   * it matches the configured token, polling is suspended until the
   * user configures a different token.
   */
  StaticString<64> unauthorized_access_token;

public:
  Glue(CurlGlobal &curl, Handler *handler);

  void SetSettings(const JETProviderSettings &settings);

  void OnTimer(const NMEAInfo &basic, const GeoBounds &visible_bounds);

private:
  Co::InvokeTask CoTick(JETProviderSettings::Radar settings,
                        GeoBounds bounds, TimeStamp now);

  void OnCompletion(std::exception_ptr error) noexcept;
};

} // namespace JETProvider
