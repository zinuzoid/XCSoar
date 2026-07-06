// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Layer.hpp"
#include "Session.hpp"
#include "co/Task.hxx"
#include "system/Path.hpp"

#include <ctime>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class CurlGlobal;

/**
 * Stateless wrappers around the skysight.io HTTP API: one coroutine
 * per endpoint plus pure JSON parsers.  All functions are safe to
 * call from any thread; the coroutines run on the #CurlGlobal event
 * loop and all parameters are passed by value.
 *
 * Errors are reported by throwing: #RateLimitError on HTTP 429 (the
 * server asks us to stop for this session), #LoginError when
 * credentials are rejected, std::runtime_error otherwise.
 */
namespace SkysightAPI {

/**
 * The server replied with HTTP 429; Skysight requests that clients
 * stop sending requests for the rest of the session.
 */
struct RateLimitError : std::runtime_error {
  RateLimitError()
    :std::runtime_error("Skysight rate limit (HTTP 429)") {}
};

/** The "/auth" endpoint rejected the credentials. */
struct LoginError : std::runtime_error {
  LoginError()
    :std::runtime_error("Skysight login rejected") {}
};

/** One entry of the "/data/last_updated" response. */
struct LayerUpdate {
  std::string layer_id;
  std::time_t time;
};

/** One entry of the "/data" response: a downloadable NetCDF file. */
struct DataFile {
  std::time_t time;
  std::string link;
};

Co::Task<SkysightSession>
Login(CurlGlobal &curl, std::string email, std::string password);

/* raw JSON fetchers; the caller may write the returned body to its
   cache before parsing it */

Co::Task<std::string>
FetchRegionsJson(CurlGlobal &curl, SkysightSession session);

Co::Task<std::string>
FetchLayersJson(CurlGlobal &curl, SkysightSession session,
                std::string region_id);

Co::Task<std::string>
FetchLastUpdatesJson(CurlGlobal &curl, SkysightSession session,
                     std::string region_id);

Co::Task<std::string>
FetchDataIndexJson(CurlGlobal &curl, SkysightSession session,
                   std::string region_id, std::string layer_id,
                   std::time_t from_time);

/**
 * Download one NetCDF data file to #path (written atomically via a
 * temporary file).
 */
Co::Task<void>
DownloadDataFile(CurlGlobal &curl, SkysightSession session,
                 std::string url, AllocatedPath path);

/* pure parsers; they throw std::exception when the document is
   malformed, but silently skip malformed entries */

std::vector<SkysightRegion>
ParseRegionsJson(std::string_view json);

std::vector<SkysightLayer>
ParseLayersJson(std::string_view json);

std::vector<LayerUpdate>
ParseLastUpdatesJson(std::string_view json);

std::vector<DataFile>
ParseDataIndexJson(std::string_view json);

SkysightSession
ParseLoginJson(std::string_view json);

} // namespace SkysightAPI
