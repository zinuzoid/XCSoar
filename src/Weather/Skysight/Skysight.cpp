// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Skysight.hpp"
#include "Protocol.hpp"
#include "SkysightRegions.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "MapWindow/OverlayBitmap.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "UIGlobals.hpp"
#include "io/FileOutputStream.hxx"
#include "io/FileReader.hxx"
#include "lib/curl/Global.hxx"
#include "system/FileUtil.hpp"
#include "util/StaticString.hxx"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <span>

using namespace std::chrono;

/** how often Tick() runs */
static constexpr auto TICK_INTERVAL = seconds(30);

/** refresh the regions/layers catalog when older than this */
static constexpr std::time_t CATALOG_MAX_AGE = 24 * 60 * 60;

/** refresh the per-layer last-update times when older than this */
static constexpr std::time_t LAST_UPDATES_MAX_AGE = 5 * 60 * 60;

/** forecast window for an explicit "update" request */
static constexpr std::time_t UPDATE_WINDOW = 24 * 60 * 60;

/** forecast window when refreshing the displayed layer */
static constexpr std::time_t DISPLAY_WINDOW = 2 * 60 * 60;

/** run CleanupFiles() at most this often */
static constexpr std::time_t CLEANUP_INTERVAL = 60 * 60;

static constexpr unsigned MAX_LOGIN_FAILURES = 10;

static constexpr auto BACKOFF_MIN = minutes(1);
static constexpr auto BACKOFF_MAX = minutes(30);

static BrokenDateTime
FromUnixTime(std::time_t t)
{
#ifdef HAVE_POSIX
  return BrokenDateTime::FromUnixTimeUTC(t);
#else
  /* only for Skysight-provided timestamps (consistent epoch) */
  return BrokenDateTime(1970, 1, 1, 0, 0, 0) + (int)t;
#endif
}

static std::time_t
ToUnixTime(BrokenDateTime t)
{
  return system_clock::to_time_t(t.ToTimePoint());
}

/**
 * Round to the nearest 30-minute forecast slot.
 */
static BrokenDateTime
ForecastSlot(BrokenDateTime t)
{
  if (t.minute >= 15 && t.minute < 45)
    t.minute = 30;
  else if (t.minute >= 45) {
    t.minute = 0;
    t = t + seconds(60 * 60);
  } else
    t.minute = 0;

  t.second = 0;
  return t;
}

/**
 * Build the base name (no extension) of a data file:
 * "<region>-<layer>-YYYYMMDDHHMM"
 */
static AllocatedPath
DataFilePath(Path cache_path, const std::string &region,
             const std::string &layer_id, std::time_t forecast_time,
             const char *extension)
{
  const BrokenDateTime t = FromUnixTime(forecast_time);
  NarrowString<256> name;
  name.Format("%s-%s-%04u%02u%02u%02u%02u%s",
              region.c_str(), layer_id.c_str(),
              t.year, t.month, t.day, t.hour, t.minute,
              extension);
  return AllocatedPath::Build(cache_path, name.c_str());
}

/**
 * Parse a cached image filename of the form
 * "<region>-<layer>-YYYYMMDDHHMM.tif".
 */
struct SkysightImageFile {
  std::string region, layer_id;
  std::time_t forecast_time = 0;
  std::time_t mtime = 0;
  bool valid = false;

  explicit SkysightImageFile(Path path);
};

SkysightImageFile::SkysightImageFile(Path path)
{
  const Path filename = path.GetBase();
  if (filename == nullptr)
    return;

  const std::string base = filename.c_str();

  const auto first = base.find('-');
  if (first == std::string::npos)
    return;

  const auto second = base.find('-', first + 1);
  if (second == std::string::npos)
    return;

  const std::string dt = base.substr(second + 1);
  if (dt.size() != 12 ||
      !std::all_of(dt.begin(), dt.end(),
                   [](char ch){ return ch >= '0' && ch <= '9'; }))
    return;

  const BrokenDateTime t((unsigned)std::stoul(dt.substr(0, 4)),
                         (unsigned)std::stoul(dt.substr(4, 2)),
                         (unsigned)std::stoul(dt.substr(6, 2)),
                         (unsigned)std::stoul(dt.substr(8, 2)),
                         (unsigned)std::stoul(dt.substr(10, 2)));
  if (!t.IsPlausible())
    return;

  region = base.substr(0, first);
  layer_id = base.substr(first + 1, second - first - 1);
  forecast_time = ToUnixTime(t);
  mtime = system_clock::to_time_t(File::GetLastModification(path));
  valid = true;
}

