// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "AsioThread.hpp"

void
AsioThread::Start()
{
  assert(!IsDefined());

  event_loop.SetAlive(true);
  Thread::Start();
}

void
AsioThread::Stop()
{
  /* may have been stopped already by whoever needs the event loop to be
     dead before tearing down objects which live on it */
  if (!IsDefined())
    return;

  /* set the "stop" flag and wake up the thread */
  event_loop.InjectBreak();

  /* wait for the thread to finish */
  Join();

  event_loop.SetAlive(false);
}

void
AsioThread::Run() noexcept
{
  event_loop.Run();
}
