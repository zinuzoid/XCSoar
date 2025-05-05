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
#include "SkysightAPI.hpp"
#include "APIQueue.hpp"
#include "APIGlue.hpp"
#include "Request.hpp"
#include "CDFDecoder.hpp"
#include "Metrics.hpp"
#include "ui/event/Timer.hpp"
#include "time/BrokenDateTime.hpp"
#include "LogFile.hpp"
#include <vector>

SkysightAPIQueue::~SkysightAPIQueue() {
	LogFormat("SkysightAPIQueue::~SkysightAPIQueue %d", timer.IsActive());
  timer.Cancel();
}

void
SkysightAPIQueue::AddRequest(std::unique_ptr<SkysightAsyncRequest> request,
			     bool append_end)
{
  LogFormat("SkysightAPIQueue::AddRequest type: %d", (unsigned) request->GetType());
  if(request->GetType() == SkysightCallType::Login) {
    if(total_login_requests++ > 1000) {
      LogFormat("SkysightAPIQueue::AddRequest We're doing more than 1000 login requests for the session.");
      is_emergency_stop = true;
    }
  }

  {
    std::lock_guard lock(request_queue_mutex);
    if (!append_end) {
      //Login requests jump to the front of the queue
      request_queue.insert(request_queue.begin(), std::move(request));
    } else {
      request_queue.emplace_back(std::move(request));
    }
    LogFormat("SkysightAPIQueue::AddRequest request_queue.size(): %zu", request_queue.size());
  }
  if (!is_busy && !timer.IsActive())
      timer.Schedule(std::chrono::milliseconds(100));
}

void SkysightAPIQueue::AddDecodeJob(std::unique_ptr<CDFDecoder> &&job) {
  decode_queue.emplace_back(std::move(job));
  if (!is_busy && !timer.IsActive())
      timer.Schedule(std::chrono::milliseconds(100));
}

static void remove_job_from_queue(
  std::vector<std::unique_ptr<SkysightAsyncRequest>> &request_queue,
  SkysightAsyncRequest *job_ptr
) {
  auto find = std::find_if(request_queue.begin(), request_queue.end(), [job_ptr](std::unique_ptr<SkysightAsyncRequest>& it) {
    return it.get() == job_ptr;
  });
  if(find == request_queue.end()) {
    LogFormat("SkysightAPIQueue::Process() Cannot find job to delete! %p", job_ptr);
    assert(false);
  } else {
    request_queue.erase(find);
  }
}

void SkysightAPIQueue::Process()
{
  std::lock_guard lock(process_mutex);
  if(is_emergency_stop) {
    LogFormat("SkysightAPIQueue::Process() is_emergency_stop=true, disable Skysight requests for this session!");
    DoClearingQueue();
    return;
  }

  is_busy = true;

  if(is_clearing) {
    DoClearingQueue();
  } else if (!request_queue.empty()) {
    std::vector<std::unique_ptr<SkysightAsyncRequest>>::iterator job = request_queue.begin();
    SkysightAsyncRequest* job_ptr = (*job).get();

    switch (job_ptr->GetStatus()) {
    case SkysightRequest::Status::Idle:
      //Provide the job with the very latest API key just prior to execution
      if (job_ptr->GetType() == SkysightCallType::Login) {
        job_ptr->SetCredentials("XCSoar-JET", email.c_str(), password.c_str());
        job_ptr->Process();
      } else {
        if (!IsLoggedIn()) {
          // inject a login request at the front of the queue
          SkysightAPI::GenerateLoginRequest();
        } else {
          job_ptr->SetCredentials(key.c_str());
          job_ptr->Process();
        }
      }

      if (!timer.IsActive())
	      timer.Schedule(std::chrono::milliseconds(300));
      break;
    case SkysightRequest::Status::Complete:
    case SkysightRequest::Status::Error:
      job_ptr->Done();
      {
        std::lock_guard lock(request_queue_mutex);
        remove_job_from_queue(request_queue, job_ptr);
      }
      break;
    case SkysightRequest::Status::Busy:
      break;
    case SkysightRequest::Status::EmergencyStop:
      LogFormat("SkysightAPIQueue::Process() SkysightRequest::Status::EmergencyStop");
      job_ptr->Done();
      {
        std::lock_guard lock(request_queue_mutex);
        remove_job_from_queue(request_queue, job_ptr);
      }
      Clear("Emergency stop");
      is_emergency_stop = true;
      break;
    }
  }

  if (!empty(decode_queue)) {
    auto &&decode_job = decode_queue.begin();
    switch ((*decode_job)->GetStatus()) {
    case CDFDecoder::Status::Idle:
      (*decode_job)->DecodeAsync();
      if (!timer.IsActive())
	      timer.Schedule(std::chrono::milliseconds(300));
      break;
    case CDFDecoder::Status::Complete:
    case CDFDecoder::Status::Error:
      (*decode_job)->Done();
      decode_queue.erase(decode_job);
      break;
    case CDFDecoder::Status::Busy:
      break;
    }
  }

  if (empty(request_queue) && empty(decode_queue))
    timer.Cancel();

  is_busy = false;
}

void
SkysightAPIQueue::Clear(const tstring msg)
{
  LogFormat("SkysightAPIQueue::Clear %s", msg.c_str());
  is_clearing = true;
}

void
SkysightAPIQueue::SetCredentials(const tstring _email,
				 const tstring _pass)
{
  password = _pass;
  email = _email;
}

void
SkysightAPIQueue::SetKey(const tstring _key,
			 const uint64_t _key_expiry_time)
{
  key = _key;
  key_expiry_time = _key_expiry_time;
}

bool
SkysightAPIQueue::IsLoggedIn()
{
  auto now = std::chrono::system_clock::to_time_t(BrokenDateTime::NowUTC().ToTimePoint());
  auto diff = (time_t)key_expiry_time - now;
  LogFormat("SkysightAPIQueue::IsLoggedIn() now: %ld key_expiry_time: %ld diff: %ld", (long)now, (long)key_expiry_time, (long)diff);

  return diff > 0;
}

void
SkysightAPIQueue::DoClearingQueue()
{
  LogFormat("SkysightAPIQueue::DoClearingQueue() request_queue: %ld", (long)request_queue.size());
  for (auto &&i = request_queue.begin(); i<request_queue.end(); ++i) {
    auto status = (*i)->GetStatus();
    if (status == SkysightRequest::Status::EmergencyStop) {
      is_emergency_stop = true;
      LogFormat("SkysightAPIQueue::DoClearingQueue() is_emergency_stop: %d", is_emergency_stop);
    } else if (status != SkysightRequest::Status::Busy) {
      (*i)->Done();
      request_queue.erase(i);
    }
  }
  timer.Cancel();
  is_clearing = false;
}
