// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "NMEA/Validity.hpp"
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
        inject_task(curl.GetEventLoop()){};
  ~Glue() noexcept = default;

  void OnTimer(const NMEAInfo &basic) noexcept;

  Data data;

private:
  CurlGlobal &curl;
  PeriodClock clock;
  mutable Mutex mutex;
  Co::InjectTask inject_task;

  Co::InvokeTask CoTick(const NMEAInfo &basic, StaticString<256> url);
  void OnCompletion(std::exception_ptr error) noexcept;
};

} // namespace NetworkWidget