static std::string
ReadFileToString(Path path)
{
  FileReader reader{path};
  std::string result;
  result.resize(reader.GetSize());

  std::size_t position = 0;
  while (position < result.size()) {
    const auto n =
      reader.Read(std::as_writable_bytes(std::span{result}.subspan(position)));
    if (n == 0)
      throw std::runtime_error("unexpected end of file");
    position += n;
  }

  return result;
}

static void
WriteStringToFile(Path path, std::string_view contents)
{
  FileOutputStream file{path};
  file.Write(std::as_bytes(std::span{contents}));
  file.Commit();
}

/* ------------ the background synchronisation coroutine ------------ */

/**
 * Value snapshot describing what one synchronisation pass should do;
 * built on the UI thread, consumed on the I/O thread.
 */
struct SkysightSyncRequest {
  SkysightSession session;
  std::string email, password;
  std::string region;
  AllocatedPath cache_path;

  bool fetch_regions = false, fetch_layers = false,
    fetch_last_updates = false;

  struct Download {
    std::string layer_id;

    /** server-side freshness; older cached files are re-downloaded */
    std::time_t last_update;

    /** forecast window to fetch */
    std::time_t from, to;

    std::map<float, SkysightLegendColor> legend;
  };

  std::vector<Download> downloads;
};

static Co::Task<SkysightSyncResult>
RunSync(CurlGlobal &curl, SkysightDecoderThread &decoder,
        /* not const: moved into the coroutine frame, and the
           AllocatedPath member makes this move-only */
        SkysightSyncRequest request)
{
  SkysightSyncResult result;

  if (request.session.IsValid(time(nullptr)))
    result.session = request.session;
  else
    result.session = co_await SkysightAPI::Login(curl, request.email,
                                                 request.password);

  if (request.fetch_regions) {
    result.regions_json =
      co_await SkysightAPI::FetchRegionsJson(curl, result.session);
    WriteStringToFile(AllocatedPath::Build(request.cache_path,
                                           "regions.json"),
                      result.regions_json);
  }

  if (request.fetch_layers) {
    result.layers_json =
      co_await SkysightAPI::FetchLayersJson(curl, result.session,
                                            request.region);
    NarrowString<128> name;
    name.Format("layers-%s.json", request.region.c_str());
    WriteStringToFile(AllocatedPath::Build(request.cache_path,
                                           name.c_str()),
                      result.layers_json);
  }

  if (request.fetch_last_updates) {
    const auto json =
      co_await SkysightAPI::FetchLastUpdatesJson(curl, result.session,
                                                 request.region);
    for (auto &i : SkysightAPI::ParseLastUpdatesJson(json))
      result.last_updates.emplace_back(std::move(i.layer_id), i.time);
    result.fetched_last_updates = true;
  }

  for (const auto &download : request.downloads) {
    const auto index_json =
      co_await SkysightAPI::FetchDataIndexJson(curl, result.session,
                                               request.region,
                                               download.layer_id,
                                               download.from);

    for (const auto &file : SkysightAPI::ParseDataIndexJson(index_json)) {
      if (file.time > download.to)
        continue;

      auto tif_path = DataFilePath(request.cache_path, request.region,
                                   download.layer_id, file.time, ".tif");

      /* skip files that are still up to date */
      if (File::Exists(tif_path) &&
          download.last_update <=
            system_clock::to_time_t(File::GetLastModification(tif_path)))
        continue;

      auto nc_path = DataFilePath(request.cache_path, request.region,
                                  download.layer_id, file.time, ".nc");

      /* explicit copy: nc_path is still needed for the decode job */
      co_await SkysightAPI::DownloadDataFile(curl, result.session,
                                             file.link,
                                             AllocatedPath{Path{nc_path}});

      decoder.Push({std::move(nc_path), std::move(tif_path),
                    download.layer_id, file.time, download.legend});
    }
  }

  co_return result;
}

