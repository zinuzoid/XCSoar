// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Layer.hpp"

#include <map>
#include <string>

class Path;

/**
 * Decode a Skysight NetCDF data file into an RGBA GeoTIFF suitable
 * for #MapOverlayBitmap, colouring values through #legend (values
 * below the first legend entry become transparent).
 *
 * This call blocks and is CPU/memory intensive; run it on a worker
 * thread (see #SkysightDecoderThread).  The output is written
 * atomically (temporary file + rename).  The input file is deleted
 * in both the success and the failure case so a broken download is
 * fetched again instead of being retried forever.
 *
 * Throws on error.
 *
 * @param varname the NetCDF variable to read; equal to the layer id
 */
void
DecodeNetCDFToGeoTIFF(Path nc_path, Path tif_path,
                      const std::string &varname,
                      const std::map<float, SkysightLegendColor> &legend);
