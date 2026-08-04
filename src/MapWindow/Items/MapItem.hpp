// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Geo/GeoVector.hpp"
#include "FLARM/Id.hpp"
#include "FLARM/Color.hpp"
#include "FLARM/Traffic.hpp"
#include "NMEA/ThermalLocator.hpp"
#include "Weather/Features.hpp"
#include "Engine/Waypoint/Ptr.hpp"
#include "Engine/Airspace/Ptr.hpp"
#include "Engine/Route/ReachResult.hpp"
#include "Tracking/SkyLines/Features.hpp"
#include "util/StaticString.hxx"

#ifdef HAVE_NOAA
#include "Weather/NOAAStore.hpp"
#endif

#include <chrono>

#include <tchar.h>

enum class TaskPointType : uint8_t;

class ObservationZonePoint;

struct MapItem
{
  enum class Type {
    LOCATION,
    ARRIVAL_ALTITUDE,
    SELF,
    TASK_OZ,
#ifdef HAVE_NOAA
    WEATHER,
#endif
    WIND_STATION,
    AIRSPACE,
    THERMAL,
    WAYPOINT,
    TRAFFIC,
#ifdef HAVE_SKYLINES_TRACKING
    SKYLINES_TRAFFIC,
#endif
    TRACE,
    JET_TRAFFIC,
    OVERLAY,
    RASP,
  } type;

protected:
  MapItem(Type _type):type(_type) {}

public:
  /* we need this virtual dummy destructor, because there is code that
     "deletes" MapItem objects without knowing that it's really a
     TaskOZMapItem */
  virtual ~MapItem() noexcept = default;
};

struct LocationMapItem: public MapItem
{
  /**
   * Magic value for "unknown elevation".
   */
  static constexpr double UNKNOWN_ELEVATION = -1e5;

  /**
   * All elevation values below this threshold are considered unknown.
   */
  static constexpr double UNKNOWN_ELEVATION_THRESHOLD = -1e4;

  GeoVector vector;

  /**
   * Terrain elevation of the point.  If that is unknown, it is nan().
   */
  double elevation;

  LocationMapItem(const GeoVector &_vector, double _elevation)
    :MapItem(Type::LOCATION), vector(_vector), elevation(_elevation) {}

  bool HasElevation() const {
    return elevation > UNKNOWN_ELEVATION_THRESHOLD;
  }
};

/**
 * An indirect MapItem that shows at what altitude the clicked location can
 * be reached in straight glide and around terrain obstacles.
 */
struct ArrivalAltitudeMapItem: public MapItem
{
  /**
   * Magic value for "unknown elevation".
   */
  static constexpr double UNKNOWN_ELEVATION = -1e5;

  /**
   * All elevation values below this threshold are considered unknown.
   */
  static constexpr double UNKNOWN_ELEVATION_THRESHOLD = -1e4;

  /**
   * Terrain elevation of the point in MSL.  If that is unknown, it is
   * nan().
   */
  double elevation;

  /** Arrival altitudes [m MSL] */
  ReachResult reach;

  /** Safety height (m) */
  double safety_height;


  ArrivalAltitudeMapItem(double _elevation,
                         ReachResult _reach,
                         double _safety_height)
    :MapItem(Type::ARRIVAL_ALTITUDE),
     elevation(_elevation), reach(_reach), safety_height(_safety_height) {}

  bool HasElevation() const {
    return elevation > UNKNOWN_ELEVATION_THRESHOLD;
  }
};

struct SelfMapItem: public MapItem
{
  GeoPoint location;
  Angle bearing;

  SelfMapItem(const GeoPoint &_location, const Angle _bearing)
    :MapItem(Type::SELF), location(_location), bearing(_bearing) {}
};

struct TaskOZMapItem: public MapItem
{
  int index;
  std::unique_ptr<ObservationZonePoint> oz;
  TaskPointType tp_type;
  WaypointPtr waypoint;

