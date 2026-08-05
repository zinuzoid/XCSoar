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

#include "Skysight.hpp"
#include "system/ConvertPathName.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "system/FileUtil.hpp"
#include "util/StringCompare.hxx"
#include "util/Macros.hpp"
#include "util/tstring.hpp"
#include <tchar.h>
#include <string>
#include <vector>
#include "util/StaticString.hxx"
#include "Profile/Profile.hpp"
#include "ActionInterface.hpp"
#include "system/FileUtil.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"
#include "time/BrokenDateTime.hpp"
#include <memory>
#ifdef ENABLE_OPENGL
#include "MapWindow/OverlayBitmap.hpp"
#endif
#include "MapWindow/GlueMapWindow.hpp"
#include "thread/Debug.hpp"

/**
 * TODO:
 * -- overlay only shows following render -- no way to trigger from child thread
 * -- no transparent bg on overlay on android
 *
 * --- for release ----
 * - Use SkysightImageFile elsewhere instead of recalculating forecast time, move to separate imp file
 * - clean up libs
 * - rebase on latest master, clean up
 * - move cache trimming to API?
 * - clean up metrics/activemetrics/displayed_metric -- inheritance rather than pointer
 * - Add documentation
 * - Test cubie compile / libs

 --- style ----
 * fix variable style/case,
 * reduce use of STL strings
 * Can use AtScopeExit for object cleanup in tiff generation
 * Use consistent string conventions ( _()?,  _T()?s )
 * replace #defines in skysight.hpp with better c++ idioms
* Use static_cast<> instead of c casts
 */

Skysight *Skysight::self;

/*
 *
 * Img File
 *
 */
SkysightImageFile::SkysightImageFile(Path _filename) {
  filename = _filename;
  fullpath = AllocatedPath::Build(Skysight::GetLocalPath(), filename);
  SkysightImageFile(filename, fullpath);
}

SkysightImageFile::SkysightImageFile(Path _filename, Path _path) { 
  filename = _filename;
  fullpath = _path;
  region = tstring(_("INVALID"));
  metric = tstring(_("INVALID"));
  datetime = 0;
  is_valid = false;
  mtime = 0;

  //images are in format region-metric-datetime.tif
  if (!filename.EndsWithIgnoreCase(".tif"))
    return;

  tstring file_base = filename.GetBase().c_str();

  std::size_t p = file_base.find(_("-"));
  if (p == tstring::npos)
    return;

  tstring reg = file_base.substr(0, p);
  tstring rem = file_base.substr(p+1);

  p = rem.find(_("-"));
  if (p == tstring::npos)
    return;
  tstring met = rem.substr(0, p);

  tstring dt = rem.substr(p+1);
  unsigned yy, mm, dd, hh, ii;
  try {
    yy = stoi(dt.substr(0, 4));
    mm = stoi(dt.substr(4, 2));
    dd = stoi(dt.substr(6, 2));
    hh = stoi(dt.substr(8, 2));
    ii = stoi(dt.substr(10, 2));
  } catch (const std::exception &e) {
    LogFormat("Skysight: failed to parse image filename: %s", e.what());
    return;
  }

  BrokenDateTime d = BrokenDateTime(yy, mm, dd, hh, ii);
  if (!d.IsPlausible())
    return;

  datetime = std::chrono::system_clock::to_time_t(d.ToTimePoint());

  mtime = std::chrono::system_clock::to_time_t(File::GetLastModification(fullpath));

  region = reg;
  metric = met;
  is_valid = true;
}

/*
 * ******   ACTIVE METRICS ************
 *
 */
bool
Skysight::IsActiveMetricLocked(const TCHAR *const id) const
{
  for (const auto &i : active_metrics)
    if (!i.id.compare(id))
      return true;
  return false;
}

bool
Skysight::IsActiveMetric(const TCHAR *const id)
{
  const std::lock_guard lock{active_metrics_mutex};
  return IsActiveMetricLocked(id);
}

bool
Skysight::ActiveMetricsFull()
{
  const std::lock_guard lock{active_metrics_mutex};
  return (active_metrics.size() >= SKYSIGHT_MAX_METRICS);
}

int
Skysight::AddActiveMetricLocked(const TCHAR *const id)
{
  if (!api->MetricExists(tstring(id)))
    return -3;

  if (IsActiveMetricLocked(id))
    return -1;

  if (active_metrics.size() >= SKYSIGHT_MAX_METRICS)
    return -2;

  SkysightActiveMetric m{tstring(id), 0, 0, 0};

  GetActiveMetricState(id, m);

  active_metrics.push_back(m);
  SaveActiveMetricsLocked();
  return active_metrics.size() - 1;
}

int
Skysight::AddActiveMetric(const TCHAR *const id)
{
  const std::lock_guard lock{active_metrics_mutex};
  return AddActiveMetricLocked(id);
}

