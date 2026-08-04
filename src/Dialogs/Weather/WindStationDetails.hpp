// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

namespace JETProvider { struct WindStation; }

/**
 * Show a read-only details dialog for a wind station's last
 * measurement.
 */
void
dlgWindStationDetailsShowModal(const JETProvider::WindStation &station);
