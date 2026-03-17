// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "NMEA/Validity.hpp"
#include "Settings.hpp"
#include "co/InjectTask.hxx"
#include "event/DeferEvent.hxx"
#include "lib/curl/Global.hxx"
#include "thread/Mutex.hxx"
#include "time/PeriodClock.hpp"
#include "util/StaticString.hxx"

#include <vector>

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
};

class Glue
{

public:
  explicit Glue(CurlGlobal &_curl) noexcept
      : curl(_curl),
        inject_task0(curl.GetEventLoop()),
        inject_task1(curl.GetEventLoop()){};
  ~Glue() noexcept = default;

  void OnTimer(const NMEAInfo &basic) noexcept;

  Data data[NetworkWidgetSettings::NETWORK_WIDGET_SLOTS];

private:
  CurlGlobal &curl;
  PeriodClock clock[NetworkWidgetSettings::NETWORK_WIDGET_SLOTS];
  mutable Mutex mutex;
  Co::InjectTask inject_task0;
  Co::InjectTask inject_task1;

  Co::InvokeTask CoTick(const NMEAInfo &basic, unsigned index,
                        StaticString<256> url);
  void OnCompletion0(std::exception_ptr error) noexcept;
  void OnCompletion1(std::exception_ptr error) noexcept;
};

} // namespace NetworkWidget