void
Skysight::RefreshActiveMetric(tstring id)
{
  const std::lock_guard lock{active_metrics_mutex};
  for (auto &i : active_metrics) {
    if (!i.id.compare(id)) {
      GetActiveMetricState(id, i);
    }
  }
}

SkysightActiveMetric
Skysight::GetActiveMetric(int index)
{
  const std::lock_guard lock{active_metrics_mutex};
  if (index < 0 || (size_t)index >= active_metrics.size()) {
    LogFormat("Skysight: GetActiveMetric index %d out of range (size=%zu)",
              index, active_metrics.size());
    return SkysightActiveMetric{_T(""), 0, 0, 0};
  }
  return active_metrics[index];
}

SkysightActiveMetric
Skysight::GetActiveMetric(const tstring id)
{
  const std::lock_guard lock{active_metrics_mutex};
  for (auto &i : active_metrics)
    if (!i.id.compare(id))
      return i;
  LogFormat("Skysight: GetActiveMetric(id) '%s' not found", id.c_str());
  return SkysightActiveMetric{_T(""), 0, 0, 0};
}

void
Skysight::SetActveMetricUpdateState(const tstring id, bool state)
{
  const std::lock_guard lock{active_metrics_mutex};
  for (auto &i : active_metrics) {
    if (!i.id.compare(id)) {
      i.updating = state;
      return;
    }
  }
}

void
Skysight::RemoveActiveMetric(int index)
{
  const std::lock_guard lock{active_metrics_mutex};
  assert(index < (int)active_metrics.size());
  active_metrics.erase(active_metrics.begin() + index);
  SaveActiveMetricsLocked();
}

void
Skysight::RemoveActiveMetric(const tstring id)
{
  const std::lock_guard lock{active_metrics_mutex};
  bool removed = false;
  for (auto i = active_metrics.begin(); i != active_metrics.end(); ) {
    if (i->id == id) {
      i = active_metrics.erase(i);
      removed = true;
    } else {
      ++i;
    }
  }
  if (!removed)
    LogFormat("Skysight: RemoveActiveMetric '%s' not found", id.c_str());
  SaveActiveMetricsLocked();
}

bool
Skysight::ActiveMetricsUpdating()
{
  const std::lock_guard lock{active_metrics_mutex};
  for (const auto &i : active_metrics)
    if (i.updating) return true;
  return false;
}

int
Skysight::NumActiveMetrics()
{
  const std::lock_guard lock{active_metrics_mutex};
  return (int)active_metrics.size();
}

void
Skysight::SaveActiveMetricsLocked()
{
  tstring am_list;

  if (!active_metrics.empty()) {
    for (const auto &i : active_metrics) {
      am_list += i.id;
      am_list += ",";
    }
    am_list.pop_back();
  }

  Profile::Set(ProfileKeys::SkysightActiveMetrics, am_list.c_str());
}

void
Skysight::SaveActiveMetrics()
{
  const std::lock_guard lock{active_metrics_mutex};
  SaveActiveMetricsLocked();
}

void
Skysight::LoadActiveMetrics()
{
  {
    const std::lock_guard lock{active_metrics_mutex};
    active_metrics.clear();

    const char *s = Profile::Get(ProfileKeys::SkysightActiveMetrics);
    if (s == NULL)
      return;
    tstring am_list = tstring(s);
    size_t pos;
    while ((pos = am_list.find(",")) != tstring::npos) {
      AddActiveMetricLocked(am_list.substr(0, pos).c_str());
      am_list.erase(0, pos + 1);
    }
    AddActiveMetricLocked(am_list.c_str()); // last one
  }

  const TCHAR *const d = Profile::Get(ProfileKeys::SkysightDisplayedMetric);
  if (d == NULL)
    return;

  // IsActiveMetric and SetDisplayedMetric each take active_metrics_mutex
  // internally; they must NOT be called while we still hold it (non-recursive).
  if (!IsActiveMetric(d))
    return;

  SetDisplayedMetric(d);
}

bool
Skysight::IsReady(__attribute__((unused)) bool force_update)
{
  if (email.empty() || password.empty() || region.empty())
    return false;

  return (NumMetrics() > 0);
}

Skysight::Skysight(CurlGlobal &_curl)
{
  self = this;
  curl = &_curl;
  Init();
}

Skysight::~Skysight()
{
  /*
   * Tear down the API (and its background download/decode threads) while this
   * object and its active_metrics_mutex are still alive, so any thread mid-way
   * through the DownloadComplete callback finishes safely.  Only afterwards
   * clear `self`, so any later stray callback hits the `if (!self)` guard.
   */
  delete api;
  api = nullptr;
  if (self == this)
    self = nullptr;
}

