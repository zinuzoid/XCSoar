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

#include "Request.hpp"
#include "APIGlue.hpp"
#include "SkysightAPI.hpp"
#include "thread/StandbyThread.hpp"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"
#include "net/http/Init.hpp"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"
#include "io/FileLineReader.hpp"
#include "LogFile.hpp"
#include "util/StaticString.hxx"
#include "lib/curl/Slist.hxx"
#include "Version.hpp"

void
SkysightRequest::FileHandler::OnData(std::span<const std::byte> data)
{
  size_t written = fwrite(data.data(), sizeof(std::byte), data.size(), file);
  if (written != (size_t)data.size())
    throw SkysightRequestError();

  received += written;
}

void
SkysightRequest::FileHandler::OnHeaders(unsigned status,
    __attribute__((unused)) Curl::Headers &&headers) {
  std::string content_encoding;
  if(auto it = headers.find("content-encoding"); it != headers.end()) {
    content_encoding = it->second.data();
  }
  LogFormat("FileHandler::OnHeaders status:%d content-encoding:%s", status, content_encoding.data());
}

void
SkysightRequest::FileHandler::OnEnd() {
  const std::lock_guard<Mutex> lock(mutex);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::FileHandler::OnError(std::exception_ptr e) noexcept {
  LogFormat("FileHandler::OnError");
  LogError(e);
  const std::lock_guard<Mutex> lock(mutex);
  error = std::move(e);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::FileHandler::Wait() {
  std::unique_lock<Mutex> lock(mutex);
  cond.wait(lock, [this]{ return done; });

  if (error)
    std::rethrow_exception(error);
}

size_t
SkysightRequest::FileHandler::GetReceived() const
{
  return received;
}

size_t
SkysightRequest::BufferHandler::GetReceived() const
{
  return received;
}

unsigned
SkysightRequest::BufferHandler::GetHeaderStatus() const
{
  return header_status;
}

void
SkysightRequest::BufferHandler::OnData(std::span<const std::byte> data)
{
  if(received + data.size() >= max_size) {
    LogFormat("BufferHandler::OnData received + data.size(): %zu is bigger "
              "than max_size: %zu, skipping the received data.",
              received + data.size(), max_size);
    return;
  }
  memcpy(buffer + received, data.data(), data.size());
  received += data.size();
}

void
SkysightRequest::BufferHandler::OnHeaders(unsigned status,
    __attribute__((unused)) Curl::Headers &&headers) {
  LogFormat("BufferHandler::OnHeaders status:%d", status);
}

void
SkysightRequest::BufferHandler::OnEnd() {
  const std::lock_guard<Mutex> lock(mutex);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::BufferHandler::OnError(std::exception_ptr e) noexcept {
  LogFormat("BufferHandler::OnError");
  LogError(e);
  const std::lock_guard<Mutex> lock(mutex);
  error = std::move(e);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::BufferHandler::Wait() {
  std::unique_lock<Mutex> lock(mutex);
  cond.wait(lock, [this]{ return done; });

  if (error)
    std::rethrow_exception(error);
}

SkysightRequest::Status
SkysightAsyncRequest::GetStatus()
{
  std::lock_guard<Mutex> lock(mutex);
  Status s = status;
  return s;
}

SkysightCallType
SkysightRequest::GetType()
{
  return args.calltype;
}

void
SkysightRequest::SetCredentials(const TCHAR *_key, const TCHAR *_username, 
				const TCHAR *_password)
{
  key = _key;
  if(_username != nullptr)
    username = _username;
  if(_password)
    password = _password;
}

SkysightCallType
SkysightAsyncRequest::GetType()
{
  std::lock_guard<Mutex> lock(mutex);
  SkysightCallType ct = args.calltype;
  return ct;
}

void
SkysightAsyncRequest::SetCredentials(const TCHAR *_key,
				     const TCHAR *_username,
				     const TCHAR *_password)
{
  std::lock_guard<Mutex> lock(mutex);
  SkysightRequest::SetCredentials(_key, _username, _password);
}

void
SkysightAsyncRequest::TriggerNullCallback(tstring &&ErrText)
{
  if(args.cb)
    args.cb(ErrText.c_str(), false, args.layer.c_str(), args.from);
}

bool
SkysightRequest::Process()
{
  return RequestToFile() == Status::Complete;
}

tstring
SkysightAsyncRequest::GetMessage()
{
  std::lock_guard<Mutex> lock(mutex);
  tstring msg = tstring (_T("Downloading ")) + args.layer;
  return msg;
}

void
SkysightAsyncRequest::Process()
{
  std::lock_guard<Mutex> lock(mutex);
  if (IsBusy()) return;
  Trigger();
}

void
SkysightAsyncRequest::Tick() noexcept
{
  status = Status::Busy;

  mutex.unlock();

  tstring resultStr;

  if (args.to_file) {
    status = RequestToFile();
    resultStr = args.path.c_str();
  } else {
    status = RequestToBuffer(resultStr);
  }

  if (status == Status::Complete) {
    SkysightAPI::ParseResponse(resultStr.c_str(), true, args);
  } else if(status == Status::EmergencyStop) {
    LogFormat("Emergency stop response: %s", resultStr.c_str());
    SkysightAPI::ParseResponse(_T("Received 429 EmergencyStop signal from Skysight."),
    false, args);
  } else {
    SkysightAPI::ParseResponse(_T("Could not fetch data from Skysight server."),
			       false, args);
  }

  mutex.lock();
}

void
SkysightAsyncRequest::Done()
{
  StandbyThread::LockStop();
}

SkysightRequest::Status
SkysightRequest::RequestToFile()
{
  LogFormat("Connecting to %s for %s with key:%s user-agent:%s", args.url.c_str(), args.path.c_str(), key.c_str(), XCSoar_ProductToken);

  Path final_path = Path(args.path.c_str());
  AllocatedPath temp_path = final_path.WithSuffix(".dltemp");

  if (!File::Delete(temp_path) && File::ExistsAny(temp_path))
    return Status::Error;

  FILE *file = _tfopen(temp_path.c_str(), _T("wb"));
  if (file == nullptr)
    return Status::Error;

  bool success = true;
  FileHandler handler(file);
  CurlRequest request(*Net::curl, args.url.c_str(), handler);
  CurlSlist request_headers;

  char api_key_buffer[4096];
  snprintf(api_key_buffer, sizeof(api_key_buffer), "%s: %s", "X-API-Key", key.c_str());
  request_headers.Append(api_key_buffer);
  snprintf(api_key_buffer, sizeof(api_key_buffer), "%s: %s", "User-Agent", XCSoar_ProductToken);
  request_headers.Append(api_key_buffer);

  tstring pBody;
  if (username.length() && password.length()) {
    char content_type_buffer[4096];
    snprintf(content_type_buffer, sizeof(content_type_buffer), "%s: %s", "Content-Type", "application/json");
    request_headers.Append(content_type_buffer);
    NarrowString<1024> creds;
    creds.Format("{\"username\":\"%s\",\"password\":\"%s\"}", username.c_str(), password.c_str());
    pBody = creds.c_str();
    request.SetRequestBody(pBody.c_str(), pBody.length());
    request.SetFailOnError(false);
  }

  request.SetRequestHeaders(request_headers.Get());
  request.SetAcceptEncoding("gzip");
  request.SetVerifyPeer(false);

  try {
    request.StartIndirect();
    handler.Wait();
  } catch (const std::exception &exc) {
    success = false;
  }

  success &= fclose(file) == 0;

  if (!success) File::Delete(temp_path);

  file = NULL;

  if (success) {
    if (!File::Delete(final_path) && File::ExistsAny(final_path)) {
      File::Delete(temp_path);
      return Status::Error;
    }
    std::rename(temp_path.c_str(), args.path.c_str());
    LogFormat("SkysightRequest::RequestToFile received: %zu", handler.GetReceived());
    
  }
  return success ? Status::Complete : Status::Error;
}

SkysightRequest::Status
SkysightRequest::RequestToBuffer(tstring &response)
{
  LogFormat("Connecting to %s for %s with key:%s user-agent:%s", args.url.c_str(), args.path.c_str(), key.c_str(), XCSoar_ProductToken);

  bool success = true;

  char buffer[1024 * 100];
  BufferHandler handler(buffer, sizeof(buffer));
  CurlRequest request(*Net::curl, args.url.c_str(), handler);
  CurlSlist request_headers;

  char api_key_buffer[4096];
  snprintf(api_key_buffer, sizeof(api_key_buffer), "%s: %s", "X-API-Key", key.c_str());
  request_headers.Append(api_key_buffer);
  snprintf(api_key_buffer, sizeof(api_key_buffer), "%s: %s", "User-Agent", XCSoar_ProductToken);
  request_headers.Append(api_key_buffer);

  tstring pBody;
  if (username.length() && password.length()) {
    char content_type_buffer[4096];
    snprintf(content_type_buffer, sizeof(content_type_buffer), "%s: %s", "Content-Type", "application/json");
    request_headers.Append(content_type_buffer);
    NarrowString<1024> creds;
    creds.Format("{\"username\":\"%s\",\"password\":\"%s\"}",
		 username.c_str(), password.c_str());
    pBody = creds.c_str();
    request.SetRequestBody(pBody.c_str(), pBody.length());
    request.SetFailOnError(false);
  }

  request.SetRequestHeaders(request_headers.Get());
  request.SetVerifyPeer(false);

  try {
    request.StartIndirect();
    handler.Wait();
  } catch (const std::exception &exc) {
    success = false;
  }

  response = tstring(buffer,
		     buffer + handler.GetReceived() / sizeof(buffer[0]));

  if(handler.GetHeaderStatus() == 429) {
    // Skysight asked us to stop the session if 429 was returned
    LogFormat("SkysightRequest::RequestToBuffer Received 429 EmergencyStop signal from Skysight.");
    return Status::EmergencyStop;
  }
  return success ? Status::Complete : Status::Error;
}
