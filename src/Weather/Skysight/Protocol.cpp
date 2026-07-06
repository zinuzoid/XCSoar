// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Protocol.hpp"
#include "Version.hpp"
#include "lib/curl/CoRequest.hxx"
#include "lib/curl/CoStreamRequest.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "lib/curl/Slist.hxx"
#include "io/FileOutputStream.hxx"

#include <boost/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <stdio.h>

#define SKYSIGHT_BASE_URL "https://skysight.io/api"

/**
 * The value of the "X-API-Key" header sent with "/auth" requests;
 * all other requests send the session key instead.
 */
static constexpr char SKYSIGHT_API_KEY[] = "XCSoar-JET";

namespace SkysightAPI {

static void
SetupEasy(CurlEasy &easy, CurlSlist &headers, const char *api_key)
{
  Curl::Setup(easy);

  /* the server knows this client by the classic product token, not
     by the "XCSoar/<version>" agent set by Curl::Setup() */
  easy.SetUserAgent(XCSoar_ProductToken);

  std::string key_header{"X-API-Key: "};
  key_header += api_key;
  headers.Append(key_header.c_str());
}

static void
CheckStatus(const Curl::CoResponse &response)
{
  if (response.status == 429)
    throw RateLimitError{};

  if (response.status < 200 || response.status >= 300)
    throw std::runtime_error("Skysight HTTP status " +
                             std::to_string(response.status));
}

/**
 * GET a JSON endpoint and return the raw response body.
 */
static Co::Task<std::string>
GetJson(CurlGlobal &curl, const SkysightSession session,
        const std::string url)
{
  CurlEasy easy{url.c_str()};
  CurlSlist headers;
  SetupEasy(easy, headers, session.key.c_str());
  easy.SetRequestHeaders(headers.Get());
  easy.SetTimeout(30);

  auto response = co_await Curl::CoRequest(curl, std::move(easy));
  CheckStatus(response);
  co_return std::move(response.body);
}

Co::Task<SkysightSession>
Login(CurlGlobal &curl, const std::string email,
      const std::string password)
{
  const std::string body =
    boost::json::serialize(boost::json::object{
      {"username", email},
      {"password", password},
    });

  CurlEasy easy{SKYSIGHT_BASE_URL "/auth"};
  CurlSlist headers;
  SetupEasy(easy, headers, SKYSIGHT_API_KEY);
  headers.Append("Content-Type: application/json");
  easy.SetRequestHeaders(headers.Get());
  easy.SetTimeout(30);
  easy.SetRequestBody(body.data(), body.size());

  auto response = co_await Curl::CoRequest(curl, std::move(easy));

  if (response.status == 401 || response.status == 403)
    throw LoginError{};

  CheckStatus(response);
  co_return ParseLoginJson(response.body);
}

Co::Task<std::string>
FetchRegionsJson(CurlGlobal &curl, const SkysightSession session)
{
  co_return co_await GetJson(curl, session,
                             SKYSIGHT_BASE_URL "/regions");
}

Co::Task<std::string>
FetchLayersJson(CurlGlobal &curl, const SkysightSession session,
                const std::string region_id)
{
  char url[256];
  snprintf(url, sizeof(url), SKYSIGHT_BASE_URL "/layers?region_id=%s",
           region_id.c_str());
  co_return co_await GetJson(curl, session, url);
}

Co::Task<std::string>
FetchLastUpdatesJson(CurlGlobal &curl, const SkysightSession session,
                     const std::string region_id)
{
  char url[256];
  snprintf(url, sizeof(url),
           SKYSIGHT_BASE_URL "/data/last_updated?region_id=%s",
           region_id.c_str());
  co_return co_await GetJson(curl, session, url);
}

Co::Task<std::string>
FetchDataIndexJson(CurlGlobal &curl, const SkysightSession session,
                   const std::string region_id,
                   const std::string layer_id,
                   const std::time_t from_time)
{
  char url[256];
  snprintf(url, sizeof(url),
           SKYSIGHT_BASE_URL "/data?region_id=%s&layer_ids=%s&from_time=%llu",
           region_id.c_str(), layer_id.c_str(),
           (unsigned long long)from_time);
  co_return co_await GetJson(curl, session, url);
}

Co::Task<void>
DownloadDataFile(CurlGlobal &curl, const SkysightSession session,
                 const std::string url, const AllocatedPath path)
{
  CurlEasy easy{url.c_str()};
  CurlSlist headers;
  SetupEasy(easy, headers, session.key.c_str());
  easy.SetRequestHeaders(headers.Get());
  easy.SetAcceptEncoding("gzip");

  /* no hard timeout on the (potentially large) data files; instead
     abort transfers that stall for a minute */
  easy.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
  easy.SetOption(CURLOPT_LOW_SPEED_TIME, 60L);

  FileOutputStream file{path};
  const auto response =
    co_await Curl::CoStreamRequest(curl, std::move(easy), file);

  /* on error status the stream received the error page; not calling
     Commit() discards it */
  CheckStatus(response);
  file.Commit();
}

/**
 * Convert a JSON value that may be either a number or a numeric
 * string (the API is not consistent about this) to a UNIX timestamp.
 */
static std::time_t
ToTimeT(const boost::json::value &jv)
{
  if (jv.is_string())
    return (std::time_t)strtoull(jv.get_string().c_str(), nullptr, 10);

  return (std::time_t)jv.to_number<int64_t>();
}

/**
 * Convert a legend value (usually a numeric string) to a finite
 * float.  Returns false on overflow/NaN instead of throwing (the old
 * std::stof() based parser crashed on out-of-range values).
 */
static bool
ToLegendValue(const boost::json::value &jv, float &value_r)
{
  double x;
  if (jv.is_string())
    x = strtod(jv.get_string().c_str(), nullptr);
  else
    x = jv.to_number<double>();

  if (!std::isfinite(x))
    return false;

  value_r = (float)x;
  return true;
}

static uint8_t
ToColorByte(const boost::json::value &jv)
{
  int i = jv.is_string()
    ? atoi(jv.get_string().c_str())
    : jv.to_number<int>();

  return (uint8_t)std::clamp(i, 0, 255);
}

std::vector<SkysightRegion>
ParseRegionsJson(std::string_view json)
{
  std::vector<SkysightRegion> regions;

  const auto doc = boost::json::parse(json);
  for (const auto &item : doc.as_array()) {
    try {
      const auto &obj = item.as_object();
      regions.push_back({
        std::string{obj.at("id").as_string()},
        std::string{obj.at("name").as_string()},
      });
    } catch (...) {
      /* skip malformed entry */
    }
  }

  return regions;
}

std::vector<SkysightLayer>
ParseLayersJson(std::string_view json)
{
  std::vector<SkysightLayer> layers;

  const auto doc = boost::json::parse(json);
  for (const auto &item : doc.as_array()) {
    try {
      const auto &obj = item.as_object();

      SkysightLayer layer;
      layer.id = obj.at("id").as_string();
      layer.name = obj.at("name").as_string();
      if (const auto *description = obj.if_contains("description");
          description != nullptr && description->is_string())
        layer.description = description->as_string();

      for (const auto &c :
             obj.at("legend").as_object().at("colors").as_array()) {
        const auto &color = c.as_object();

        float value;
        if (!ToLegendValue(color.at("value"), value))
          continue;

        const auto &rgb = color.at("color").as_array();
        layer.legend.emplace(value, SkysightLegendColor{
          ToColorByte(rgb.at(0)),
          ToColorByte(rgb.at(1)),
          ToColorByte(rgb.at(2)),
        });
      }

      /* a layer without a legend cannot be rendered */
      if (!layer.legend.empty())
        layers.push_back(std::move(layer));
    } catch (...) {
      /* skip malformed entry */
    }
  }

  return layers;
}

std::vector<LayerUpdate>
ParseLastUpdatesJson(std::string_view json)
{
  std::vector<LayerUpdate> updates;

  const auto doc = boost::json::parse(json);
  for (const auto &item : doc.as_array()) {
    try {
      const auto &obj = item.as_object();
      updates.push_back({
        std::string{obj.at("layer_id").as_string()},
        ToTimeT(obj.at("time")),
      });
    } catch (...) {
      /* skip malformed entry */
    }
  }

  return updates;
}

std::vector<DataFile>
ParseDataIndexJson(std::string_view json)
{
  std::vector<DataFile> files;

  const auto doc = boost::json::parse(json);
  for (const auto &item : doc.as_array()) {
    try {
      const auto &obj = item.as_object();
      files.push_back({
        ToTimeT(obj.at("time")),
        std::string{obj.at("link").as_string()},
      });
    } catch (...) {
      /* skip malformed entry */
    }
  }

  return files;
}

SkysightSession
ParseLoginJson(std::string_view json)
{
  const auto doc = boost::json::parse(json);
  const auto &obj = doc.as_object();

  SkysightSession session;
  session.key = obj.at("key").as_string();
  session.expiry = ToTimeT(obj.at("valid_until"));

  if (session.key.empty())
    throw std::runtime_error("Skysight login response without key");

  return session;
}

} // namespace SkysightAPI
