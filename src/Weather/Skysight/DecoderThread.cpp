// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DecoderThread.hpp"
#include "CDFDecoder.hpp"
#include "LogFile.hpp"

#include <utility>

void
SkysightDecoderThread::Push(Job job)
{
  const std::lock_guard lock{mutex};
  queue.push_back(std::move(job));

  /* if the thread is busy, the drain loop in Tick() will pick the
     job up; Run() clears "busy" in the same critical section as
     Tick()'s final empty-queue check, so no job can fall through */
  if (!IsBusy())
    Trigger();
}

std::deque<SkysightDecoderThread::Result>
SkysightDecoderThread::TakeResults()
{
  const std::lock_guard lock{mutex};
  return std::exchange(results, {});
}

void
SkysightDecoderThread::Tick() noexcept
{
  while (!queue.empty() && !IsStopped()) {
    Job job = std::move(queue.front());
    queue.pop_front();

    mutex.unlock();

    Result result{std::move(job.tif_path), std::move(job.layer_id),
                  job.forecast_time, false};

    try {
      DecodeNetCDFToGeoTIFF(job.nc_path, result.tif_path,
                            result.layer_id, job.legend);
      result.success = true;
    } catch (const std::exception &e) {
      LogFormat("Skysight decode of %s failed: %s",
                job.nc_path.c_str(), e.what());
    }

    mutex.lock();
    results.push_back(std::move(result));

    /* cheap thread-safe notification; the owner reacts by calling
       TakeResults() from its own thread */
    if (on_results_changed)
      on_results_changed();
  }
}
