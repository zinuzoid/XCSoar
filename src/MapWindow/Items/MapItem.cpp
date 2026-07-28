// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapItem.hpp"
#include "Engine/Task/ObservationZones/ObservationZonePoint.hpp"

TaskOZMapItem::TaskOZMapItem(int _index, const ObservationZonePoint &_oz,
                             TaskPointType _tp_type, WaypointPtr &&_waypoint)
  :MapItem(Type::TASK_OZ), index(_index), oz(_oz.Clone()),
   tp_type(_tp_type), waypoint(std::move(_waypoint)) {}

TaskOZMapItem::~TaskOZMapItem() noexcept = default;

JETProviderTrafficMapItem::JETProviderTrafficMapItem(const char *_traffic_id,
                                                     const char *_name,
                                                     const char *_code,
                                                     const char *_type,
                                                     int _altitude,
                                                     double _speed,
                                                     double _vspeed,
                                                     double _climb_rate_avg30s,
                                                     int _track,
                                                     uint32_t _epoch,
                                                     FlarmTraffic::AlarmType _alarm_level,
                                                     FlarmColor _color) noexcept
  :MapItem(Type::JET_TRAFFIC), altitude(_altitude), speed(_speed),
   vspeed(_vspeed), climb_rate_avg30s(_climb_rate_avg30s), track(_track),
   epoch(_epoch), alarm_level(_alarm_level), color(_color)
{
  /* the JETProvider parser leaves these null when the CSV field was
     empty */
  traffic_id = _traffic_id != nullptr ? _traffic_id : "";
  name = _name != nullptr ? _name : "";
  code = _code != nullptr ? _code : "";
  type = _type != nullptr ? _type : "";
}
