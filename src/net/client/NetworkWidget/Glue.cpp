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
  const NetworkWidgetSettings &settings =
      CommonInterface::GetComputerSettings().network_widget;

  const auto interval = std::chrono::seconds(settings.interval);

  if (!inject_task0 && !settings.urls[0].empty() &&
      clock[0].CheckUpdate(interval))
  {
    inject_task0.Start(CoTick(basic, 0, settings.urls[0]),
                       BIND_THIS_METHOD(OnCompletion0));
  }

  if (!inject_task1 && !settings.urls[1].empty() &&
      clock[1].CheckUpdate(interval))
  {
    inject_task1.Start(CoTick(basic, 1, settings.urls[1]),
                       BIND_THIS_METHOD(OnCompletion1));
  }
}

Co::InvokeTask
Glue::CoTick(const NMEAInfo &basic, unsigned index, StaticString<256> url)
{
  CurlEasy easy{url};
  Curl::Setup(easy);

  Curl::CoResponse res = co_await Curl::CoRequest(curl, std::move(easy));

  if (res.status != 200)
  {
    throw FmtRuntimeError("NetworkWidget[{}] error status: {} body: {}",
                          index, res.status, res.body);
  }

  std::istringstream istr(res.body);
  std::string line;
  {
    if (istr.eof()) throw FmtRuntimeError("NetworkWidget[{}] zero line body", index);

    const std::lock_guard lock{data[index].mutex};
    std::getline(istr, line);
    data[index].line1 = line;

    if (!istr.eof())
    {
      std::getline(istr, line);
      data[index].line2 = line;
    }
    else
    {
      data[index].line2 = "";
    }

    if (!istr.eof())
    {
      std::getline(istr, line);
      data[index].line3 = line;
    }
    else
    {
      data[index].line3 = "";
    }
    data[index].validity.Update(basic.clock);
  }

  LogFormat("NetworkWidget[%u]::OnCompletion: %s|%s|%s'", index,
            data[index].line1.c_str(), data[index].line2.c_str(),
            data[index].line3.c_str());
}

void
Glue::OnCompletion0(std::exception_ptr error) noexcept
{
  if (error) LogError(error, "NetworkWidget[0] request failed");
}

void
Glue::OnCompletion1(std::exception_ptr error) noexcept
{
  if (error) LogError(error, "NetworkWidget[1] request failed");
}

} // namespace NetworkWidget
