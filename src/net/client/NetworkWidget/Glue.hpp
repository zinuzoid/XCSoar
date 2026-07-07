// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "NMEA/Validity.hpp"
#include "Settings.hpp"
#include "co/InjectTask.hxx"
#include "lib/curl/Global.hxx"
#include "thread/Mutex.hxx"
#include "time/PeriodClock.hpp"
#include "util/StaticString.hxx"

#include <array>
#include <utility>

struct NMEAInfo;

namespace NetworkWidget
{

struct Data
{
  Mutex mutex;
  std::string line1;
  std::string line2;
  std::string line3;
  Validity validity;

  /**
   * Did the most recent fetch attempt fail?  Set by Slot::OnCompletion;
   * read by the InfoBox to show a visible error state.
   */
  bool failed = false;
};

class Glue
{
  /**
   * One independent poll target.  Bundling the per-slot state here lets
   * the machinery scale with NETWORK_WIDGET_SLOTS instead of duplicating
   * members by hand.
   */
  struct Slot
  {
    Data data;
    PeriodClock clock;
    Co::InjectTask inject_task;
    unsigned index;

    Slot(EventLoop &event_loop, unsigned _index) noexcept
        : inject_task(event_loop), index(_index) {}

    void OnCompletion(std::exception_ptr error) noexcept;
  };

public:
  explicit Glue(CurlGlobal &_curl) noexcept
      : Glue(_curl, std::make_index_sequence<
                        NetworkWidgetSettings::NETWORK_WIDGET_SLOTS>{}) {}
  ~Glue() noexcept = default;

  void OnTimer(const NMEAInfo &basic) noexcept;

  Data &GetData(unsigned index) noexcept { return slots[index].data; }

private:
  template <std::size_t... I>
  Glue(CurlGlobal &_curl, std::index_sequence<I...>) noexcept
      : curl(_curl), slots{Slot{_curl.GetEventLoop(), I}...} {}

  CurlGlobal &curl;
  std::array<Slot, NetworkWidgetSettings::NETWORK_WIDGET_SLOTS> slots;

  Co::InvokeTask CoTick(const NMEAInfo &basic, unsigned index,
                        StaticString<256> url);
};

} // namespace NetworkWidget
