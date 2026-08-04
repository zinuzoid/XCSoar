// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Station.hpp"
#include "co/InjectTask.hxx"
#include "thread/Mutex.hxx"
#include "time/PeriodClock.hpp"

#include <vector>

class CurlGlobal;

/**
 * Client for the winds.mobi wind station API
 * (https://winds.mobi/api/2.3/doc).  Polls stations near the current
 * map view and publishes them for the map to draw.
 *
 * Modelled on TIM::Glue (net/client/tim/Glue.hpp): all viewport
 * reading happens in OnTimer() on the UI thread; the coroutine
 * receives only by-value parameters and never touches UI-thread state
 * across a co_await.
 */
namespace WindsMobi {

class Glue {
  CurlGlobal &curl;

  PeriodClock clock;

  mutable Mutex mutex;

  std::vector<Station> stations;

  Co::InjectTask inject_task;

  /** the query center/radius of the last request sent, used to detect
      whether the map has panned/zoomed far enough to justify an early
      refresh */
  GeoPoint last_query_center = GeoPoint::Invalid();
  double last_query_radius = 0;

  unsigned total_requests = 0;

  /** stop polling for the rest of this session if something has gone
      badly wrong (e.g. the server keeps failing); guards against a
      runaway request loop */
  bool emergency_stop = false;

public:
  explicit Glue(CurlGlobal &_curl) noexcept;
  ~Glue() noexcept;

  auto Lock() const noexcept {
    return std::lock_guard{mutex};
  }

  /**
   * Must lock the #mutex while accessing the returned reference.
   */
  const auto &Get() const noexcept {
    return stations;
  }

  /**
   * Called regularly (e.g. every 500 ms) from the UI thread.
   */
  void OnTimer() noexcept;

private:
  Co::InvokeTask Start(GeoPoint center, double radius);
  void OnCompletion(std::exception_ptr error) noexcept;
};

} // namespace WindsMobi
