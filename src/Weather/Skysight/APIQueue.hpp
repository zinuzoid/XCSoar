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
#ifndef WEATHER_SKYSIGHTAPI_QUEUE_HPP
#define WEATHER_SKYSIGHTAPI_QUEUE_HPP

#include "Request.hpp"
#include "CDFDecoder.hpp"
#include "Metrics.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include <vector>

class SkysightAPIQueue final {
  std::mutex request_queue_mutex;
  std::mutex process_mutex;
  std::vector<std::unique_ptr<SkysightAsyncRequest>> request_queue;
  std::vector<std::unique_ptr<CDFDecoder>> decode_queue;
  bool is_busy = false;
  bool is_clearing = false;
  tstring key;
  uint64_t key_expiry_time = 0;
  tstring email;
  tstring password;
  bool is_emergency_stop = false;
  unsigned total_login_requests = 0;

  void Process();
  UI::PeriodicTimer timer{[this]{ Process(); }};

public:
  SkysightAPIQueue() {};
  ~SkysightAPIQueue();

  void SetCredentials(const tstring _email, const tstring _pass);
  void SetKey(const tstring _key, const uint64_t _key_expiry_time);
  bool IsLoggedIn();
  void AddRequest(std::unique_ptr<SkysightAsyncRequest> request,
		  bool append_end = true);
  void AddDecodeJob(std::unique_ptr<CDFDecoder> &&job);
  void Clear(const tstring msg);
  void DoClearingQueue();
};

#endif
