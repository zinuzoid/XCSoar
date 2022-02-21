// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlideRatioFormatter.hpp"
#include "util/StringFormat.hpp"

#include <cassert>
#include <math.h>

void
FormatGlideRatio(TCHAR *buffer, size_t size, double gr, const TCHAR *suffix)
{
  assert(buffer != NULL);
  assert(size >= 8);

  if(suffix == NULL) {
    StringFormat(buffer, size,
              fabs(gr) < 100 ? _T("%.1f") : _T("%.0f"), (double) gr);
  } else {
    StringFormat(buffer, size,
             fabs(gr) < 100 ? _T("%s%.1f") : _T("%s%.0f"), suffix, (double) gr);
  }

}