/* ------------------------------ glue ------------------------------ */

Skysight::Skysight(CurlGlobal &_curl)
  :curl(_curl),
   cache_path(MakeLocalPath("skysight")),
   sync_inject(_curl.GetEventLoop()),
   decoder([this]{ decode_notify.SendNotification(); })
{
  UpdateSettings();
}

Skysight::~Skysight()
{
  tick_timer.Cancel();

  /* cancel the pipeline coroutine before the decoder (declared after
     sync_inject) joins its worker; nothing else references this
     object from other threads */
  sync_inject.Cancel();
}

void
Skysight::UpdateSettings()
{
  const auto &settings =
    CommonInterface::GetComputerSettings().weather.skysight;

  email = settings.email;
  password = settings.password;
  region = settings.region;

  /* an explicit (re-)configuration resets all failure state,
     including a hard stop */
  consecutive_failures = login_failures = 0;
  backoff_duration = {};

  if (regions.empty())
    for (auto r = skysight_region_defaults; r->id != nullptr; ++r)
      regions.push_back({r->id, r->name});

  if (email.empty() || password.empty()) {
    SetState(State::NOT_CONFIGURED, _("Skysight is not configured"));
    tick_timer.Cancel();
    return;
  }

  const auto region_known = [this](const std::string &id){
    return std::any_of(regions.begin(), regions.end(),
                       [&id](const auto &r){ return r.id == id; });
  };

  if (region.empty() || !region_known(region))
    region = "EUROPE";

  LoadCachedCatalog();

  if (!selection_loaded && !layers.empty())
    LoadSelectionFromProfile();

  SetState(State::IDLE, _("Waiting for synchronisation"));

  if (!tick_timer.IsActive())
    tick_timer.Schedule(TICK_INTERVAL);

  /* synchronise soon, but not synchronously from here: this may be
     called from dialog code */
  Tick();
}

void
Skysight::SetState(State new_state, std::string text)
{
  state = new_state;
  status_text = std::move(text);
  NotifyListener();
}

void
Skysight::LoadCachedCatalog()
{
  try {
    const auto path = AllocatedPath::Build(cache_path, "regions.json");
    if (File::Exists(path)) {
      auto cached = SkysightAPI::ParseRegionsJson(ReadFileToString(path));
      if (!cached.empty())
        regions = std::move(cached);
    }
  } catch (...) {
    LogError(std::current_exception(), "Skysight regions cache");
  }

  try {
    NarrowString<128> name;
    name.Format("layers-%s.json", region.c_str());
    const auto path = AllocatedPath::Build(cache_path, name.c_str());
    if (File::Exists(path)) {
      auto cached = SkysightAPI::ParseLayersJson(ReadFileToString(path));
      if (!cached.empty())
        layers = std::move(cached);
    }
  } catch (...) {
    LogError(std::current_exception(), "Skysight layers cache");
  }
}

const SkysightLayer *
Skysight::GetLayer(std::string_view id) const noexcept
{
  for (const auto &layer : layers)
    if (layer.id == id)
      return &layer;

  return nullptr;
}

const Skysight::SelectedLayer *
Skysight::GetSelectedLayer(std::size_t index) const noexcept
{
  return index < selected_layers.size()
    ? &selected_layers[index]
    : nullptr;
}

bool
Skysight::IsSelectedLayer(std::string_view id) const noexcept
{
  return std::any_of(selected_layers.begin(), selected_layers.end(),
                     [id](const auto &i){ return i.id == id; });
}

