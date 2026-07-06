// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "DecoderThread.hpp"
#include "Layer.hpp"
#include "Session.hpp"
#include "Settings.hpp"
#include "system/Path.hpp"
#include "time/BrokenDateTime.hpp"
#include "time/PeriodClock.hpp"
#include "ui/event/CoInjectFunction.hpp"
#include "ui/event/Notify.hpp"
#include "ui/event/PeriodicTimer.hpp"

#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class CurlGlobal;

/**
 * What one background synchronisation pass brought home; produced
 * on the I/O thread, consumed on the UI thread (values only).
 */
struct SkysightSyncResult {
  SkysightSession session;

  /** raw catalog documents; empty = not fetched this pass */
  std::string regions_json, layers_json;

  std::vector<std::pair<std::string, std::time_t>> last_updates;
  bool fetched_last_updates = false;
};

/**
 * Gets notified on the UI thread whenever Skysight state, selection
 * or download/decode progress changed.
 */
class SkysightListener {
public:
  virtual void OnSkysightUpdate() noexcept = 0;
};

/**
 * Glue class for the Skysight weather service: owns the session,
 * the layer catalog, the selected layers, the download pipeline and
 * the map overlay.
 *
 * Threading contract: every public method as well as all mutable
 * state runs on the UI thread only.  Background work happens in
 * coroutines on the #CurlGlobal event loop and in the decoder
 * thread; both only receive value snapshots and hand results back
 * through UI-thread callbacks.  There are no singletons; the
 * instance is owned via std::shared_ptr by #MapWindow (see
 * DataGlobals::GetSkysight()).
 */
class Skysight final {
public:
  enum class State : uint8_t {
    /** no credentials configured */
    NOT_CONFIGURED,

    /** configured; waiting for the next synchronisation */
    IDLE,

    LOGGING_IN,
    LOADING_CATALOG,
    READY,

    /** a failure occurred; waiting for the retry backoff to expire */
    BACKOFF_WAIT,

    /**
     * Hard stop (rate limit, repeated login failures or implausible
     * device clock); only UpdateSettings() leaves this state.
     */
    STOPPED,
  };

  /** download state of one user-selected layer */
  struct SelectedLayer {
    std::string id;

    /** forecast range covered by cached .tif files (UNIX time) */
    std::time_t from = 0, to = 0;

    /** most recent modification among the cached files */
    std::time_t mtime = 0;
  };

  static constexpr std::size_t MAX_SELECTED_LAYERS = 5;

private:
  CurlGlobal &curl;
  const AllocatedPath cache_path;

  std::string email, password;

  /** the effective region id (validated against #regions) */
  std::string region;

  SkysightSession session;

  State state = State::NOT_CONFIGURED;
  std::string status_text;

  /* catalog */
  std::vector<SkysightRegion> regions;
  std::vector<SkysightLayer> layers;
  bool selection_loaded = false;
  std::time_t last_updates_time = 0;

  /* selection */
  std::vector<SelectedLayer> selected_layers;
  std::string displayed_layer;

  /** the forecast slot currently shown as overlay (0 = none) */
  std::time_t displayed_forecast_time = 0;

  /** layer ids with a pending explicit update request */
  std::vector<std::string> update_queue;

  /** layer ids being downloaded by the current sync pass */
  std::vector<std::string> sync_layer_ids;

  /* failure handling */
  unsigned consecutive_failures = 0, login_failures = 0;
  PeriodClock backoff_clock;
  std::chrono::steady_clock::duration backoff_duration{};

  std::time_t last_cleanup_time = 0;

  /** rate limit for downloads triggered by a missing overlay file */
  std::time_t last_auto_fetch_time = 0;

  SkysightListener *listener = nullptr;

  bool sync_running = false;

  UI::CoInjectFunction<SkysightSyncResult> sync_inject;

  /* decode_notify must outlive decoder: the decoder references it
     from its worker thread until its destructor has joined */
  UI::Notify decode_notify{[this]{ OnDecodeResults(); }};
  SkysightDecoderThread decoder;

  UI::PeriodicTimer tick_timer{[this]{ Tick(); }};

public:
  explicit Skysight(CurlGlobal &curl);
  ~Skysight();

  Skysight(const Skysight &) = delete;
  Skysight &operator=(const Skysight &) = delete;

  /**
   * (Re-)read the profile settings, reset the failure state and
   * schedule a synchronisation.  Called at startup and whenever the
   * user saved the weather configuration.
   */
  void UpdateSettings();

  State GetState() const noexcept {
    return state;
  }

  /** human-readable state/error description for the UI */
  const std::string &GetStatusText() const noexcept {
    return status_text;
  }

  bool IsConfigured() const noexcept {
    return state != State::NOT_CONFIGURED;
  }

  /** is the layer catalog available? */
  bool IsReady() const noexcept {
    return !layers.empty();
  }

  const std::vector<SkysightRegion> &GetRegions() const noexcept {
    return regions;
  }

  const std::vector<SkysightLayer> &GetLayers() const noexcept {
    return layers;
  }

  [[gnu::pure]]
  const SkysightLayer *GetLayer(std::string_view id) const noexcept;

  /* selection management (backed by the SkysightActiveMetrics
     profile key) */

  std::size_t NumSelectedLayers() const noexcept {
    return selected_layers.size();
  }

  bool SelectedLayersFull() const noexcept {
    return selected_layers.size() >= MAX_SELECTED_LAYERS;
  }

  /** returns nullptr when out of range */
  [[gnu::pure]]
  const SelectedLayer *GetSelectedLayer(std::size_t index) const noexcept;

  [[gnu::pure]]
  bool IsSelectedLayer(std::string_view id) const noexcept;

  bool AddSelectedLayer(const std::string &id);
  void RemoveSelectedLayer(const std::string &id);

  /**
   * Queue a download of the next 24 h of forecasts for one selected
   * layer (or for all of them when id is empty).
   */
  void UpdateSelectedLayer(const std::string &id);

  /** is any download or decode in progress? */
  bool IsUpdating() noexcept;

  /** is a download or decode for this layer in progress? */
  bool IsLayerUpdating(std::string_view id) noexcept;

  /* overlay */

  const std::string &GetDisplayedLayerId() const noexcept {
    return displayed_layer;
  }

  /**
   * Show the given selected layer as map overlay (nullptr to
   * disable).  Persisted in the SkysightDisplayedMetric profile key.
   */
  bool DisplayLayer(const char *id);

  void SetListener(SkysightListener *_listener) noexcept {
    listener = _listener;
  }

  Path GetCachePath() const noexcept {
    return cache_path;
  }

private:
  void Tick();

  void StartSync();
  void OnSyncSuccess(SkysightSyncResult result);
  void OnSyncError(std::exception_ptr error);
  void OnDecodeResults();

  void SetState(State new_state, std::string text = {});

  void LoadCachedCatalog();
  void LoadSelectionFromProfile();
  void SaveSelectionToProfile();

  /** re-scan the cache folder for one selected layer */
  void RefreshSelectedLayerState(SelectedLayer &layer);

  /**
   * Update the map overlay to the file closest to the current
   * forecast slot.  Returns false when no suitable file exists yet
   * (a download is queued in that case).
   */
  bool UpdateOverlay();

  void CleanupFiles();

  [[gnu::pure]]
  BrokenDateTime GetNow() const noexcept;

  void NotifyListener() noexcept {
    if (listener != nullptr)
      listener->OnSkysightUpdate();
  }
};