void
Skysight::Init()
{
  const auto settings = CommonInterface::GetComputerSettings().weather.skysight;
  region = settings.region.c_str();
  email = settings.email.c_str();
  password = settings.password.c_str();

  api = new SkysightAPI(region);
  api->FetchInitialData(email, password, APIInited);
  CleanupFiles();
}

void
Skysight::APIInited(__attribute__((unused)) const tstring details, __attribute__((unused)) const bool success,
            __attribute__((unused)) const tstring layer_id, __attribute__((unused)) const uint64_t time_index)
{
  if (!self || !self->api)
    return;

  if (self->api->metrics.size()) {
    self->LoadActiveMetrics();
    self->Render(true);
  }
}

bool
Skysight::GetActiveMetricState(tstring metric_name, SkysightActiveMetric &m)
{
  tstring search_pattern = region + "-" + metric_name + "*";
  std::vector<SkysightImageFile> img_files = ScanFolder(search_pattern);

  if (img_files.size() > 0) {
    uint64_t min_date = (uint64_t)std::numeric_limits<uint64_t>::max;
    uint64_t max_date = 0;
    uint64_t updated = 0;

    for (auto &i: img_files) {
      min_date = std::min(min_date, i.datetime);
      max_date = std::max(max_date, i.datetime);
      updated  = std::max(updated, i.mtime);
    }
    if (MetricExists(metric_name)) {
      m.id = metric_name;
      m.from = min_date;
      m.to = max_date;
      m.mtime = updated;

      return true;
    }
  }

  return false;
}

std::vector<SkysightImageFile>
Skysight::ScanFolder(tstring search_string = "*.tif")
{
  //start by checking for output files
  std::vector<SkysightImageFile> file_list;

  struct SkysightFileVisitor: public File::Visitor {
    std::vector<SkysightImageFile> &file_list;
    explicit SkysightFileVisitor(std::vector<SkysightImageFile> &_file_list):
      file_list(_file_list) {}

    void Visit(Path path, Path filename) override {
      //is this a tif filename
      if (filename.EndsWithIgnoreCase(".tif")) {
        SkysightImageFile img_file = SkysightImageFile(filename, path);
        if (img_file.is_valid)
          file_list.emplace_back(img_file);
      }
    }

  } visitor(file_list);

  Directory::VisitSpecificFiles(GetLocalPath(), _T(search_string.c_str()),
				visitor);
  return file_list;
}

void
Skysight::CleanupFiles()
{
  struct SkysightFileVisitor: public File::Visitor {
    explicit SkysightFileVisitor(const uint64_t _to): to(_to) {}
    const uint64_t to;
    void Visit(Path path, Path filename) override {
      if (filename.EndsWithIgnoreCase(".tif")) {
        SkysightImageFile img_file = SkysightImageFile(filename, path);
        if ((img_file.mtime <= (to - (60*60*24*5))) ||
	    (img_file.datetime < (to - (60*60*24))) ) {
          File::Delete(path);
        }
      }
    }
  } visitor(std::chrono::system_clock::to_time_t(Skysight::GetNow().ToTimePoint()));

  Directory::VisitSpecificFiles(GetLocalPath(), _T("*.tif"), visitor);
}

BrokenDateTime
Skysight::FromUnixTime(uint64_t t)
{
  return api->FromUnixTime(t);
}

void
Skysight::Render(bool force_update)
{
  if (!displayed_metric.id.empty()) {
    //set by dl callback
    if (update_flag) {
      DisplayActiveMetric(displayed_metric.id.c_str());
    }

    //Request next images
    BrokenDateTime now = Skysight::GetNow(force_update);
    if (force_update ||
	(!update_flag && displayed_metric < GetForecastTime(now))) {
      force_update = false;
      api->GetImageAt(displayed_metric.id.c_str(), now,
		      now + std::chrono::seconds(60*60), DownloadComplete);
    }
  }
}

BrokenDateTime
Skysight::GetForecastTime(BrokenDateTime curr_time)
{
  if (!curr_time.IsPlausible())
    curr_time = Skysight::GetNow();

  if ((curr_time.minute >= 15) && (curr_time.minute < 45))
    curr_time.minute = 30;
  else if (curr_time.minute >= 45) {
    curr_time.minute = 0;
    curr_time = curr_time + std::chrono::seconds(60*60);
  }
  else if (curr_time.minute < 15)
    curr_time.minute = 0;

  curr_time.second = 0;
  return curr_time;
}

bool
Skysight::SetDisplayedMetric(const TCHAR *const id,
			     BrokenDateTime forecast_time)
{
  if (!IsActiveMetric(id))
    return false;

  displayed_metric = DisplayedMetric(tstring(id), forecast_time);

  return true;
}

