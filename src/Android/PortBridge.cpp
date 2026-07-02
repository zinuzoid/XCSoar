// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortBridge.hpp"
#include "NativePortListener.hpp"
#include "NativeInputListener.hpp"
#include "java/Array.hxx"
#include "java/Class.hxx"
#include "java/Exception.hxx"

#include <string.h>

jmethodID PortBridge::setListener_method;
jmethodID PortBridge::setInputListener_method;
jmethodID PortBridge::getState_method;
jmethodID PortBridge::drain_method;
jmethodID PortBridge::getBaudRate_method;
jmethodID PortBridge::setBaudRate_method;
jmethodID PortBridge::write_method;

void
PortBridge::Initialise(JNIEnv *env)
{
  Java::Class cls(env, "org/xcsoar/AndroidPort");

  setListener_method = env->GetMethodID(cls, "setListener",
                                        "(Lorg/xcsoar/PortListener;)V");
  setInputListener_method = env->GetMethodID(cls, "setInputListener",
                                             "(Lorg/xcsoar/InputListener;)V");
  getState_method = env->GetMethodID(cls, "getState", "()I");
  drain_method = env->GetMethodID(cls, "drain", "()Z");
  getBaudRate_method = env->GetMethodID(cls, "getBaudRate", "()I");
  setBaudRate_method = env->GetMethodID(cls, "setBaudRate", "(I)Z");
  write_method = env->GetMethodID(cls, "write", "([BI)I");
}

PortBridge::PortBridge(JNIEnv *env, jobject obj)
  :Java::GlobalCloseable(env, obj),
   write_buffer(env, env->NewByteArray(write_buffer_size))
{
  Java::RethrowException(env);
}

void
PortBridge::setListener(JNIEnv *env, PortListener *_listener)
{
  auto listener = _listener != nullptr
    ? Java::LocalObject{env, NativePortListener::Create(env, *_listener)}
    : Java::LocalObject{};

  env->CallVoidMethod(Get(), setListener_method, listener.Get());
  Java::RethrowException(env);
}

void
PortBridge::setInputListener(JNIEnv *env, DataHandler *handler)
{
  auto listener = handler != nullptr
    ? Java::LocalObject{env, NativeInputListener::Create(env, *handler)}
    : Java::LocalObject{};

  env->CallVoidMethod(Get(), setInputListener_method, listener.Get());
  Java::RethrowException(env);
}

int
PortBridge::getState(JNIEnv *env) noexcept
{
  int state = env->CallIntMethod(Get(), getState_method);
  if (Java::DiscardException(env))
    return 1; // STATE_FAILED

  return state;
}

bool
PortBridge::drain(JNIEnv *env) noexcept
{
  bool result = env->CallBooleanMethod(Get(), drain_method);
  if (Java::DiscardException(env))
    return false;

  return result;
}

int
PortBridge::getBaudRate(JNIEnv *env) const noexcept
{
  int baud_rate = env->CallIntMethod(Get(), getBaudRate_method);
  if (Java::DiscardException(env))
    return 0;

  return baud_rate;
}

bool
PortBridge::setBaudRate(JNIEnv *env, int baud_rate)
{
  bool result = env->CallBooleanMethod(Get(), setBaudRate_method, baud_rate);
  Java::RethrowException(env);
  return result;
}

std::size_t
PortBridge::write(JNIEnv *env, std::span<const std::byte> src)
{
  if (src.size() > write_buffer_size)
    src = src.first(write_buffer_size);

  memcpy(Java::ByteArrayElements{env, write_buffer}.get(),
         src.data(), src.size());

  int nbytes = env->CallIntMethod(Get(), write_method,
                                  write_buffer.Get(), src.size());
  Java::RethrowException(env);
  if (nbytes <= 0)
    throw std::runtime_error{"Port write failed"};

  return (std::size_t)nbytes;
}
