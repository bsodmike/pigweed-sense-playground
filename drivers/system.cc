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

#include "drivers/system.h"

#include "hardware/adc.h"
#include "pico/bootrom.h"

namespace am {
namespace {

void SystemInit() {
  static bool initialized = false;
  if (!initialized) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    SystemSetLed(false);

    adc_init();

    initialized = true;
  }
}

}  // namespace

void SystemSetLed(bool enable) {
  SystemInit();
  gpio_put(PICO_DEFAULT_LED_PIN, enable ? 0 : 1);
}

float SystemReadTemp() {
  SystemInit();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);  // 4 is the on board temp sensor.

  // See raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
  constexpr float kConversionFactor = 3.3f / (1 << 12);
  float adc = static_cast<float>(adc_read()) * kConversionFactor;
  return 27.0f - (adc - 0.706f) / 0.001721f;
}

void SystemReboot(uint8_t reboot_types) {
  SystemInit();
  bool mass_storage =
      reboot_types & static_cast<uint8_t>(RebootType::kMassStorage);
  bool picoboot = reboot_types & static_cast<uint8_t>(RebootType::kPicoboot);
  if (mass_storage && picoboot) {
    reset_usb_boot(0, 0);
  } else if (picoboot) {
    reset_usb_boot(0, 1);
  } else if (mass_storage) {
    reset_usb_boot(0, 2);
  }
}

}  // namespace am
