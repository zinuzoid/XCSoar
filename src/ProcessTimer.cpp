// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProcessTimer.hpp"
#include "Interface.hpp"
#include "ActionInterface.hpp"
#include "Input/InputQueue.hpp"
#include "Input/InputEvents.hpp"
#include "Device/MultipleDevices.hpp"
#include "Blackboard/DeviceBlackboard.hpp"
#include "time/PeriodClock.hpp"
#include "MainWindow.hpp"
#include "PopupMessage.hpp"
#include "Simulator.hpp"
#include "Replay/Replay.hpp"
#include "InfoBoxes/InfoBoxManager.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "BallastDumpManager.hpp"
#include "Operation/Operation.hpp"
#include "Tracking/TrackingGlue.hpp"
#include "net/client/tim/Glue.hpp"
#include "net/client/NetworkWidget/Glue.hpp"
#include "ui/event/Idle.hpp"
#include "Dialogs/Tracking/CloudEnableDialog.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"
#include "BackendComponents.hpp"
#include "LogFile.hpp"

static void
MessageProcessTimer() noexcept
{
  // don't display messages if airspace warning dialog is active
  if (CommonInterface::main_window->popup != nullptr &&
      CommonInterface::main_window->popup->Render())
    // turn screen on if blanked and receive a new message
    ResetUserIdle();
}

/**
 * Sets the system time to GPS time if not yet done and
 * defined in settings
 */
static void
SystemClockTimer() noexcept
{
#ifdef _WIN32
  const NMEAInfo &basic = CommonInterface::Basic();

  // as soon as we get a fix for the first time, set the
  // system clock to the GPS time.
  static bool sysTimeInitialised = false;

  if (basic.alive &&
      CommonInterface::GetComputerSettings().set_system_time_from_gps
      && basic.gps.real
      /* assume that we only have a valid date and time when we have a
         full GPS fix */
      && basic.location_available
      && !sysTimeInitialised) {
    SYSTEMTIME sysTime;
    ::GetSystemTime(&sysTime);

    sysTime.wYear = (unsigned short)basic.date_time_utc.year;
    sysTime.wMonth = (unsigned short)basic.date_time_utc.month;
    sysTime.wDay = (unsigned short)basic.date_time_utc.day;
    sysTime.wHour = (unsigned short)basic.date_time_utc.hour;
    sysTime.wMinute = (unsigned short)basic.date_time_utc.minute;
    sysTime.wSecond = (unsigned short)basic.date_time_utc.second;
    sysTime.wMilliseconds = 0;
    ::SetSystemTime(&sysTime);

    sysTimeInitialised =true;
  } else if (!basic.alive)
    /* set system clock again after a device reconnect; the new device
       may have a better GPS time */
    sysTimeInitialised = false;
#else
  // XXX
#endif
}

static void
SystemProcessTimer() noexcept
{
  SystemClockTimer();
}

static void
BlackboardProcessTimer() noexcept
{
  backend_components->device_blackboard->ExpireWallClock();
  XCSoarInterface::ExchangeBlackboard();
}

static void
BallastDumpProcessTimer() noexcept
{
  ComputerSettings &settings_computer =
    CommonInterface::SetComputerSettings();

  GlidePolar &glide_polar = settings_computer.polar.glide_polar_task;

  static BallastDumpManager ballast_manager;

  // Start/Stop the BallastDumpManager
  ballast_manager.SetEnabled(settings_computer.polar.ballast_timer_active);

  // If the BallastDumpManager is not enabled we must not call Update()
  if (!ballast_manager.IsEnabled())
    return;

  if (!ballast_manager.Update(glide_polar, settings_computer.plane.dump_time))
    // Plane is dry now -> disable ballast_timer
    settings_computer.polar.ballast_timer_active = false;

  if (backend_components->protected_task_manager != nullptr)
    backend_components->protected_task_manager->SetGlidePolar(glide_polar);
}

static void
ProcessAutoBugs() noexcept
{
  /**
   * Increase the bugs value every hour.
   */
  static constexpr FloatDuration interval = std::chrono::hours{1};

  /**
   * Decrement the bugs setting by 1%.
   */
  static constexpr double decrement(0.01);

  /**
   * Don't go below this bugs setting.
   */
  static constexpr double min_bugs(0.7);

  /**
   * The time stamp (from FlyingState::flight_time) when we last
   * increased the bugs value automatically.
   */
  static FloatDuration last_auto_bugs;

  const FlyingState &flight = CommonInterface::Calculated().flight;
  const PolarSettings &polar = CommonInterface::GetComputerSettings().polar;

  if (!flight.flying)
    /* reset when not flying */
    last_auto_bugs = {};
  else if (!polar.auto_bugs)
    /* feature is disabled */
    last_auto_bugs = flight.flight_time;
  else if (flight.flight_time >= last_auto_bugs + interval &&
           polar.bugs > min_bugs) {
    last_auto_bugs = flight.flight_time;
    ActionInterface::SetBugs(std::max(polar.bugs - decrement, min_bugs));
  }
}

