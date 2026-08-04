// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

namespace WindsMobi { struct Station; }

/**
 * Show a read-only details dialog for a winds.mobi wind station's
 * last measurement.
 */
void
dlgWindStationDetailsShowModal(const WindsMobi::Station &station);
