// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rclcpp/utilities.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "rcl/error_handling.h"
#include "rcl/rcl.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

// Determine if sigaction is available
#if __APPLE__ || _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE
#define HAS_SIGACTION
#endif

/// Represent the status of the global interrupt signal.
static volatile sig_atomic_t g_signal_status = 0;
/// Guard condition for interrupting the rmw implementation when the global interrupt signal fired.
static rcl_guard_condition_t g_sigint_guard_cond_handle =
  rcl_get_zero_initialized_guard_condition();
/// Condition variable for timed sleep (see sleep_for).
static std::condition_variable g_interrupt_condition_variable;
static std::atomic<bool> g_is_interrupted(false);
/// Mutex for protecting the global condition variable.
static std::mutex g_interrupt_mutex;

#ifdef HAS_SIGACTION
static struct sigaction old_action;
#else
static void (* old_signal_handler)(int) = 0;
#endif

void
#ifdef HAS_SIGACTION
signal_handler(int signal_value, siginfo_t * siginfo, void * context)
#else
signal_handler(int signal_value)
#endif
{
  // TODO(wjwwood): remove? move to console logging at some point?
  printf("signal_handler(%d)\n", signal_value);
#ifdef HAS_SIGACTION
  if (old_action.sa_flags & SA_SIGINFO) {
    if (old_action.sa_sigaction != NULL) {
      old_action.sa_sigaction(signal_value, siginfo, context);
    }
  } else {
    // *INDENT-OFF*
    if (
      old_action.sa_handler != NULL &&  // Is set
      old_action.sa_handler != SIG_DFL &&  // Is not default
      old_action.sa_handler != SIG_IGN)  // Is not ignored
    // *INDENT-ON*
    {
      old_action.sa_handler(signal_value);
    }
  }
#else
  if (old_signal_handler) {
    old_signal_handler(signal_value);
  }
#endif
  g_signal_status = signal_value;
  rcl_ret_t status = rcl_trigger_guard_condition(&g_sigint_guard_cond_handle);
  if (status != RCL_RET_OK) {
    fprintf(stderr,
      "[rclcpp::error] failed to trigger guard condition: %s\n", rcl_get_error_string_safe());
  }
  g_is_interrupted.store(true);
  g_interrupt_condition_variable.notify_all();
}

void
rclcpp::utilities::init(int argc, char * argv[])
{
  g_is_interrupted.store(false);
  if (rcl_init(argc, argv, rcl_get_default_allocator()) != RCL_RET_OK) {
    // *INDENT-OFF* (prevent uncrustify from making unecessary indents here)
    throw std::runtime_error(
      std::string("failed to initialize rmw implementation: ") + rcl_get_error_string_safe());
    // *INDENT-ON*
  }
#ifdef HAS_SIGACTION
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = ::signal_handler;
  action.sa_flags = SA_SIGINFO;
  ssize_t ret = sigaction(SIGINT, &action, &old_action);
  if (ret == -1)
#else
  ::old_signal_handler = std::signal(SIGINT, ::signal_handler);
  // NOLINTNEXTLINE(readability/braces)
  if (::old_signal_handler == SIG_ERR)
#endif
  {
    const size_t error_length = 1024;
    // NOLINTNEXTLINE(runtime/arrays)
    char error_string[error_length];
#ifndef _WIN32
#ifdef _GNU_SOURCE
    char * msg = strerror_r(errno, error_string, error_length);
    if (msg != error_string) {
      strncpy(error_string, msg, error_length);
      msg[error_length - 1] = '\0';
    }
#else
    int error_status = strerror_r(errno, error_string, error_length);
    if (error_status != 0) {
      throw std::runtime_error("Failed to get error string for errno: " + std::to_string(errno));
    }
#endif
#else
    strerror_s(error_string, error_length, errno);
#endif
    // *INDENT-OFF*
    throw std::runtime_error(
      std::string("Failed to set SIGINT signal handler: (" + std::to_string(errno) + ")") +
      error_string);
    // *INDENT-ON*
  }
  rcl_guard_condition_options_t options = rcl_guard_condition_get_default_options();
  if (rcl_guard_condition_init(&g_sigint_guard_cond_handle, options) != RCL_RET_OK) {
    throw std::runtime_error(std::string(
              "Couldn't initialize guard condition: ") + rcl_get_error_string_safe());
  }
}

bool
rclcpp::utilities::ok()
{
  return ::g_signal_status == 0;
}

void
rclcpp::utilities::shutdown()
{
  g_signal_status = SIGINT;
  if (rcl_trigger_guard_condition(&g_sigint_guard_cond_handle) != RCL_RET_OK) {
    fprintf(stderr,
      "[rclcpp::error] failed to trigger guard condition: %s\n", rmw_get_error_string_safe());
  }
  g_is_interrupted.store(true);
  g_interrupt_condition_variable.notify_all();
}

rcl_guard_condition_t *
rclcpp::utilities::get_global_sigint_guard_condition()
{
  return &::g_sigint_guard_cond_handle;
}

bool
rclcpp::utilities::sleep_for(const std::chrono::nanoseconds & nanoseconds)
{
  std::chrono::nanoseconds time_left = nanoseconds;
  {
    std::unique_lock<std::mutex> lock(::g_interrupt_mutex);
    auto start = std::chrono::steady_clock::now();
    ::g_interrupt_condition_variable.wait_for(lock, nanoseconds);
    time_left -= std::chrono::steady_clock::now() - start;
  }
  if (time_left > std::chrono::nanoseconds::zero() && !g_is_interrupted) {
    return sleep_for(time_left);
  }
  // Return true if the timeout elapsed successfully, otherwise false.
  return !g_is_interrupted;
}
