// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Driver/VectorVario.hpp"
#include "Device/Driver.hpp"

class VectorVarioDevice : public AbstractDevice {};

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
