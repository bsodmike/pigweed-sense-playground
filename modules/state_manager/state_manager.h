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

#include "modules/air_sensor/air_sensor.h"
#include "modules/led/polychrome_led.h"
#include "modules/pubsub/pubsub_events.h"
#include "modules/state_manager/common_base_union.h"
#include "pw_chrono/system_timer.h"
#include "pw_string/string.h"

namespace sense {

// State machine that controls what displays on the LED.
class LedOutputStateMachine {
 public:
  explicit constexpr LedOutputStateMachine(PolychromeLed& led,
                                           uint8_t brightness)
      : state_(kPassthrough),
        brightness_(brightness),
        red_(0),
        green_(0),
        blue_(0),
        led_(&led) {}

  void Override(uint32_t color, uint8_t brightness) {
    state_ = kOverride;
    led_->SetColor(color);
    led_->SetBrightness(brightness);
  }

  void EndOverride() {
    state_ = kPassthrough;
    UpdateLed();
  }

  void SetColor(const LedValue& value) {
    red_ = value.r();
    green_ = value.g();
    blue_ = value.b();
    UpdateLed();
  }

  void SetBrightness(uint8_t brightness) {
    brightness_ = brightness;
    UpdateLed();
  }

 private:
  void UpdateLed() {
    if (state_ == kPassthrough) {
      led_->SetColor(red_, green_, blue_);
      led_->SetBrightness(brightness_);
    }
  }
  enum : bool {
    kPassthrough,  // show stored values and update as they come in
    kOverride,     // display a specific color
  } state_;

  uint8_t brightness_;
  uint8_t red_;
  uint8_t green_;
  uint8_t blue_;

  PolychromeLed* led_;
};

class StateManager {
 public:
  StateManager(PubSub& pubsub, PolychromeLed& led)
      : pubsub_(&pubsub),
        led_(led, kDefaultBrightness),
        state_(*this),
        demo_mode_timer_(
            [this](auto) { pubsub_->Publish(DemoModeTimerExpired{}); }) {}

  void Init();

 private:
  /// How long to show demo modes before returning to the regular AQI monitor.
  static constexpr pw::chrono::SystemClock::duration kDemoModeTimeout =
      std::chrono::seconds(30);

  static constexpr uint8_t kDefaultBrightness = 220;
  static constexpr uint16_t kThresholdIncrement = 128;
  static constexpr uint16_t kMaxThreshold = 768;

  // Represents a state in the Sense app state machine.
  class State {
   public:
    State(StateManager& manager, const char* name)
        : manager_(manager), name_(name) {
      manager_.led_.SetBrightness(kDefaultBrightness);
    }

    virtual ~State() = default;

    // Name of the state for logging.
    const char* name() const { return name_; }

    // Events for handling alarms.
    virtual void AlarmStateChanged(bool alarm) {
      if (manager_.alarmed_ == alarm) {
        return;
      }
      manager_.alarmed_ = alarm;
      if (alarm) {
        manager_.SetState<AirQualityAlarmMode>();
      } else {
        manager_.SetState<AirQualityMode>();
      }
    }

    // Events for releasing buttons.
    virtual void ButtonAReleased() {
      manager_.SetState<AirQualityThresholdMode>();
    }
    virtual void ButtonBReleased() {
      manager_.SetState<AirQualityThresholdMode>();
    }
    virtual void ButtonXReleased() {}
    virtual void ButtonYReleased() {}

    // Events for requested LED values from other components.
    virtual void ProximityModeLedValue(const LedValue&) {}
    virtual void AirQualityModeLedValue(const LedValue&) {}
    virtual void ColorRotationModeLedValue(const LedValue&) {}

    virtual void MorseCodeEdge(const MorseCodeValue&) {}

    // Demo mode returns to air quality mode after a specified time.
    virtual void DemoModeTimerExpired() {}

   protected:
    StateManager& manager() { return manager_; }

   private:
    StateManager& manager_;
    const char* name_;
  };

  // Base class for all demo states to handle button behavior and timer.
  class TimeoutState : public State {
   public:
    TimeoutState(StateManager& manager,
              const char* name,
              pw::chrono::SystemClock::duration timeout)
        : State(manager, name) {
      manager.demo_mode_timer_.InvokeAfter(timeout);
    }

    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void DemoModeTimerExpired() override {
      manager().SetState<AirQualityMode>();
    }
  };