static void
ProcessFuelBurn() noexcept
{
  const auto &flight = CommonInterface::Calculated().flight;
  const auto &plane = CommonInterface::GetComputerSettings().plane;

  static int last_flight_time = std::chrono::duration_cast<std::chrono::seconds>(flight.flight_time).count();

  if (!plane.is_powered || !flight.flying) {
    return;
  }

  if (last_flight_time == std::chrono::duration_cast<std::chrono::seconds>(flight.flight_time).count()) {
    return;
  }
  last_flight_time = std::chrono::duration_cast<std::chrono::seconds>(flight.flight_time).count();

  if (plane.fuel_consumption <= 0.0 || plane.fuel_onboard <= 0.0) {
    return;
  }
  
  {
    const double decremental = plane.fuel_consumption / 60.0 / 60.0;
    ActionInterface::SetFuelOnboard(plane.fuel_onboard - decremental);
  }
}

static void
SettingsProcessTimer() noexcept
{
  CloudEnableDialog();
  BallastDumpProcessTimer();
  ProcessAutoBugs();
  ProcessFuelBurn();
}

static void
CommonProcessTimer() noexcept
{
  BlackboardProcessTimer();

  SettingsProcessTimer();

  InfoBoxManager::ProcessTimer();
  InputEvents::ProcessTimer();

  MessageProcessTimer();
  SystemProcessTimer();
}

static void
ConnectionProcessTimer() noexcept
{
  if (backend_components->devices == nullptr)
    return;

  static bool connected_last = false;
  static bool location_last = false;
  static bool wait_connect = false;

  const NMEAInfo &basic = CommonInterface::Basic();

  const bool connected_now = basic.alive,
    location_now = basic.location_available;
  if (connected_now) {
    if (location_now) {
      wait_connect = false;
    } else if (!connected_last || location_last) {
      // waiting for lock first time
      InputEvents::processGlideComputer(GCE_GPS_FIX_WAIT);
    }
  } else if (!connected_last) {
    if (!wait_connect) {
      // gps is waiting for connection first time
      wait_connect = true;
      InputEvents::processGlideComputer(GCE_GPS_CONNECTION_WAIT);
    }
  }

  connected_last = connected_now;
  location_last = location_now;

  /* this OperationEnvironment instance must be persistent, because
     DeviceDescriptor::Open() is asynchronous */
  static QuietOperationEnvironment env;
  backend_components->devices->AutoReopen(env);
}

void
ProcessTimer() noexcept
{
  CommonProcessTimer();

  if (!is_simulator()) {
    // now check GPS status
    if (backend_components->devices != nullptr)
      backend_components->devices->Tick();

    // also service replay logger
    if (backend_components->replay && backend_components->replay->IsActive()) {
      if (CommonInterface::MovementDetected())
        backend_components->replay->Stop();
    }

    ConnectionProcessTimer();
  } else {
    static PeriodClock m_clock;

    if (backend_components->replay && backend_components->replay->IsActive()) {
      m_clock.Update();
    } else if (m_clock.Elapsed() >= std::chrono::seconds(1)) {
      m_clock.Update();
      backend_components->device_blackboard->ProcessSimulation();
    } else if (!m_clock.IsDefined())
      m_clock.Update();
  }

  if (net_components != nullptr) {
#ifdef HAVE_TRACKING
    if (net_components->tracking) {
      net_components->tracking->SetSettings(CommonInterface::GetComputerSettings().tracking);
      net_components->tracking->OnTimer(CommonInterface::Basic(), CommonInterface::Calculated());
    }
#endif

#ifdef HAVE_HTTP
    if (net_components->tim != nullptr &&
        CommonInterface::GetComputerSettings().weather.enable_tim)
      net_components->tim->OnTimer(CommonInterface::Basic());
    if (net_components->networkWidget != nullptr)
      net_components->networkWidget->OnTimer(CommonInterface::Basic());
#endif
  }
}
