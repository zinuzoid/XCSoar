// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Glue.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"
#include "NMEA/Info.hpp"
#include "Settings.hpp"
#include "lib/curl/CoRequest.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringStrip.hxx"

#include <string_view>

namespace NetworkWidget
{

void
Glue::OnTimer(const NMEAInfo &basic) noexcept
{
  const NetworkWidgetSettings &settings =
      CommonInterface::GetComputerSettings().network_widget;

  const auto interval = std::chrono::seconds(settings.interval);

  for (unsigned i = 0; i < NetworkWidgetSettings::NETWORK_WIDGET_SLOTS; ++i) {
    Slot &slot = slots[i];
    if (!slot.inject_task && !settings.urls[i].empty() &&
        slot.clock.CheckUpdate(interval))
      slot.inject_task.Start(CoTick(basic, i, settings.urls[i]),
                             BIND_METHOD(slot, &Slot::OnCompletion));
  }
}

Co::InvokeTask
Glue::CoTick(const NMEAInfo &basic, unsigned index, StaticString<256> url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);

  Curl::CoResponse res = co_await Curl::CoRequest(curl, std::move(easy));

  if (res.status != 200)
    throw FmtRuntimeError("NetworkWidget[{}] error status: {} body: {}",
                          index, res.status, res.body);

  // Split the body into up to three lines, trimming trailing CR/whitespace
  // so a CRLF response does not leave carriage returns in the display.
  std::string_view body{res.body};
  std::string lines[3];
  for (auto &line : lines) {
    const auto nl = body.find('\n');
    line.assign(StripRight(body.substr(0, nl)));
    if (nl == std::string_view::npos) {
      body = {};
      break;
    }
    body.remove_prefix(nl + 1);
  }

  Data &data = slots[index].data;
  {
    const std::lock_guard lock{data.mutex};
    data.line1 = std::move(lines[0]);
    data.line2 = std::move(lines[1]);
    data.line3 = std::move(lines[2]);
    data.validity.Update(basic.clock);
  }

  LogFormat("NetworkWidget[%u]::OnCompletion: %s|%s|%s'", index,
            data.line1.c_str(), data.line2.c_str(), data.line3.c_str());
}

void
Glue::Slot::OnCompletion(std::exception_ptr error) noexcept
{
  if (error)
    LogError(error, "NetworkWidget request failed");

  const std::lock_guard lock{data.mutex};
  data.failed = error != nullptr;
}

} // namespace NetworkWidget
