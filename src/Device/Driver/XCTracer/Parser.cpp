// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "../XCTracer/Internal.hpp"
#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
#include "NMEA/Info.hpp"

/**
 * Parser for the XCTracer Vario
 * For the XCTracer Protocol
 * @see https://www.xctracer.com/en/tech-specs/?oid=1861
 *
 * Native XTRC sentences
 * $XCTRC,2015,1,5,16,34,33,36,46.947508,7.453117,540.32,12.35,270.4,2.78,,,,964.93,98*67
 *
 * $XCTRC,year,month,day,hour,minute,second,centisecond,latitude,longitude,altitude,speedoverground,
 *      course,climbrate,res,res,res,rawpressure,batteryindication*checksum
 *
 * OR in LXWP0 mode with GPS sentences
 * $GPGGA,081158.400,4837.7021,N,00806.2928,E,2,5,1.57,113.9,M,47.9,M,,*5B
 * $LXWP0,N,,119.9,0.16,,,,,,259,,*64
 * $GPRMC,081158.800,A,4837.7018,N,00806.2923,E,2.34,261.89,110815,,,D*69
 */

using std::string_view_literals::operator""sv;

/**
 * Helper functions to parse and check an input field
 * Should these be added as methods to Class CSVLine ?
 * e.g. bool CSVLine::ReadCheckedRange(unsigned &value_r, unsigned min, unsigned max)
 *
 * @param line Input line
 * @param value_r Return parsed and checked value
 * @param min Minimum value to be accepted
 * @param max Maximum value to be accepted
 * @return true if parsed OK and within range
 */
static bool
ReadCheckedRange(NMEAInputLine &line, unsigned &value_r, unsigned min, unsigned max)
{
  double value;
  if (!line.ReadChecked(value))
    return false;

  /* check min/max as floating point to catch input values out of unsigned range */
  if (value < min || value > max)
    return false;

  /* finally we can cast/convert w/o being out of range */
  value_r = (unsigned) value;
  return true;
}

/* same helper as above with params of type double */
static bool
ReadCheckedRange(NMEAInputLine &line,double &value_r, double min, double max)
{
  double value;
  if (!line.ReadChecked(value))
    return false;

  /* check range */
  if (value < min || value > max)
    return false;

  value_r = value;
  return true;
}

static int
ReadCheckedLatitudeLongitude(NMEAInputLine &line, double &value_latitude, double &value_longitude)
{
  int valid_fields = 0;
  double latitude, longitude;
  double epsilon = std::numeric_limits<double>::epsilon();

  valid_fields += ReadCheckedRange(line, latitude, -90.0, 90.0);
  valid_fields += ReadCheckedRange(line, longitude, -180.0, 180.0);

  if(valid_fields == 2 && !(fabs(latitude) < epsilon && fabs(longitude) < epsilon)) {
    value_latitude = latitude;
    value_longitude = longitude;
  } else {
    /**
     * While inactive on the ground XCTracer send the 0.0/0.0 location.
     * We mark as invalid here, so map is not jumping to the 0.0/0.0
     */
    valid_fields = 0;
  }

  return valid_fields;
}

/**
 * the parser for the LXWP0 subset sentence that the XC-Tracer can produce
 */
inline bool
XCTracerDevice::LXWP0(NMEAInputLine &line, NMEAInfo &info)
{
  /*
   *  $LXWP0,N,,119.9,0.16,,,,,,259,,*64
   *  0 logger stored (Y/N)
   *  1 n.u.
   *  2 baroaltitude (m)
   *  3 vario (m/s)
   *  4-8 n.u.
   *  9 course (0..360)
   * 10 n.u
   * 11 n.u.
   */

  line.Skip(2);

  double value;
  /* read baroaltitude */
  if (line.ReadChecked(value))
    /* XC-Tracer sends uncorrected altitude above 1013.25hPa here */
    info.ProvidePressureAltitude(value);

  /* read vario */
  if (line.ReadChecked(value))
    info.ProvideTotalEnergyVario(value);

  line.Skip(5);

  /* read course */
  if (Angle track; line.ReadBearing(track)) {
    info.track = track;
    info.track_available.Update(info.clock);
  }

  return true;
}

/**
 * the parser for the XCTRC sentence
 */