bool
Skysight::AddSelectedLayer(const std::string &id)
{
  if (GetLayer(id) == nullptr || IsSelectedLayer(id) ||
      SelectedLayersFull())
    return false;

  auto &layer = selected_layers.emplace_back();
  layer.id = id;
  RefreshSelectedLayerState(layer);
  SaveSelectionToProfile();

  /* start fetching data right away */
  UpdateSelectedLayer(id);
  return true;
}

void
Skysight::RemoveSelectedLayer(const std::string &id)
{
  if (displayed_layer == id)
    DisplayLayer(nullptr);

  std::erase_if(selected_layers,
                [&id](const auto &i){ return i.id == id; });
  std::erase(update_queue, id);
  SaveSelectionToProfile();
  NotifyListener();
}

void
Skysight::UpdateSelectedLayer(const std::string &id)
{
  for (auto &layer : selected_layers) {
    if (!id.empty() && layer.id != id)
      continue;

    if (std::find(update_queue.begin(), update_queue.end(),
                  layer.id) == update_queue.end())
      update_queue.push_back(layer.id);
  }

  NotifyListener();
  Tick();
}

bool
Skysight::IsUpdating() noexcept
{
  return sync_running || !update_queue.empty() || decoder.HasJobs();
}

bool
Skysight::IsLayerUpdating(std::string_view id) noexcept
{
  const auto contains = [id](const auto &ids){
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  };

  return contains(update_queue) ||
    (sync_running && contains(sync_layer_ids)) ||
    decoder.HasJobs(id);
}

void
Skysight::SaveSelectionToProfile()
{
  std::string list;
  for (const auto &layer : selected_layers) {
    if (!list.empty())
      list += ',';
    list += layer.id;
  }

  Profile::Set(ProfileKeys::SkysightActiveMetrics, list.c_str());
}

void
Skysight::LoadSelectionFromProfile()
{
  selection_loaded = true;

  const char *s = Profile::Get(ProfileKeys::SkysightActiveMetrics);
  if (s != nullptr) {
    std::string_view remaining{s};
    while (!remaining.empty()) {
      const auto comma = remaining.find(',');
      const auto id = remaining.substr(0, comma);
      remaining = comma == std::string_view::npos
        ? std::string_view{}
        : remaining.substr(comma + 1);

      const std::string id_str{id};
      if (!id_str.empty() && GetLayer(id_str) != nullptr &&
          !IsSelectedLayer(id_str) && !SelectedLayersFull()) {
        auto &layer = selected_layers.emplace_back();
        layer.id = id_str;
        RefreshSelectedLayerState(layer);
      }
    }
  }

  const char *d = Profile::Get(ProfileKeys::SkysightDisplayedMetric);
  if (d != nullptr && *d != 0 && IsSelectedLayer(d))
    DisplayLayer(d);
}

void
Skysight::RefreshSelectedLayerState(SelectedLayer &layer)
{
  layer.from = layer.to = layer.mtime = 0;

  struct Visitor : public File::Visitor {
    SelectedLayer &layer;
    const std::string &region;

    Visitor(SelectedLayer &_layer, const std::string &_region)
      :layer(_layer), region(_region) {}

    void Visit(Path path, Path) override {
      const SkysightImageFile file{path};
      if (!file.valid || file.region != region ||
          file.layer_id != layer.id)
        return;

      layer.from = layer.from == 0
        ? file.forecast_time
        : std::min(layer.from, file.forecast_time);
      layer.to = std::max(layer.to, file.forecast_time);
      layer.mtime = std::max(layer.mtime, file.mtime);
    }
  } visitor{layer, region};

  NarrowString<256> pattern;
  pattern.Format("%s-%s-*.tif", region.c_str(), layer.id.c_str());
  Directory::VisitSpecificFiles(cache_path, pattern.c_str(), visitor);
}

