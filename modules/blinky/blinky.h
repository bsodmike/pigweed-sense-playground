// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <chrono>

#include "modules/led/monochrome_led.h"
#include "modules/led/polychrome_led.h"
#include "modules/timer_future/timer_future.h"
#include "modules/worker/worker.h"
#include "pw_allocator/allocator.h"
#include "pw_async2/coro_or_else_task.h"
#include "pw_async2/dispatcher.h"
#include "pw_chrono/system_clock.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace sense {

/// Simple component that blinks the on-board LED.
class Blinky final {
 public:
  static constexpr uint32_t kDefaultIntervalMs = 1000;
  static constexpr pw::chrono::SystemClock::duration kDefaultInterval =
      pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(kDefaultIntervalMs));

  Blinky();
  ~Blinky();

  /// Injects this object's dependencies.
  ///
  /// This method MUST be called before using any other method.
  void Init(pw::async2::Dispatcher& dispatcher,
            pw::Allocator& allocator,
            MonochromeLed& monochrome_led,
            PolychromeLed& polychrome_led);

  /// Turns the LED on if it is off, and off if it is on.
  void Toggle() PW_LOCKS_EXCLUDED(lock_);

  /// Sets the state of the LED.
  void SetLed(bool on) PW_LOCKS_EXCLUDED(lock_);

  /// Queues a sequence of call backs to blink the configured number of times.
  ///
  /// @param  blink_count   The number of times to blink the LED.
  /// @param  interval_ms   The duration of a blink, in milliseconds.
  pw::Status Blink(uint32_t blink_count, uint32_t interval_ms)
      PW_LOCKS_EXCLUDED(lock_);

  /// Blink LED twice.
  pw::Status BlinkTwice()
      PW_LOCKS_EXCLUDED(lock_);

  /// Fades the LED on and off continuously.
  ///
  /// @param  interval_ms   The duration of a fade cycle, in milliseconds.
  void Pulse(uint32_t interval_ms) PW_LOCKS_EXCLUDED(lock_);

  /// Sets the color of the RGB LED>
  void SetRgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
      PW_LOCKS_EXCLUDED(lock_);

  /// Cycles the RGB LED through all the colors.
  void Rainbow(uint32_t interval_ms) PW_LOCKS_EXCLUDED(lock_);

  /// Returns whether this instance is currently blinking or not.
  bool IsIdle() const PW_LOCKS_EXCLUDED(lock_);

 private:
  /// Creates a blinking coroutine.
  pw::async2::Coro<pw::Status> BlinkLoop(
      pw::async2::CoroContext&,
      uint32_t blink_count,
      pw::chrono::SystemClock::duration interval) PW_LOCKS_EXCLUDED(lock_);

  pw::async2::Dispatcher* dispatcher_;
  pw::Allocator* allocator_;
  mutable pw::sync::InterruptSpinLock lock_;
  MonochromeLed* monochrome_led_ PW_GUARDED_BY(lock_) = nullptr;
  PolychromeLed* polychrome_led_ PW_GUARDED_BY(lock_) = nullptr;
  AsyncTimer timer_;
  mutable pw::async2::CoroOrElseTask blink_task_;
};

}  // namespace sense