void
Skysight::DownloadComplete(__attribute__((unused)) const tstring details, const bool success,
                const tstring layer_id, __attribute__((unused)) const uint64_t time_index)
{
  if (!self)
    return;

  self->SetActveMetricUpdateState(layer_id, false);
  self->RefreshActiveMetric(layer_id);

  if (success && (self->displayed_metric == layer_id.c_str()))
    self->update_flag = true;
}

bool
Skysight::DownloadActiveMetric(tstring id = "*")
{
  BrokenDateTime now = Skysight::GetNow();
  if (id == "*") {
    std::vector<tstring> ids;
    {
      const std::lock_guard lock{active_metrics_mutex};
      for (const auto &i : active_metrics)
        ids.push_back(i.id);
    }
    for (const auto &mid : ids) {
      SetActveMetricUpdateState(mid, true);
      api->GetImageAt(mid.c_str(), now, now + std::chrono::seconds(60*60*24),
                      DownloadComplete);
    }
  } else {
    SetActveMetricUpdateState(id, true);
    api->GetImageAt(id.c_str(), now, now + std::chrono::seconds(60*60*24), DownloadComplete);
  }
  return true;
}

void
Skysight::OnCalculatedUpdate(const MoreData &basic,
			     __attribute__((unused)) const DerivedInfo &calculated)
{
  // maintain current time -- for use in replays etc.
  // Cannot be accessed directly from chid threads
  curr_time = basic.date_time_utc;
}

BrokenDateTime
Skysight::GetNow(bool use_system_time)
{
  if (use_system_time)
    return BrokenDateTime::NowUTC();

  return (curr_time.IsPlausible()) ? curr_time : BrokenDateTime::NowUTC();
}

bool
Skysight::DisplayActiveMetric(const TCHAR *const id)
{
  update_flag = false;

  if (!id) {
    displayed_metric.clear();
    auto *map = UIGlobals::GetMap();
    if (map == nullptr)
      return false;

#ifdef ENABLE_OPENGL
    map->SetOverlay(nullptr);
#endif
    Profile::Set(ProfileKeys::SkysightDisplayedMetric, "");
    return true;
  }

  if (!IsActiveMetric(id))
    return false;

  Profile::Set(ProfileKeys::SkysightDisplayedMetric, id);

  BrokenDateTime now = GetForecastTime(Skysight::GetNow());

  int offset = 0;
  uint64_t n = std::chrono::system_clock::to_time_t(now.ToTimePoint());

  uint64_t test_time;
  bool found = false;
  NarrowString<256> filename;
  BrokenDateTime bdt;
  int max_offset = (60*60);

  //TODO: We're only searching w a max offset of 1 hr, simplify this!
  while (!found) {
    //look back for closest forecast first, then look forward
    for (int j=0; j <= 1; ++j) {
      test_time = n + ( offset * ((2*j)-1) );

      bdt = FromUnixTime(test_time);
      filename.Format("%s-%s-%04u%02u%02u%02u%02u.tif",
            region.c_str(), id,
            bdt.year, bdt.month,
            bdt.day, bdt.hour, bdt.minute);

      if (File::Exists(AllocatedPath::Build(GetLocalPath(),
					    filename.c_str()))) {
        found = true;
        break;
      }
      if (offset == 0)
	break;
    }
    if (!found)
      offset += (60*30);

    if (offset > max_offset)
      break;
  }

  if (!found) {
    SetDisplayedMetric(id);
    return false;
  }

  if (!SetDisplayedMetric(id, bdt))
    return false;

#ifdef ENABLE_OPENGL
  auto path = AllocatedPath::Build(Skysight::GetLocalPath(), filename.c_str());
  StaticString<256> desc;
  tstring metric_name;
  api->TryGetMetricName(displayed_metric.id, metric_name);
  desc.Format("Skysight: %s (%04u-%02u-%02u %02u:%02u)",
	      metric_name.empty() ? displayed_metric.id.c_str() : metric_name.c_str(),
	      bdt.year, bdt.month, bdt.day, bdt.hour, bdt.minute);
  tstring label = desc.c_str();

  auto *map = UIGlobals::GetMap();
  if (map == nullptr)
    return false;

  LogFormat("Skysight::DisplayActiveMetric %s", path.c_str());
  std::unique_ptr<MapOverlayBitmap> bmp;
  try {
    bmp.reset(new MapOverlayBitmap(path));
  } catch (...) {
    LogError(std::current_exception(), "MapOverlayBitmap load error");
    return false;
  }

  bmp->SetAlpha(0.6);
  bmp->SetLabel(label);
  map->SetOverlay(std::move(bmp));
  return true;
#else
  /* MapOverlayBitmap is only implemented for OpenGL; the metric has
     been selected, but there is nothing to draw on the map */
  return false;
#endif
}
