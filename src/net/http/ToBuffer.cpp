/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2021 The XCSoar Project
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

#include "ToBuffer.hpp"
#include "Request.hxx"
#include "Handler.hxx"
#include "Progress.hpp"
#include "Operation/Operation.hpp"
#include "thread/AsyncWaiter.hxx"
#include "util/ScopeExit.hxx"
#include "util/ConstBuffer.hxx"
#include "LogFile.hpp"

#include <cstdint>
#include <string.h>

#ifdef ANDROID
#include "java/Global.hxx"
#include "java/URL.hxx"
#include "java/InputStream.hxx"
#endif

class DownloadToBufferHandler final : public CurlResponseHandler {
  uint8_t *buffer;
  const size_t max_size;

  size_t received = 0;

  AsyncWaiter waiter;

public:
  DownloadToBufferHandler(void *_buffer, size_t _max_size)
    :buffer((uint8_t *)_buffer), max_size(_max_size) {}

  size_t GetReceived() const {
    return received;
  }

  void Cancel() noexcept {
    waiter.SetDone();
  }

  void Wait() {
    waiter.Wait();
  }

  /* virtual methods from class CurlResponseHandler */
  void OnHeaders(unsigned status,
                 std::multimap<std::string, std::string> &&headers) override {
  }

  void OnData(ConstBuffer<void> data) override {
    size_t remaining = max_size - received;
    if (remaining == 0) {
      /* buffer is full - stop here */
      waiter.SetDone();
      throw Pause{};
    }

    if (waiter.IsDone())
      throw Pause{};

    if (data.size > remaining)
      data.size = remaining;

    memcpy(buffer + received, data.data, data.size);
    received += data.size;
  }

  void OnEnd() override {
    waiter.SetDone();
  }

  void OnError(std::exception_ptr e) noexcept override {
    waiter.SetError(std::move(e));
  }
};

size_t
Net::DownloadToBuffer(CurlGlobal &curl, const char *url,
                      const char *username, const char *password,
                      void *_buffer, size_t max_length,
                      OperationEnvironment &env)
{
  DownloadToBufferHandler handler(_buffer, max_length);

#ifdef ANDROID
  try {
    jobject connection = Java::URL::openConnection(Java::GetEnv(), url);
    if(Java::GetEnv()->ExceptionCheck()) {
      throw Java::GetEnv()->ExceptionOccurred();
      return -1;
    }
    if (connection == nullptr) {
      return -1;
    }

    Java::URLConnection::setConnectTimeout(Java::GetEnv(), connection, 10000);
    if(Java::GetEnv()->ExceptionCheck()) {
      throw Java::GetEnv()->ExceptionOccurred();
      return -1;
    }

    jobject is = Java::URLConnection::getInputStream(Java::GetEnv(), connection);
    if(Java::GetEnv()->ExceptionCheck()) {
      throw Java::GetEnv()->ExceptionOccurred();
      return -1;
    }
    if (is == nullptr) {
      return -1;
    }

    jbyteArray jbufferArr = Java::GetEnv()->NewByteArray(max_length);
    int64_t len = Java::InputStream::read(Java::GetEnv(), is, jbufferArr);

    jbyte *jbuffer = Java::GetEnv()->GetByteArrayElements(jbufferArr, nullptr);
    jbuffer[len] = 0;
    char *buffer =  (char*) jbuffer;

    handler.OnData(ConstBuffer<void>(buffer, len));
  }
  catch (...) {
    LogError(std::current_exception(), "DownloadToBuffer ANDROID");
    Java::GetEnv()->ExceptionClear();
    #ifdef DEBUG
    throw std::current_exception();
    #endif
  }
#else
  CurlRequest request(curl, url, handler);
  AtScopeExit(&request) { request.StopIndirect(); };

  request.SetFailOnError();

  if (username != nullptr)
    request.SetOption(CURLOPT_USERNAME, username);
  if (password != nullptr)
    request.SetOption(CURLOPT_PASSWORD, password);

  env.SetCancelHandler([&]{
    request.StopIndirect();
    handler.Cancel();
  });

  AtScopeExit(&env) { env.SetCancelHandler({}); };

  const Net::ProgressAdapter progress(request.GetEasy(), env);

  request.StartIndirect();
  handler.Wait();
#endif

  return handler.GetReceived();
}

void
Net::DownloadToBufferJob::Run(OperationEnvironment &env)
{
  length = DownloadToBuffer(curl, url, username, password,
                            buffer, max_length, env);

}