  class AirQualityMode final : public State {
   public:
    AirQualityMode(StateManager& manager)
        : State(manager, "AirQualityMode") {}

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }
    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  class AirQualityThresholdMode final : public TimeoutState {
   public:
    static constexpr auto kThresholdModeTimeout =
        pw::chrono::SystemClock::for_at_least(std::chrono::seconds(3));

    AirQualityThresholdMode(StateManager& manager)
        : TimeoutState(manager, "AirQualityThresholdMode", kThresholdModeTimeout) {
      manager.DisplayThreshold();
    }

    void ButtonAReleased() override {
      manager().IncrementThreshold(kThresholdModeTimeout);
    }

    void ButtonBReleased() override {
      manager().DecrementThreshold(kThresholdModeTimeout);
    }
  };

  class AirQualityAlarmMode final : public State {
   public:
    AirQualityAlarmMode(StateManager& manager)
        : State(manager, "AirQualityAlarmMode") {
      manager.StartMorseReadout(/* repeat: */ true);
    }

    void ButtonYReleased() override {
      manager().pubsub_->Publish(AlarmSilenceRequest{.seconds = 60});
    }
    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetBrightness(value.turn_on ? kDefaultBrightness : 0);
    }
  };

  class MorseReadout final : public State {
   public:
    MorseReadout(StateManager& manager) : State(manager, "MorseReadout") {
      manager.StartMorseReadout(/* repeat: */ false);
    }

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }
    void ButtonYReleased() override { manager().SetState<AirQualityMode>(); }
    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetBrightness(value.turn_on ? kDefaultBrightness : 0);
      if (value.message_finished) {
        manager().SetState<AirQualityMode>();
      }
    }
  };

  class ProximityDemo final : public TimeoutState {
   public:
    ProximityDemo(StateManager& manager)
        : TimeoutState(manager, "ProximityDemo", kDemoModeTimeout) {}

    void ButtonXReleased() override { manager().SetState<MorseCodeDemo>(); }

    void ProximityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  class MorseCodeDemo final : public TimeoutState {
   public:
    MorseCodeDemo(StateManager& manager)
        : TimeoutState(manager, "MorseCodeDemo", kDemoModeTimeout) {
      manager.led_.SetColor(LedValue(0, 255, 255));
      manager.pubsub_->Publish(
          MorseEncodeRequest{.message = "PW", .repeat = 0});
    }

    void ButtonXReleased() override { manager().SetState<ColorRotationDemo>(); }

    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetBrightness(value.turn_on ? kDefaultBrightness : 0);
    }
  };

  class ColorRotationDemo final : public TimeoutState {
   public:
    ColorRotationDemo(StateManager& manager)
        : TimeoutState(manager, "ColorRotationDemo", kDemoModeTimeout) {}

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }

    void ColorRotationModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  // Respond to a PubSub event.
  void Update(Event event);

  void HandleButtonPress(bool pressed, void (State::* function)());

  template <typename StateType>
  void SetState() {
    demo_mode_timer_.Cancel();  // always reset the timer
    const char* old_state = state_.get().name();
    state_.emplace<StateType>(*this);
    LogStateChange(old_state);
  }

  void LogStateChange(const char* old_state) const;

  void StartMorseReadout(bool repeat);

  void DisplayThreshold();

  void IncrementThreshold(pw::chrono::SystemClock::duration timeout);

  void DecrementThreshold(pw::chrono::SystemClock::duration timeout);

  PubSub* pubsub_;
  LedOutputStateMachine led_;

  CommonBaseUnion<State,
                  AirQualityMode,
                  AirQualityThresholdMode,
                  AirQualityAlarmMode,
                  MorseReadout,
                  ProximityDemo,
                  MorseCodeDemo,
                  ColorRotationDemo>
      state_;

  pw::chrono::SystemTimer demo_mode_timer_;

  bool alarmed_ = false;
  uint16_t current_threshold_ = AirSensor::kDefaultTheshold;
  uint16_t last_air_quality_score_ = AirSensor::kAverageScore;
  pw::InlineString<4> air_quality_score_string_;
};

}  // namespace sense
