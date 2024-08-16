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
#define PW_LOG_MODULE_NAME "BLINKY"
#include "blinky.h"

#include <mutex>

#include "modules/blinky/blinky.h"
#include "pw_async2/coro.h"
#include "pw_log/log.h"

namespace sense {

using ::pw::Allocator;
using ::pw::OkStatus;
using ::pw::Status;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::Dispatcher;

Coro<Status> Blinky::BlinkLoop(CoroContext&,
                               uint32_t blink_count,
                               pw::chrono::SystemClock::duration interval) {
  for (uint32_t blinked = 0; blinked < blink_count || blink_count == 0;
       ++blinked) {
    {
      PW_LOG_INFO("LED blinking: OFF");
      std::lock_guard lock(lock_);
      monochrome_led_->TurnOff();
    }
    co_await timer_.WaitFor(interval);
    {
      PW_LOG_INFO("LED blinking: ON");
      std::lock_guard lock(lock_);
      monochrome_led_->TurnOn();
    }
    co_await timer_.WaitFor(interval);
  }
  {
    std::lock_guard lock(lock_);
    monochrome_led_->TurnOff();
  }
  PW_LOG_INFO("Stopped blinking");
  co_return OkStatus();
}

Blinky::Blinky()
    : blink_task_(Coro<Status>::Empty(), [](Status) {
        PW_LOG_ERROR("Failed to allocate blink loop coroutine.");
      }) {}

void Blinky::Init(Dispatcher& dispatcher,
                  Allocator& allocator,
                  MonochromeLed& monochrome_led,
                  PolychromeLed& polychrome_led) {
  dispatcher_ = &dispatcher;
  allocator_ = &allocator;

  std::lock_guard lock(lock_);
  monochrome_led_ = &monochrome_led;
  monochrome_led_->TurnOff();

  polychrome_led_ = &polychrome_led;
  polychrome_led_->Enable();
  polychrome_led_->TurnOff();
}

Blinky::~Blinky() { blink_task_.Deregister(); }

void Blinky::Toggle() {
  blink_task_.Deregister();
  PW_LOG_INFO("Toggling LED");
  std::lock_guard lock(lock_);
  monochrome_led_->Toggle();
}

void Blinky::SetLed(bool on) {
  blink_task_.Deregister();
  std::lock_guard lock(lock_);
  if (on) {
    PW_LOG_INFO("Setting LED on");
    monochrome_led_->TurnOn();
  } else {
    PW_LOG_INFO("Setting LED off");
    monochrome_led_->TurnOff();
  }
}

pw::Status Blinky::Blink(uint32_t blink_count, uint32_t interval_ms) {
  if (blink_count == 0) {
    PW_LOG_INFO("Blinking forever at a %ums interval", interval_ms);
  } else {
    PW_LOG_INFO(
        "Blinking %u times at a %ums interval", blink_count, interval_ms);
  }

  pw::chrono::SystemClock::duration interval =
      pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(interval_ms));

  blink_task_.Deregister();
  CoroContext coro_cx(*allocator_);
  blink_task_.SetCoro(BlinkLoop(coro_cx, blink_count, interval));
  dispatcher_->Post(blink_task_);
  return OkStatus();
}

pw::Status Blinky::BlinkTwice() {
  uint32_t num_toggles = 4;
  uint32_t interval_ms = 1000;

  PW_LOG_INFO(
      "Blinking %u times at a %ums interval", num_toggles / 2, interval_ms);
  // pw::chrono::SystemClock::duration interval =
  //     pw::chrono::SystemClock::for_at_least(
  //         std::chrono::milliseconds(interval_ms));
  // timer_.Cancel();
  // {
  //   std::lock_guard lock(lock_);
  //   monochrome_led_->TurnOff();
  //   num_toggles_ = num_toggles;
  //   interval_ = interval;
  // }
  // return ScheduleToggle();

  // Error when using code in the docs:
  // Ref: https://pigweed.dev/docs/showcases/sense/tutorial/host_sim.html#create-a-blinktwice-rpc-method
  //   
  //   Use --sandbox_debug to see verbose messages from the sandbox and retain the sandbox build root for debugging
  // modules/blinky/blinky.cc:128:10: error: no member named 'Cancel' in 'sense::AsyncTimer'
  //   128 |   timer_.Cancel();
  //       |   ~~~~~~ ^
  // modules/blinky/blinky.cc:132:5: error: use of undeclared identifier 'num_toggles_'; did you mean 'num_toggles'?
  //   132 |     num_toggles_ = num_toggles;
  //       |     ^~~~~~~~~~~~
  //       |     num_toggles
  // modules/blinky/blinky.cc:120:12: note: 'num_toggles' declared here
  //   120 |   uint32_t num_toggles = 4;
  //       |            ^
  // modules/blinky/blinky.cc:132:18: error: explicitly assigning value of variable of type 'uint32_t' (aka 'unsigned int') to itself [-Werror,-Wself-assign]
  //   132 |     num_toggles_ = num_toggles;
  //       |     ~~~~~~~~~~~~ ^ ~~~~~~~~~~~
  // modules/blinky/blinky.cc:133:5: error: use of undeclared identifier 'interval_'; did you mean 'interval'?
  //   133 |     interval_ = interval;
  //       |     ^~~~~~~~~
  //       |     interval
  // modules/blinky/blinky.cc:125:37: note: 'interval' declared here
  //   125 |   pw::chrono::SystemClock::duration interval =
  //       |                                     ^
  // modules/blinky/blinky.cc:133:15: error: explicitly assigning value of variable of type 'pw::chrono::SystemClock::duration' (aka 'duration<long long, ratio<1, ((TickType_t)1000)>>') to itself [-Werror,-Wself-assign-overloaded]
  //   133 |     interval_ = interval;
  //       |     ~~~~~~~~~ ^ ~~~~~~~~
  // modules/blinky/blinky.cc:135:10: error: use of undeclared identifier 'ScheduleToggle'
  //   135 |   return ScheduleToggle();
  //       |          ^    

  // FIXME just to get this to compile.
  return OkStatus();
}

void Blinky::Pulse(uint32_t interval_ms) {
  blink_task_.Deregister();
  PW_LOG_INFO("Pulsing forever at a %ums interval", interval_ms);
  std::lock_guard lock(lock_);
  monochrome_led_->Pulse(interval_ms);
}

void Blinky::SetRgb(uint8_t red,
                    uint8_t green,
                    uint8_t blue,
                    uint8_t brightness) {
  blink_task_.Deregister();
  PW_LOG_INFO("Setting RGB LED with red=0x%02x, green=0x%02x, blue=0x%02x",
              red,
              green,
              blue);
  std::lock_guard lock(lock_);
  polychrome_led_->SetColor(red, green, blue);
  polychrome_led_->SetBrightness(brightness);
  polychrome_led_->TurnOn();
}

void Blinky::Rainbow(uint32_t interval_ms) {
  blink_task_.Deregister();
  PW_LOG_INFO("Cycling through rainbow at a %ums interval", interval_ms);
  std::lock_guard lock(lock_);
  polychrome_led_->Rainbow(interval_ms);
}

bool Blinky::IsIdle() const {
  std::lock_guard lock(lock_);
  return !blink_task_.IsRegistered();
}

}  // namespace sense
