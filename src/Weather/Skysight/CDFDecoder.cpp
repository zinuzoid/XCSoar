/*
Copyright_License {
  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "CDFDecoder.hpp"

#ifdef ANDROID
#include <netcdfcpp.h>
#include <geotiffio.h>
#include <xtiffio.h>
#else
#include <netcdf>
#include <geotiff/geotiffio.h>
#include <geotiff/xtiffio.h>
#endif

#include "SkysightAPI.hpp"
#include "util/AllocatedArray.hxx"
#include "system/FileUtil.hpp"
#include "LogFile.hpp"

static void tiff_errorhandler(const char* module, const char* fmt, va_list ap)
{
  LogFormat("%s", module);
  LogFormat(fmt, ap);
}

void CDFDecoder::DecodeAsync()
{
  std::lock_guard<Mutex> lock(mutex);

  Trigger();
}

void CDFDecoder::Done()
{
  StandbyThread::LockStop();
}

CDFDecoder::Status CDFDecoder::GetStatus()
{
  mutex.lock();
  CDFDecoder::Status s = status;
  mutex.unlock();
  return s;
}

void CDFDecoder::Tick() noexcept
{
  status = Status::Busy;
  mutex.unlock();

  TIFFSetErrorHandler(tiff_errorhandler);
  TIFFSetWarningHandler(tiff_errorhandler);
  try {
    Decode();
  } catch(const std::exception &exc) {
    LogFormat("CDFDecoder::Tick error: %s", exc.what());
    DecodeError();
  }
}

bool CDFDecoder::DecodeError() {
  MakeCallback(false);
  mutex.lock();
  status = Status::Error;
  mutex.unlock();
  return true;
}

bool CDFDecoder::DecodeSuccess()
{
  MakeCallback(true);
  mutex.lock();
  status = Status::Complete;
  mutex.unlock();
  return true;
}

bool CDFDecoder::Decode()
{
#ifdef ANDROID
  NcFile data_file(path.c_str(), NcFile::FileMode::ReadOnly);
  if (!data_file.is_valid())
    return DecodeError();

  size_t lat_size = data_file.get_dim("lat")->size();
  size_t lon_size = data_file.get_dim("lon")->size();
#else
  netCDF::NcFile data_file(path.c_str(), netCDF::NcFile::read);
  if (data_file.isNull())
    return DecodeError();

  size_t lat_size = data_file.getDim("lat").getSize();
  size_t lon_size = data_file.getDim("lon").getSize();
#endif

  AllocatedArray<double>lat_vals(lat_size);
  AllocatedArray<double>lon_vals(lon_size);
  AllocatedArray<double>var_vals(lat_size * lon_size);

#ifdef ANDROID
  data_file.get_var("lat")->get(&lat_vals[0], lat_size);
  data_file.get_var("lon")->get(&lon_vals[0], lon_size);
#else
  data_file.getVar("lat").getVar(&lat_vals[0]);
  data_file.getVar("lon").getVar(&lon_vals[0]);
#endif

  double lat_min = lat_vals[lat_size - 1];
  double lat_max = lat_vals[0];
  double lon_min = lon_vals[0];
  double lon_max = lon_vals[lon_size - 1];
  double lon_scale = (lon_max - lon_min) / lon_size;
  double lat_scale = (lat_max - lat_min) / lat_size;

#ifdef ANDROID
  NcVar *data_var = data_file.get_var(data_varname.c_str());
  if (!data_var->is_valid())
    return DecodeError();

  data_var->get(&var_vals[0], (long)lat_size, (long)lon_size);
  double fill_value = data_var->get_att("_FillValue")->values()->as_double(0);
  float var_offset = data_var->get_att("add_offset")->values()->as_float(0);
  float var_scale = data_var->get_att("scale_factor")->values()->as_float(0);
#else
  netCDF::NcVar data_var = data_file.getVar(data_varname);
  if (data_var.isNull())
    return DecodeError();

  data_var.getVar(&var_vals[0]);

  double fill_value;
  float var_offset, var_scale;
  data_var.getAtt("_FillValue").getValues(&fill_value);
  data_var.getAtt("add_offset").getValues(&var_offset);
  data_var.getAtt("scale_factor").getValues(&var_scale);
#endif

  //Generate GeoTiff
  TIFF *tf = XTIFFOpen(output_path.c_str(), "w");
  if (!tf) {
    throw std::runtime_error("can't XTIFFOpen");
    return DecodeError();
  }

  GTIF *gt = GTIFNew(tf);
  if (!gt) {
    (void)TIFFClose(tf);
    throw std::runtime_error("can't GTIFNew");
    return DecodeError();
  }
  double tp_topleft[6] = {0, 0, 0, lon_min, lat_min, 0};
  double pix_scale[3] = {lon_scale, -lat_scale, 0};

  const int samplesperpixel = 4;
  TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, lon_size);
  TIFFSetField(tf, TIFFTAG_IMAGELENGTH, lat_size);
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
  GTIFKeySet(gt, GTCitationGeoKey, TYPE_ASCII, 27, "Generated by XCSoar with data from skysight.io");
  GTIFKeySet(gt, GeogLinearUnitsGeoKey, TYPE_SHORT, 1, Linear_Meter);
  GTIFKeySet(gt, GeogAngularUnitsGeoKey, TYPE_SHORT, 1, Angular_Degree);

  tsize_t linebytes = samplesperpixel * lon_size;
  unsigned char *row;

  row = (TIFFScanlineSize(tf) > linebytes) ?
    (unsigned char *)_TIFFmalloc(linebytes) :
    (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tf));
  if (!row)
    return DecodeError();

  TIFFSetField(tf, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tf, linebytes));

  int rb;
  unsigned int index = 0;
  bool success = true;
  double point_value;
  for (int y = (int)lat_size - 1; y >= 0; y--) {
    memset(row, 0, linebytes * sizeof(unsigned char)); //ensures unused data points are transparent
    for (unsigned int x = 0; x < lon_size; x++) {
      index = ((unsigned int)y * lon_size) + x;
      rb = x * samplesperpixel;
      if (var_vals[index] != fill_value) {
        point_value = (var_vals[index] * var_scale) + var_offset;
        if (point_value > legend.begin()->first) {
          auto color_index = legend.lower_bound(point_value);
          color_index--;

          row[rb] = color_index->second.Red;
          row[rb+1] = color_index->second.Green;
          row[rb+2] = color_index->second.Blue;
          row[rb+3] = (unsigned char)255;
        }
      }
    }

    if (TIFFWriteScanline(tf, row, (uint32_t)(lat_size - (y + 1)), 0) != 1) {
      success = false;
      break;
    }
  }

  if (success)
    GTIFWriteKeys(gt);

  (void)TIFFClose(tf);

  if (row)
    _TIFFfree(row);

  GTIFFree(gt);

  data_file.close();
  File::Delete(path);
  return (success) ? DecodeSuccess() : DecodeError();

  return false;
}

void CDFDecoder::MakeCallback(bool result)
{
  if (callback) {
    SkysightAPI::MakeCallback(callback, output_path.c_str(), result,
			      data_varname.c_str(), time_index);
  }
}