  TaskOZMapItem(int _index, const ObservationZonePoint &_oz,
                TaskPointType _tp_type, WaypointPtr &&_waypoint);
  ~TaskOZMapItem() noexcept override;
};

struct AirspaceMapItem: public MapItem
{
  ConstAirspacePtr airspace;

  template<typename T>
  explicit AirspaceMapItem(T &&_airspace) noexcept
    :MapItem(Type::AIRSPACE), airspace(std::forward<T>(_airspace)) {}
};

struct WaypointMapItem: public MapItem
{
  WaypointPtr waypoint;

  WaypointMapItem(const WaypointPtr &_waypoint)
    :MapItem(Type::WAYPOINT), waypoint(_waypoint) {}
};

#ifdef HAVE_NOAA
struct WeatherStationMapItem: public MapItem
{
  NOAAStore::iterator station;

  WeatherStationMapItem(const NOAAStore::iterator &_station)
    :MapItem(Type::WEATHER), station(_station) {}
};
#endif

struct TrafficMapItem: public MapItem
{
  FlarmId id;
  FlarmColor color;

  TrafficMapItem(FlarmId _id, FlarmColor _color)
    :MapItem(Type::TRAFFIC), id(_id), color(_color) {}
};

#ifdef HAVE_SKYLINES_TRACKING

struct SkyLinesTrafficMapItem : public MapItem
{
  using Time = std::chrono::duration<uint_least32_t, std::chrono::milliseconds::period>;

  uint32_t id;

  Time time_of_day;

  int altitude;

  StaticString<40> name;

  SkyLinesTrafficMapItem(uint32_t _id, Time _time_of_day_ms,
                         int _altitude,
                         const TCHAR *_name)
    :MapItem(Type::SKYLINES_TRAFFIC), id(_id), time_of_day(_time_of_day_ms),
     altitude(_altitude),
     name(_name) {}
};

#endif

/**
 * A target received from the JETProvider radar API.  This is a
 * self-contained snapshot, because the JETProvider::Data map (and the
 * heap strings it points to) is replaced on every poll.
 */
struct JETProviderTrafficMapItem : public MapItem
{
  /** absolute altitude [m]; negative if unknown */
  int altitude;

  /** ground speed [m/s]; negative if unknown */
  double speed;

  /** vertical speed [m/s] */
  double vspeed;

  /** average climb rate over the last 30 seconds [m/s]; negative if
      unknown */
  double climb_rate_avg30s;

  /** true track [degrees]; negative if unknown */
  int track;

  FlarmTraffic::AlarmType alarm_level;

  FlarmColor color;

  /** the radar API's unique target id */
  StaticString<32> traffic_id;

  /** display name / callsign */
  StaticString<40> name;

  /** competition code */
  StaticString<16> code;

  /** aircraft type */
  StaticString<32> type;

  JETProviderTrafficMapItem(const char *_traffic_id,
                            const char *_name, const char *_code,
                            const char *_type,
                            int _altitude, double _speed, double _vspeed,
                            double _climb_rate_avg30s, int _track,
                            FlarmTraffic::AlarmType _alarm_level,
                            FlarmColor _color) noexcept;
};

struct ThermalMapItem: public MapItem
{
  ThermalSource thermal;

  ThermalMapItem(const ThermalSource &_thermal)
    :MapItem(Type::THERMAL), thermal(_thermal) {}
};

/**
 * The live flight trace of a followed pilot, fetched by JETProvider.
 */
struct TraceMapItem: public MapItem
{
  StaticString<64> id;

  unsigned n_points;

  /** index into TrafficLook::trace_pens, matching the map */
  unsigned color_index;

  TraceMapItem(const TCHAR *_id, unsigned _n_points, unsigned _color_index)
    :MapItem(Type::TRACE), id(_id), n_points(_n_points),
     color_index(_color_index) {}
};
