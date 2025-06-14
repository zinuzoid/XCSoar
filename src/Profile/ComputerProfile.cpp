// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ComputerProfile.hpp"
#include "AirspaceConfig.hpp"
#include "TaskProfile.hpp"
#include "TrackingProfile.hpp"
#include "WeatherProfile.hpp"
#include "Keys.hpp"
#include "ContestProfile.hpp"
#include "Map.hpp"
#include "Computer/Settings.hpp"
#include "JETProviderProfile.hpp"
#include "NetworkWidgetProfile.hpp"

namespace Profile {
  static void Load(const ProfileMap &map, WindSettings &settings);
  static void Load(const ProfileMap &map, PolarSettings &settings);
  static void Load(const ProfileMap &map, LoggerSettings &settings);
  static void Load(const ProfileMap &map, TeamCodeSettings &settings);
  static void Load(const ProfileMap &map, PlacesOfInterestSettings &settings);
  static void Load(const ProfileMap &map, FeaturesSettings &settings);
  static void Load(const ProfileMap &map, CirclingSettings &settings);
  static void Load(const ProfileMap &map, WaveSettings &settings);
  static void Load(const ProfileMap &map, WeGlideSettings &settings);
};

void
Profile::Load(const ProfileMap &map, WindSettings &settings)
{
  unsigned auto_wind_mode = settings.GetLegacyAutoWindMode();
  if (map.Get(ProfileKeys::AutoWind, auto_wind_mode))
    settings.SetLegacyAutoWindMode(auto_wind_mode);

  map.Get(ProfileKeys::ExternalWind, settings.external_wind);
}

void
Profile::Load(const ProfileMap &map, PolarSettings &settings)
{
  double degradation;
  if (map.Get(ProfileKeys::PolarDegradation, degradation) &&
      degradation >= 0.5 && degradation <= 1)
    settings.SetDegradationFactor(degradation);

  map.Get(ProfileKeys::AutoBugs, settings.auto_bugs);
}

void
Profile::Load(const ProfileMap &map, LoggerSettings &settings)
{
  map.Get(ProfileKeys::LoggerTimeStepCruise, settings.time_step_cruise);
  map.Get(ProfileKeys::LoggerTimeStepCircling, settings.time_step_circling);

  if (!map.GetEnum(ProfileKeys::AutoLogger, settings.auto_logger)) {
    // Legacy
    bool disable_auto_logger;
    if (map.Get(ProfileKeys::DisableAutoLogger, disable_auto_logger))
      settings.auto_logger =
          disable_auto_logger ? LoggerSettings::AutoLogger::OFF :
                                LoggerSettings::AutoLogger::ON;
  }

  map.Get(ProfileKeys::LoggerID, settings.logger_id);
  map.Get(ProfileKeys::PilotName, settings.pilot_name);
  map.Get(ProfileKeys::CoPilotName, settings.copilot_name);
  map.Get(ProfileKeys::CrewWeightTemplate, settings.crew_mass_template);
  map.Get(ProfileKeys::EnableFlightLogger, settings.enable_flight_logger);
  map.Get(ProfileKeys::EnableNMEALogger, settings.enable_nmea_logger);
}

void
Profile::Load(const ProfileMap &map, WeGlideSettings &settings)
{
  map.Get(ProfileKeys::WeGlideEnabled, settings.enabled);
  map.Get(ProfileKeys::WeGlideAutomaticUpload, settings.automatic_upload);
  map.Get(ProfileKeys::WeGlidePilotID, settings.pilot_id);

  const char *date = map.Get(ProfileKeys::WeGlidePilotBirthDate);
  if (date != nullptr) {
    unsigned day, month, year;
    if (sscanf(date, "%04u-%02u-%02u", &year, &month, &day) == 3)
      settings.pilot_birthdate = {year, month, day};
  }
}

void
Profile::Load(const ProfileMap &map, TeamCodeSettings &settings)
{
  map.Get(ProfileKeys::TeamcodeRefWaypoint, settings.team_code_reference_waypoint);
}

void
Profile::Load(const ProfileMap &map, PlacesOfInterestSettings &settings)
{
  map.Get(ProfileKeys::HomeWaypoint, settings.home_waypoint);
  settings.home_location_available =
    map.GetGeoPoint(ProfileKeys::HomeLocation, settings.home_location);
}

void
Profile::Load(const ProfileMap &map, FeaturesSettings &settings)
{
  map.GetEnum(ProfileKeys::FinalGlideTerrain, settings.final_glide_terrain);
  map.Get(ProfileKeys::BlockSTF, settings.block_stf_enabled);
  map.Get(ProfileKeys::EnableNavBaroAltitude, settings.nav_baro_altitude_enabled);
}

void
Profile::Load(const ProfileMap &map, CirclingSettings &settings)
{
  map.Get(ProfileKeys::EnableExternalTriggerCruise,
          settings.external_trigger_cruise_enabled);
  map.Get(ProfileKeys::CruiseToCirclingModeSwitchThreshold,
          settings.cruise_to_circling_mode_switch_threshold);
  map.Get(ProfileKeys::CirclingToCruiseModeSwitchThreshold,
          settings.circling_to_cruise_mode_switch_threshold);
}

void
Profile::Load(const ProfileMap &map, WaveSettings &settings)
{
  map.Get(ProfileKeys::WaveAssistant, settings.enabled);
}

static bool
LoadUTCOffset(const ProfileMap &map, RoughTimeDelta &value_r)
{
  /* NOTE: Until 6.2.4 utc_offset was stored as a positive int in the
     settings file (with negative offsets stored as "utc_offset + 24 *
     3600").  Later versions will create a new signed settings key. */

  int value;
  if (map.Get(ProfileKeys::UTCOffsetSigned, value)) {
  } else if (map.Get(ProfileKeys::UTCOffset, value)) {
    if (value > 12 * 3600)
      value = (value % (24 * 3600)) - 24 * 3600;
  } else
    /* no profile value present */
    return false;

  if (value > 13 * 3600 || value < -13 * 3600)
    /* illegal value */
    return false;

  value_r = RoughTimeDelta::FromSeconds(value);
  return true;
}

void
Profile::Load(const ProfileMap &map, ComputerSettings &settings)
{
  Load(map, settings.wind);
  Load(map, settings.polar);
  Load(map, settings.team_code);
  Load(map, settings.poi);
  Load(map, settings.features);
  Load(map, settings.airspace);
  Load(map, settings.circling);
  Load(map, settings.wave);

  map.GetEnum(ProfileKeys::AverEffTime, settings.average_eff_time);

  map.Get(ProfileKeys::SetSystemTimeFromGPS, settings.set_system_time_from_gps);

  LoadUTCOffset(map, settings.utc_offset);

  Load(map, settings.task);
  Load(map, settings.contest);
  Load(map, settings.logger);
  Load(map, settings.weglide);

#ifdef HAVE_TRACKING
  Load(map, settings.tracking);
#endif

  Load(map, settings.weather);
  Load(map, settings.jet_provider_setting);
  Load(map, settings.network_widget);
}
