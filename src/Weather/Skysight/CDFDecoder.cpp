// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CDFDecoder.hpp"
#include "LogFile.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/AllocatedArray.hxx"

#ifdef ANDROID
#include <netcdfcpp.h>
#include <geotiffio.h>
#include <xtiffio.h>
#else
#include <netcdf>
#include <geotiff/geotiffio.h>
#include <geotiff/xtiffio.h>
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

static void
TiffErrorHandler(const char *module, const char *fmt, va_list ap)
{
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  LogFormat("libtiff %s: %s", module != nullptr ? module : "", buffer);
}

struct DecodedGrid {
  std::size_t lat_size, lon_size;
  AllocatedArray<double> lat_vals, lon_vals, var_vals;
  double fill_value;
  float var_offset, var_scale;
};

/**
 * Read the NetCDF file into memory.  Throws on error.
 */
static DecodedGrid
ReadNetCDF(Path nc_path, const std::string &varname)
{
  DecodedGrid grid;

#ifdef ANDROID
  /* the Android build uses the legacy netcdfcpp API */
  NcError nc_error{NcError::silent_nonfatal};
  NcFile data_file(nc_path.c_str(), NcFile::FileMode::ReadOnly);
  if (!data_file.is_valid())
    throw std::runtime_error("cannot open NetCDF file");

  const auto *lat_dim = data_file.get_dim("lat");
  const auto *lon_dim = data_file.get_dim("lon");
  if (lat_dim == nullptr || lon_dim == nullptr)
    throw std::runtime_error("NetCDF file without lat/lon dimensions");

  grid.lat_size = lat_dim->size();
  grid.lon_size = lon_dim->size();
#else
  netCDF::NcFile data_file(nc_path.c_str(), netCDF::NcFile::read);
  if (data_file.isNull())
    throw std::runtime_error("cannot open NetCDF file");

  grid.lat_size = data_file.getDim("lat").getSize();
  grid.lon_size = data_file.getDim("lon").getSize();
#endif

  if (grid.lat_size == 0 || grid.lon_size == 0)
    throw std::runtime_error("NetCDF file with empty lat/lon dimensions");

  grid.lat_vals.ResizeDiscard(grid.lat_size);
  grid.lon_vals.ResizeDiscard(grid.lon_size);
  grid.var_vals.ResizeDiscard(grid.lat_size * grid.lon_size);

#ifdef ANDROID
  auto *lat_var = data_file.get_var("lat");
  auto *lon_var = data_file.get_var("lon");
  auto *data_var = data_file.get_var(varname.c_str());
  if (lat_var == nullptr || lon_var == nullptr || data_var == nullptr ||
      !data_var->is_valid())
    throw std::runtime_error("NetCDF variable missing");

  lat_var->get(&grid.lat_vals[0], (long)grid.lat_size);
  lon_var->get(&grid.lon_vals[0], (long)grid.lon_size);
  data_var->get(&grid.var_vals[0], (long)grid.lat_size,
                (long)grid.lon_size);

  auto *fill_att = data_var->get_att("_FillValue");
  auto *offset_att = data_var->get_att("add_offset");
  auto *scale_att = data_var->get_att("scale_factor");
  if (fill_att == nullptr || offset_att == nullptr || scale_att == nullptr)
    throw std::runtime_error("NetCDF variable attributes missing");

  grid.fill_value = fill_att->values()->as_double(0);
  grid.var_offset = offset_att->values()->as_float(0);
  grid.var_scale = scale_att->values()->as_float(0);
#else
  netCDF::NcVar data_var = data_file.getVar(varname);
  if (data_var.isNull())
    throw std::runtime_error("NetCDF variable missing");

  data_file.getVar("lat").getVar(&grid.lat_vals[0]);
  data_file.getVar("lon").getVar(&grid.lon_vals[0]);
  data_var.getVar(&grid.var_vals[0]);

  data_var.getAtt("_FillValue").getValues(&grid.fill_value);
  data_var.getAtt("add_offset").getValues(&grid.var_offset);
  data_var.getAtt("scale_factor").getValues(&grid.var_scale);
#endif

  return grid;
}

/**
 * Write the grid as an RGBA GeoTIFF.  Throws on error.
 */
