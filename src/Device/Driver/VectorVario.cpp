// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Driver/VectorVario.hpp"
#include "Device/Driver.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
#include "Units/System.hpp"

using std::string_view_literals::operator""sv;

class VectorVarioDevice : public AbstractDevice {
public:
  bool ParseNMEA(const char *line, NMEAInfo &info) override;
};

bool
VectorVarioDevice::ParseNMEA(const char *string, NMEAInfo &info)
{
  if (!VerifyNMEAChecksum(string))
    return false;

  NMEAInputLine line(string);
  const auto type = line.ReadView();

  if (type != "$PTAS1"sv)
    return false;

  // $PTAS1,CV,AV,baro_alt+2000,TAS
  // CV and AV (vario fields) come from the BLE vario characteristic instead;
  // skip them here so the two sources don't conflict.
  line.Skip(); // current vario
  line.Skip(); // average vario

  double baro_altitude;
  if (line.ReadChecked(baro_altitude))
    info.ProvidePressureAltitude(
        Units::ToSysUnit(baro_altitude - 2000, Unit::FEET));

  double vtas;
  if (line.ReadChecked(vtas))
    info.ProvideTrueAirspeed(Units::ToSysUnit(vtas, Unit::KNOTS));

  return true;
}

static Device *
VectorVarioCreateOnPort([[maybe_unused]] const DeviceConfig &config,
                        [[maybe_unused]] Port &com_port)
{
  return new VectorVarioDevice();
}

const struct DeviceRegister vector_vario_driver = {
  _T("VectorVario"),
  _T("Vector Vario"),
  0,
  VectorVarioCreateOnPort,
};