inline bool
XCTracerDevice::XCTRC(NMEAInputLine &line, NMEAInfo &info)
{
  /*
   * $XCTRC,year,month,day,hour,minute,second,centisecond,latitude,longitude,
   *  altitude,speedoverground,course,climbrate,res,res,res,rawpressure,
   *  batteryindication*checksum
   * $XCTRC,2015,8,11,10,56,23,80,48.62825,8.104885,129.4,0.01,322.76,-0.05,,,,997.79,77*66
   */

  /* count the number of valid fields. If not as expected incr nmea error counter */
  int valid_fields = 0;

  /**
   * parse the date
   * parse and absorb all three values, even if one or more is empty, e.g. ",,13"
   */
  unsigned year, month, day;
  valid_fields += ReadCheckedRange(line,year,1800,2500);
  valid_fields += ReadCheckedRange(line,month,1,12);
  valid_fields += ReadCheckedRange(line,day,1,31);

  /**
   * parse the time
   * parse and check all four values, even if one or more is empty, e.g. ",,33,"
   */
  unsigned hour, minute, second, centisecond;
  valid_fields += ReadCheckedRange(line,hour,0,23);
  valid_fields += ReadCheckedRange(line,minute,0,59);
  valid_fields += ReadCheckedRange(line,second,0,59);
  valid_fields += ReadCheckedRange(line,centisecond,0,99);

  /**
   * parse the GPS fix
   * parse and check both  values, even if one or more is empty, e.g. ",3.1234,"
   */
  double latitude, longitude;
  valid_fields += ReadCheckedLatitudeLongitude(line, latitude, longitude);

  /**
   * we only want to accept GPS fixes with completely sane values
   * for date, time and lat/long
   */
  if (valid_fields == 3+4+2) {
    /* now convert the date, time, lat & long fields to the internal format */
    BrokenDate date = BrokenDate(year,month,day);
    const TimeStamp time{
      FloatDuration{std::chrono::hours{hour}} +
      FloatDuration{std::chrono::minutes{minute}} +
      FloatDuration{std::chrono::seconds{second}} +
      FloatDuration{std::chrono::milliseconds{centisecond * 10}}
    };

    GeoPoint point;
    point.latitude = Angle::Degrees(latitude);
    point.longitude = Angle::Degrees(longitude);

    /**
     * only update GPS and date/time if time has advanced
     */
    if (time > last_time || date > last_date) {
      /* update last date/time */
      last_time = time;
      last_date = date;

      /* set time and date_time_utc, don't use ProvideTime() */
      info.time = time;
      info.time_available.Update(info.clock);
      info.date_time_utc.hour = hour;
      info.date_time_utc.minute = minute;
      info.date_time_utc.second = second;

      info.ProvideDate(date);
      info.location = point;
      info.location_available.Update(info.clock);
      info.gps.real = true;
    }
  }

  double value;
  /* read gps altitude */
  if (line.ReadChecked(value)) {
    info.gps_altitude = value;
    info.gps_altitude_available.Update(info.clock);
    valid_fields++;
  }

  /* read spead over ground */
  if (line.ReadChecked(value)) {
    info.ground_speed = value;
    info.ground_speed_available.Update(info.clock);
    valid_fields++;
  }

  /* read course*/
  if (Angle track; line.ReadBearing(track)) {
    /* update only if we're moving .. */
    if (info.MovementDetected()) {
      info.track = track;
      info.track_available.Update(info.clock);
    }
    valid_fields++;
  }

  /* read climbrate */
  if (line.ReadChecked(value)) {
    info.ProvideTotalEnergyVario(value);
    valid_fields++;
  }

  /* skip 3 reserved values */
  line.Skip(3);

  /* read raw pressure */
  if (line.ReadChecked(value)) {
    // convert to pressure
    info.ProvideStaticPressure(AtmosphericPressure::HectoPascal(value));
    valid_fields++;
  }

  /* read battery level */
  if (ReadCheckedRange(line,value,0.0,100.0)) {
    info.battery_level = value;
    info.battery_level_available.Update(info.clock);
    valid_fields++;
  }

  return true;
}

/**
 * the NMEA parser virtual function
 */
bool
XCTracerDevice::ParseNMEA(const char *string, NMEAInfo &info)
{
  if (!VerifyNMEAChecksum(string))
    return false;

  NMEAInputLine line(string);

  const auto type = line.ReadView();
  if (type == "$LXWP0"sv)
    return LXWP0(line, info);

  else if (type == "$XCTRC"sv)
    return XCTRC(line, info);

  else
    return false;
}
