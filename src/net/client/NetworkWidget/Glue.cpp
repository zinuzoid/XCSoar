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

#include <sstream>

namespace NetworkWidget
{

void
Glue::OnTimer([[maybe_unused]] const NMEAInfo &basic) noexcept
{
  if (inject_task) return;

  const NetworkWidgetSettings &settings =
      CommonInterface::GetComputerSettings().network_widget;

  if (settings.url.empty() ||
      !clock.CheckUpdate(std::chrono::seconds(settings.interval)))
    return;

  inject_task.Start(CoTick(basic, settings.url),
                    BIND_THIS_METHOD(OnCompletion));
}

Co::InvokeTask
Glue::CoTick(const NMEAInfo &basic, StaticString<256> url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);

  Curl::CoResponse res = co_await Curl::CoRequest(curl, std::move(easy));
  // LogFormat("url: %s res.body: %s", url.c_str(), res.body.c_str());

  if (res.status != 200)
  {
    throw FmtRuntimeError("NetworkWidget error status: {} body: {}",
                          res.status, res.body);
  }

  std::istringstream istr(res.body);
  std::string line;
  {
    if (istr.eof()) throw FmtRuntimeError("NetworkWidget zero line body");

    const std::lock_guard lock{data.mutex};
    std::getline(istr, line);
    data.line1 = line;

    if (!istr.eof())
    {
      std::getline(istr, line);
      data.line2 = line;
    }
    else
    {
      data.line2 = "";
    }

    if (!istr.eof())
    {
      std::getline(istr, line);
      data.line3 = line;
    }
    else
    {
      data.line3 = "";
    }
    data.validity.Update(basic.clock);
  }

  LogFormat("NetworkWidget::OnCompletion: %s|%s|%s'", data.line1.c_str(),
            data.line2.c_str(), data.line3.c_str());
}

void
Glue::OnCompletion(std::exception_ptr error) noexcept
{
  if (error) LogError(error, "NetworkWidget request failed");
}

} // namespace NetworkWidget