bool
Skysight::DisplayLayer(const char *id)
{
  if (id == nullptr || *id == 0) {
    displayed_layer.clear();
    displayed_forecast_time = 0;
#ifdef ENABLE_OPENGL
    if (auto *map = UIGlobals::GetMap(); map != nullptr)
      map->SetOverlay(nullptr);
#endif
    Profile::Set(ProfileKeys::SkysightDisplayedMetric, "");
    NotifyListener();
    return true;
  }

  if (!IsSelectedLayer(id))
    return false;

  displayed_layer = id;
  displayed_forecast_time = 0;
  Profile::Set(ProfileKeys::SkysightDisplayedMetric, id);
  NotifyListener();
  return UpdateOverlay();
}

bool
Skysight::UpdateOverlay()
{
  if (displayed_layer.empty())
    return false;

  const std::time_t target = ToUnixTime(ForecastSlot(GetNow()));

  /* pick the cached file closest to the current forecast slot */
  struct Visitor : public File::Visitor {
    const std::string &region, &layer_id;
    const std::time_t target;

    std::time_t best_time = 0;
    AllocatedPath best_path = nullptr;

    Visitor(const std::string &_region, const std::string &_layer_id,
            std::time_t _target)
      :region(_region), layer_id(_layer_id), target(_target) {}

    void Visit(Path path, Path) override {
      const SkysightImageFile file{path};
      if (!file.valid || file.region != region ||
          file.layer_id != layer_id)
        return;

      if (best_path == nullptr ||
          std::abs((long)(file.forecast_time - target)) <
          std::abs((long)(best_time - target))) {
        best_time = file.forecast_time;
        best_path = AllocatedPath{path};
      }
    }
  } visitor{region, displayed_layer, target};

  NarrowString<256> pattern;
  pattern.Format("%s-%s-*.tif", region.c_str(), displayed_layer.c_str());
  Directory::VisitSpecificFiles(cache_path, pattern.c_str(), visitor);

  if (visitor.best_path == nullptr ||
      std::abs((long)(visitor.best_time - target)) >
      (long)DISPLAY_WINDOW) {
    /* nothing usable cached yet: fetch data, but rate-limited so a
       server without data for this layer is not hammered in a loop */
    const std::time_t now = ToUnixTime(GetNow());
    if (!IsUpdating() && last_auto_fetch_time + 10 * 60 < now) {
      last_auto_fetch_time = now;
      UpdateSelectedLayer(displayed_layer);
    }
    return false;
  }

  if (visitor.best_time == displayed_forecast_time)
    /* the right overlay is already being shown */
    return true;

  auto *map = UIGlobals::GetMap();
  if (map == nullptr)
    return false;

  std::unique_ptr<MapOverlayBitmap> bmp;
  try {
    bmp = std::make_unique<MapOverlayBitmap>(Path{visitor.best_path});
  } catch (...) {
    LogError(std::current_exception(), "Skysight overlay load");
    return false;
  }

  const auto *layer = GetLayer(displayed_layer);
  const BrokenDateTime t = FromUnixTime(visitor.best_time);
  StaticString<256> label;
  label.Format("Skysight: %s (%04u-%02u-%02u %02u:%02u)",
               layer != nullptr ? layer->name.c_str()
                                : displayed_layer.c_str(),
               t.year, t.month, t.day, t.hour, t.minute);

  bmp->SetAlpha(0.6);
  bmp->SetLabel(label);
#ifdef ENABLE_OPENGL
  map->SetOverlay(std::move(bmp));
#endif
  displayed_forecast_time = visitor.best_time;
  return true;
}

