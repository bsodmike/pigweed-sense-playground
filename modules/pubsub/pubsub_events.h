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

#include <variant>

#include "modules/pubsub/pubsub.h"

namespace sense {

// VOC / CO2 crossed over the threshold.
struct AlarmStateChange {
  bool alarm;
};

// Base for button state changes.
class ButtonStateChange {
 public:
  explicit constexpr ButtonStateChange(bool is_pressed)
      : pressed_(is_pressed) {}

  bool pressed() const { return pressed_; }

 private:
  bool pressed_;
};

// Button state changes for specific buttons.
class ButtonA : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonB : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonX : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};
class ButtonY : public ButtonStateChange {
 public:
  using ButtonStateChange::ButtonStateChange;
};

/// Proximity sensor state change.
struct ProximityStateChange {
  bool proximity;
};

/// New proximity sample.
struct ProximitySample {
  /// Unspecified proximity units where 0 is the minimum (farthest) and 65535 is
  /// the maximum (nearest) value reported by the sensor.
  uint16_t sample;
};

/// Air quality score that combines relative humidity and gas resistance values.
struct AirQuality {
  /// 10 bit value ranging from 0 (very poor) to 1023 (excellent).
  uint16_t score;
};

class LedValue {
 public:
  explicit constexpr LedValue(uint8_t r, uint8_t g, uint8_t b)
      : r_(r), g_(g), b_(b) {}
  explicit constexpr LedValue() : r_(0), g_(0), b_(0) {}

  constexpr uint8_t r() const { return r_; }
  constexpr uint8_t g() const { return g_; }
  constexpr uint8_t b() const { return b_; }

 private:
  uint8_t r_;
  uint8_t g_;
  uint8_t b_;
};

class LedValueColorRotationMode : public LedValue {
 public:
  using LedValue::LedValue;
  explicit LedValueColorRotationMode(const LedValue& parent)
      : LedValue(parent) {}
};
class LedValueMorseCodeMode : public LedValue {
 public:
  explicit LedValueMorseCodeMode(const LedValue& parent, bool pattern_finished)
      : LedValue(parent), pattern_finished_(pattern_finished) {}

  /// True if this LED color update is the final for the encoded phrase.
  [[nodiscard]] bool pattern_finished() const { return pattern_finished_; }

 private:
  bool pattern_finished_;
};
class LedValueProximityMode : public LedValue {
 public:
  using LedValue::LedValue;
  explicit LedValueProximityMode(const LedValue& parent) : LedValue(parent) {}
};
class LedValueAirQualityMode : public LedValue {
 public:
  using LedValue::LedValue;
  explicit LedValueAirQualityMode(const LedValue& parent) : LedValue(parent) {}
};

// This definition must be kept up to date with modules/pubsub/pubsub.proto.
using Event = std::variant<AlarmStateChange,
                           ButtonA,
                           ButtonB,
                           ButtonX,
                           ButtonY,
                           LedValueColorRotationMode,
                           LedValueMorseCodeMode,
                           LedValueProximityMode,
                           LedValueAirQualityMode,
                           ProximityStateChange,
                           ProximitySample,
                           AirQuality>;

// PubSub using Sense events.
using PubSub = GenericPubSub<Event>;

}  // namespace sense
