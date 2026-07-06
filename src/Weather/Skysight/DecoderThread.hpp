// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Layer.hpp"
#include "system/Path.hpp"
#include "thread/StandbyThread.hpp"

#include <ctime>
#include <deque>
#include <functional>
#include <map>
#include <string>

/**
 * A single reusable worker thread that converts downloaded NetCDF
 * files to GeoTIFF overlays, one job at a time (replaces the old
 * one-thread-per-decode scheme).
 *
 * After each finished job the "results changed" callback is invoked
 * on the worker thread; it must be cheap and thread-safe (e.g.
 * UI::Notify::SendNotification()), and the owner then collects
 * finished jobs with TakeResults() on its own thread.
 *
 * The destructor joins the thread, so this object must be destroyed
 * before anything the callback refers to.
 */
class SkysightDecoderThread final : private StandbyThread {
public:
  struct Job {
    AllocatedPath nc_path;
    AllocatedPath tif_path;
    std::string layer_id;
    std::time_t forecast_time;
    std::map<float, SkysightLegendColor> legend;
  };

  struct Result {
    AllocatedPath tif_path;
    std::string layer_id;
    std::time_t forecast_time;
    bool success;
  };

private:
  /* both guarded by StandbyThread::mutex */
  std::deque<Job> queue;
  std::deque<Result> results;

  const std::function<void()> on_results_changed;

public:
  explicit SkysightDecoderThread(std::function<void()> _on_results_changed)
    :StandbyThread("skysightdec"),
     on_results_changed(std::move(_on_results_changed)) {}

  ~SkysightDecoderThread() {
    LockStop();
  }

  void Push(Job job);

  std::deque<Result> TakeResults();

private:
  void Tick() noexcept override;
};