void
Skysight::Tick()
{
  if (state == State::NOT_CONFIGURED || state == State::STOPPED)
    return;

  const std::time_t now = ToUnixTime(GetNow());

  /* guard against devices with a dead RTC: refusing to talk to the
     server beats hammering it with requests carrying bogus times */
  if (const auto stale = [now](const auto &layer){
        return layer.last_update != 0 &&
          layer.last_update + 365 * 24 * 60 * 60 < now;
      };
      std::any_of(layers.begin(), layers.end(), stale)) {
    SetState(State::STOPPED,
             _("Stopped: device clock implausible (dead RTC battery?)"));
    return;
  }

  if (last_cleanup_time + CLEANUP_INTERVAL < now) {
    last_cleanup_time = now;
    CleanupFiles();
  }

  /* keep the overlay tracking the current forecast slot */
  if (!displayed_layer.empty() &&
      displayed_forecast_time != 0 &&
      ToUnixTime(ForecastSlot(GetNow())) != displayed_forecast_time)
    UpdateOverlay();

  if (sync_running)
    return;

  if (state == State::BACKOFF_WAIT) {
    if (!backoff_clock.Check(backoff_duration))
      return;
    SetState(State::IDLE);
  }

  StartSync();
}

void
Skysight::StartSync()
{
  SkysightSyncRequest request;
  request.session = session;
  request.email = email;
  request.password = password;
  request.region = region;
  request.cache_path = AllocatedPath{Path{cache_path}};

  const std::time_t now = ToUnixTime(GetNow());

  const auto cache_age = [now](Path path) -> std::time_t {
    if (!File::Exists(path))
      return std::numeric_limits<std::time_t>::max();
    return now - system_clock::to_time_t(File::GetLastModification(path));
  };

  request.fetch_regions =
    cache_age(AllocatedPath::Build(cache_path, "regions.json")) >
    CATALOG_MAX_AGE;

  NarrowString<128> layers_name;
  layers_name.Format("layers-%s.json", region.c_str());
  request.fetch_layers = layers.empty() ||
    cache_age(AllocatedPath::Build(cache_path, layers_name.c_str())) >
    CATALOG_MAX_AGE;

  request.fetch_last_updates = !layers.empty() &&
    (last_updates_time == 0 ||
     last_updates_time + LAST_UPDATES_MAX_AGE < now);

  for (const auto &id : update_queue) {
    const auto *layer = GetLayer(id);
    if (layer == nullptr)
      continue;

    request.downloads.push_back({
      id,
      layer->last_update,
      ToUnixTime(ForecastSlot(GetNow())),
      now + UPDATE_WINDOW,
      layer->legend,
    });
  }

  if (!request.fetch_regions && !request.fetch_layers &&
      !request.fetch_last_updates && request.downloads.empty()) {
    /* nothing to do */
    if (state == State::IDLE)
      SetState(State::READY, _("Up to date"));
    return;
  }

  update_queue.clear();
  sync_layer_ids.clear();
  for (const auto &download : request.downloads)
    sync_layer_ids.push_back(download.layer_id);

  if (!session.IsValid(now))
    SetState(State::LOGGING_IN, _("Logging in to Skysight"));
  else if (request.fetch_layers || request.fetch_regions)
    SetState(State::LOADING_CATALOG, _("Loading Skysight layer list"));
  else if (!request.downloads.empty())
    SetState(State::READY, _("Downloading weather data"));

  sync_running = true;
  sync_inject.Start(RunSync(curl, decoder, std::move(request)),
                    [this](SkysightSyncResult result){
                      OnSyncSuccess(std::move(result));
                    },
                    [this](std::exception_ptr error){
                      OnSyncError(std::move(error));
                    });
}