static void
WriteGeoTIFF(Path tif_path, const DecodedGrid &grid,
             const std::map<float, SkysightLegendColor> &legend)
{
  const double lat_min = grid.lat_vals[grid.lat_size - 1];
  const double lat_max = grid.lat_vals[0];
  const double lon_min = grid.lon_vals[0];
  const double lon_max = grid.lon_vals[grid.lon_size - 1];
  const double lon_scale = (lon_max - lon_min) / grid.lon_size;
  const double lat_scale = (lat_max - lat_min) / grid.lat_size;

  TIFF *tf = XTIFFOpen(tif_path.c_str(), "w");
  if (tf == nullptr)
    throw std::runtime_error("XTIFFOpen failed");

  GTIF *gt = GTIFNew(tf);
  if (gt == nullptr) {
    TIFFClose(tf);
    throw std::runtime_error("GTIFNew failed");
  }

  double tp_topleft[6] = {0, 0, 0, lon_min, lat_min, 0};
  double pix_scale[3] = {lon_scale, -lat_scale, 0};

  constexpr int samplesperpixel = 4;
  TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, grid.lon_size);
  TIFFSetField(tf, TIFFTAG_IMAGELENGTH, grid.lat_size);
  TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
  TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(tf, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  TIFFSetField(tf, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

  TIFFSetField(tf, TIFFTAG_GEOTIEPOINTS, 6, tp_topleft);
  TIFFSetField(tf, TIFFTAG_GEOPIXELSCALE, 3, pix_scale);

  GTIFKeySet(gt, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelTypeGeographic);
  GTIFKeySet(gt, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
  GTIFKeySet(gt, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);
  GTIFKeySet(gt, GTCitationGeoKey, TYPE_ASCII, 27,
             "Generated by XCSoar with data from skysight.io");
  GTIFKeySet(gt, GeogLinearUnitsGeoKey, TYPE_SHORT, 1, Linear_Meter);
  GTIFKeySet(gt, GeogAngularUnitsGeoKey, TYPE_SHORT, 1, Angular_Degree);

  const tsize_t linebytes = samplesperpixel * grid.lon_size;
  const tsize_t row_size = std::max(linebytes, TIFFScanlineSize(tf));
  const auto row = std::make_unique<unsigned char[]>(row_size);

  TIFFSetField(tf, TIFFTAG_ROWSPERSTRIP,
               TIFFDefaultStripSize(tf, linebytes));

  bool success = true;
  for (int y = (int)grid.lat_size - 1; y >= 0; y--) {
    /* zero-fill so unused data points stay transparent */
    memset(row.get(), 0, row_size);

    for (unsigned x = 0; x < grid.lon_size; x++) {
      const std::size_t index = ((std::size_t)y * grid.lon_size) + x;
      const unsigned rb = x * samplesperpixel;

      if (grid.var_vals[index] == grid.fill_value)
        continue;

      const double point_value =
        (grid.var_vals[index] * grid.var_scale) + grid.var_offset;
      if (point_value > legend.begin()->first) {
        auto color = legend.lower_bound(point_value);
        --color;

        row[rb] = color->second.red;
        row[rb + 1] = color->second.green;
        row[rb + 2] = color->second.blue;
        row[rb + 3] = 255;
      }
    }

    if (TIFFWriteScanline(tf, row.get(),
                          (uint32_t)(grid.lat_size - (y + 1)), 0) != 1) {
      success = false;
      break;
    }
  }

  if (success)
    GTIFWriteKeys(gt);

  TIFFClose(tf);
  GTIFFree(gt);

  if (!success)
    throw std::runtime_error("TIFFWriteScanline failed");
}

void
DecodeNetCDFToGeoTIFF(Path nc_path, Path tif_path,
                      const std::string &varname,
                      const std::map<float, SkysightLegendColor> &legend)
{
  if (legend.empty())
    throw std::runtime_error("layer without a legend");

  TIFFSetErrorHandler(TiffErrorHandler);
  TIFFSetWarningHandler(TiffErrorHandler);

  const auto tmp_path = tif_path.WithSuffix(".tmp");

  try {
    const auto grid = ReadNetCDF(nc_path, varname);
    WriteGeoTIFF(tmp_path, grid, legend);
  } catch (...) {
    File::Delete(tmp_path);
    File::Delete(nc_path);
    throw;
  }

  File::Delete(nc_path);
  if (!File::Replace(tmp_path, tif_path)) {
    File::Delete(tmp_path);
    throw std::runtime_error("cannot rename decoded GeoTIFF");
  }
}
