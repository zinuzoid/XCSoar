// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LibTiff.hpp"
#include "UncompressedImage.hpp"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"
#include "util/ScopeExit.hxx"

#include <algorithm>
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
 * Validate that TIFF strip/tile data is consistent before handing the file
 * to libtiff's decoders.  Corrupted or truncated TIFFs can otherwise cause
 * libtiff to memcpy past valid memory in DumpModeDecode, resulting in a
 * segfault that no try/catch can recover from.
 *
 * Two classes of corruption are rejected:
 *  - data that does not fit within the actual file (truncated download), and
 *  - for uncompressed data, a declared strip/tile smaller than the size the
 *    image geometry requires.  DumpModeDecode memcpy's the geometry-derived
 *    size, so a short raw buffer is read past its end.
 */
static void
ValidateTiffStrips(TIFF *tiff, uint64_t file_size)
{
  /* Overflow-safe "offset + count <= file_size": adding the two uint64_t
     values directly can wrap around for a corrupt header and pass the check,
     after which libtiff reads past valid memory. */
  const auto RangeWithinFile = [file_size](uint64_t offset,
                                           uint64_t count) noexcept {
    return count <= file_size && offset <= file_size - count;
  };

  /* The geometry cross-check only applies to uncompressed data; compressed
     strips legitimately hold fewer raw bytes than their decoded size, and the
     observed crash is specifically in DumpModeDecode (uncompressed). */
  uint16_t compression = COMPRESSION_NONE;
  TIFFGetFieldDefaulted(tiff, TIFFTAG_COMPRESSION, &compression);
  const bool uncompressed = compression == COMPRESSION_NONE;

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

    /* tiles are padded to a constant full size */
    const tmsize_t tile_size = uncompressed ? TIFFTileSize(tiff) : 0;
    if (uncompressed && tile_size <= 0)
      throw std::runtime_error("Invalid TIFF tile geometry");

    for (ttile_t i = 0; i < num_tiles; i++) {
      if (!RangeWithinFile(offsets[i], byte_counts[i]))
        throw std::runtime_error("TIFF file is truncated");

      if (uncompressed && byte_counts[i] < (uint64_t)tile_size)
        throw std::runtime_error("TIFF tile smaller than image geometry");
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

    uint32_t height = 0, rows_per_strip = 0;
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
    /* a missing/absurd RowsPerStrip means "whole image in one strip" */
    if (rows_per_strip == 0 || rows_per_strip > height)
      rows_per_strip = height;

    /* number of strips that cover the image height once (PlanarConfig
       separate repeats this set once per sample plane) */
    const uint32_t strips_per_plane =
      rows_per_strip > 0 ? (height + rows_per_strip - 1) / rows_per_strip : 0;

    for (tstrip_t i = 0; i < num_strips; i++) {
      if (!RangeWithinFile(offsets[i], byte_counts[i]))
        throw std::runtime_error("TIFF file is truncated");

      if (uncompressed) {
        /* rows in this strip; the last strip of each plane may be partial */
        uint32_t strip_in_plane = strips_per_plane > 0
          ? (uint32_t)(i % strips_per_plane) : 0;
        uint64_t row0 = (uint64_t)strip_in_plane * rows_per_strip;
        uint32_t nrows = row0 < height
          ? (uint32_t)std::min<uint64_t>(rows_per_strip, height - row0)
          : rows_per_strip;

        /* TIFFVStripSize is exactly the size DumpModeDecode will memcpy */
        tmsize_t expected = TIFFVStripSize(tiff, nrows);
        if (expected <= 0 || byte_counts[i] < (uint64_t)expected)
          throw std::runtime_error("TIFF strip smaller than image geometry");
      }
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