void
Skysight::OnSyncSuccess(SkysightSyncResult result)
{
  sync_running = false;
  consecutive_failures = login_failures = 0;
  session = result.session;

  if (!result.regions_json.empty()) {
    try {
      auto fetched = SkysightAPI::ParseRegionsJson(result.regions_json);
      if (!fetched.empty())
        regions = std::move(fetched);
    } catch (...) {
      LogError(std::current_exception(), "Skysight regions parse");
    }
  }

  if (!result.layers_json.empty()) {
    try {
      auto fetched = SkysightAPI::ParseLayersJson(result.layers_json);
      if (!fetched.empty()) {
        /* preserve known last-update times */
        for (auto &layer : fetched)
          if (const auto *old = GetLayer(layer.id); old != nullptr)
            layer.last_update = old->last_update;
        layers = std::move(fetched);

        /* the catalog may have arrived for the first time */
        if (!selection_loaded)
          LoadSelectionFromProfile();
      }
    } catch (...) {
      LogError(std::current_exception(), "Skysight layers parse");
    }
  }

  if (result.fetched_last_updates) {
    last_updates_time = ToUnixTime(GetNow());
    for (const auto &[id, update_time] : result.last_updates)
      for (auto &layer : layers)
        if (layer.id == id)
          layer.last_update = update_time;
  }

  sync_layer_ids.clear();

  for (auto &layer : selected_layers)
    RefreshSelectedLayerState(layer);

  SetState(State::READY, _("Up to date"));

  if (!displayed_layer.empty() && displayed_forecast_time == 0)
    UpdateOverlay();

  /* more work may already be queued (e.g. a layer selected while
     the pass was running) */
  if (!update_queue.empty())
    StartSync();
}

void
Skysight::OnSyncError(std::exception_ptr error)
{
  sync_running = false;
  update_queue.clear();
  sync_layer_ids.clear();

  try {
    std::rethrow_exception(error);
  } catch (const SkysightAPI::RateLimitError &) {
    /* the server explicitly asked us to stop for this session */
    SetState(State::STOPPED,
             _("Stopped: rate limited by the Skysight server"));
    return;
  } catch (const SkysightAPI::LoginError &) {
    if (++login_failures >= MAX_LOGIN_FAILURES) {
      SetState(State::STOPPED,
               _("Stopped: Skysight login keeps failing; check your credentials"));
      return;
    }

    session.Clear();
    status_text = _("Skysight login failed: check e-mail and password");
  } catch (const std::exception &e) {
    LogFormat("Skysight sync error: %s", e.what());
    status_text = _("Skysight connection failed");
  }

  ++consecutive_failures;
  backoff_duration = std::min<steady_clock::duration>(
    BACKOFF_MIN * (1 << std::min(consecutive_failures - 1, 5u)),
    BACKOFF_MAX);
  backoff_clock.Update();
  SetState(State::BACKOFF_WAIT, std::move(status_text));
}

void
Skysight::OnDecodeResults()
{
  for (auto &result : decoder.TakeResults()) {
    for (auto &layer : selected_layers)
      if (layer.id == result.layer_id)
        RefreshSelectedLayerState(layer);

    if (result.success && result.layer_id == displayed_layer)
      UpdateOverlay();
  }

  NotifyListener();
}

void
Skysight::CleanupFiles()
{
  const std::time_t now = ToUnixTime(GetNow());

  struct Visitor : public File::Visitor {
    const std::time_t now;

    explicit Visitor(std::time_t _now):now(_now) {}

    void Visit(Path path, Path filename) override {
      const char *name = filename.c_str();
      const auto mtime =
        system_clock::to_time_t(File::GetLastModification(path));

      if (filename.EndsWithIgnoreCase(".tif")) {
        const SkysightImageFile file{path};
        if (mtime + 5 * 24 * 60 * 60 <= now ||
            (file.valid && file.forecast_time + 24 * 60 * 60 < now))
          File::Delete(path);
      } else if (filename.EndsWithIgnoreCase(".nc") ||
                 filename.EndsWithIgnoreCase(".tmp") ||
                 filename.EndsWithIgnoreCase(".dltemp") ||
                 strstr(name, "-datafiles-") != nullptr) {
        /* stray temporaries and leftovers of the old implementation */
        if (mtime + 24 * 60 * 60 <= now)
          File::Delete(path);
      }
    }
  } visitor{now};

  Directory::VisitFiles(cache_path, visitor);
}

BrokenDateTime
Skysight::GetNow() const noexcept
{
  /* prefer the blackboard time so replays work; fall back to the
     system clock */
  const auto &basic = CommonInterface::Basic();
  if (basic.date_time_utc.IsPlausible())
    return basic.date_time_utc;

  return BrokenDateTime::NowUTC();
}
