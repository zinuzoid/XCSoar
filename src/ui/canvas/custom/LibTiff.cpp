// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LibTiff.hpp"
#include "UncompressedImage.hpp"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"
#include "util/ScopeExit.hxx"

#include <stdexcept>

#include <tiffio.h>

#ifdef USE_GEOTIFF
#include "Geo/Quadrilateral.hpp"

#include <geotiff.h>
#include <geo_normalize.h>
#include <geovalues.h>
#include <xtiffio.h>
#endif

static TIFF *
TiffOpen(Path path, const char *mode)
{
#ifdef USE_GEOTIFF
  XTIFFInitialize();
#endif

#ifdef _UNICODE
  return TIFFOpenW(path.c_str(), mode);
#else
  return TIFFOpen(path.c_str(), mode);
#endif
}

class TiffLoader {
  TIFF *const tiff;

public:
  explicit TiffLoader(Path path)
    :tiff(TiffOpen(path, "r")) {
    if (tiff == nullptr)
      throw std::runtime_error("Failed to open TIFF file");
  }

  ~TiffLoader() {
    TIFFClose(tiff);
  }

  TIFF *Get() {
    return tiff;
  }

  void GetField(uint32_t tag, int &value_r) {
    TIFFGetField(tiff, tag, &value_r);
  }

  void RGBAImageBegin(TIFFRGBAImage &img) {
    char emsg[1024];
    if (!TIFFRGBAImageBegin(&img, tiff, 0, emsg))
      throw std::runtime_error(emsg);
  }
};

/**
 * Validate that TIFF strip data fits within the actual file size.
 * Corrupted or truncated TIFFs can cause libtiff to memcpy past
 * valid memory in DumpModeDecode, resulting in a segfault.
 */
static void
ValidateTiffStrips(TIFF *tiff, uint64_t file_size)
{
  if (TIFFIsTiled(tiff)) {
    ttile_t num_tiles = TIFFNumberOfTiles(tiff);
    if (num_tiles == 0)
      throw std::runtime_error("TIFF file has no tiles");

    uint64_t *offsets = nullptr;
    uint64_t *byte_counts = nullptr;
    if (!TIFFGetField(tiff, TIFFTAG_TILEOFFSETS, &offsets) || !offsets ||
        !TIFFGetField(tiff, TIFFTAG_TILEBYTECOUNTS, &byte_counts) ||
        !byte_counts)
      throw std::runtime_error("TIFF file missing tile metadata");

    for (ttile_t i = 0; i < num_tiles; i++) {
      if (offsets[i] + byte_counts[i] > file_size)
        throw std::runtime_error("TIFF file is truncated");
    }
  } else {
    tstrip_t num_strips = TIFFNumberOfStrips(tiff);
    if (num_strips == 0)
      throw std::runtime_error("TIFF file has no strips");

    uint64_t *offsets = nullptr;
    uint64_t *byte_counts = nullptr;
    if (!TIFFGetField(tiff, TIFFTAG_STRIPOFFSETS, &offsets) || !offsets ||
        !TIFFGetField(tiff, TIFFTAG_STRIPBYTECOUNTS, &byte_counts) ||
        !byte_counts)
      throw std::runtime_error("TIFF file missing strip metadata");

    for (tstrip_t i = 0; i < num_strips; i++) {
      if (offsets[i] + byte_counts[i] > file_size)
        throw std::runtime_error("TIFF file is truncated");
    }
  }
}

static UncompressedImage
LoadTiff(TIFFRGBAImage &img)
{
  if (img.width > 8192 || img.height > 8192)
    throw std::runtime_error("TIFF file is too large");

  std::unique_ptr<uint8_t[]> data(new uint8_t[img.width * img.height * 4]);
  uint32_t *data32 = (uint32_t *)(void *)data.get();

  if (!TIFFRGBAImageGet(&img, data32, img.width, img.height))
    throw std::runtime_error("Failed to copy TIFF data");

  return UncompressedImage(UncompressedImage::Format::RGBA, img.width * 4,
                           img.width, img.height, std::move(data), true);
}

static UncompressedImage
LoadTiff(TiffLoader &tiff, uint64_t file_size)
{
  ValidateTiffStrips(tiff.Get(), file_size);

  TIFFRGBAImage img;
  tiff.RGBAImageBegin(img);

  AtScopeExit(&img) { TIFFRGBAImageEnd(&img); };

  return LoadTiff(img);
}

UncompressedImage
LoadTiff(Path path)
{
  uint64_t file_size = File::GetSize(path);
  if (file_size == 0)
    throw std::runtime_error("TIFF file is empty or inaccessible");

  TiffLoader tiff(path);
  return LoadTiff(tiff, file_size);
}

#ifdef USE_GEOTIFF

static GeoPoint
TiffPixelToGeoPoint(GTIF &gtif, GTIFDefn &defn, double x, double y)
{
  if (!GTIFImageToPCS(&gtif, &x, &y))
    return GeoPoint::Invalid();

  if (defn.Model != ModelTypeGeographic &&
      !GTIFProj4ToLatLong(&defn, 1, &x, &y))
    return GeoPoint::Invalid();

  return GeoPoint(Angle::Degrees(x), Angle::Degrees(y));
}

std::pair<UncompressedImage, GeoQuadrilateral>
LoadGeoTiff(Path path)
{
  TiffLoader tiff(path);

  GeoQuadrilateral bounds;

  {
    auto gtif = GTIFNew(tiff.Get());
    if (gtif == nullptr)
      throw std::runtime_error("Not a GeoTIFF file");

    AtScopeExit(gtif) { GTIFFree(gtif); };

    GTIFDefn defn;
    if (!GTIFGetDefn(gtif, &defn))
      throw std::runtime_error("Failed to parse GeoTIFF metadata");

    int width, height;
    tiff.GetField(TIFFTAG_IMAGEWIDTH, width);
    tiff.GetField(TIFFTAG_IMAGELENGTH, height);

    bounds.top_left = TiffPixelToGeoPoint(*gtif, defn, 0, 0);
    bounds.top_right = TiffPixelToGeoPoint(*gtif, defn, width, 0);
    bounds.bottom_left = TiffPixelToGeoPoint(*gtif, defn, 0, height);
    bounds.bottom_right = TiffPixelToGeoPoint(*gtif, defn, width, height);

    if (!bounds.Check())
      throw std::runtime_error("Invalid GeoTIFF bounds");
  }

  uint64_t file_size = File::GetSize(path);
  if (file_size == 0)
    throw std::runtime_error("GeoTIFF file is empty or inaccessible");

  return std::make_pair(LoadTiff(tiff, file_size), bounds);
}

#endif
