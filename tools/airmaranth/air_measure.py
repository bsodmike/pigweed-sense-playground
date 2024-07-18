# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Get air sensor measurements."""

import logging

from airmaranth.device import get_device_connection

_LOG = logging.getLogger(__file__)


def main():
    device_connection = get_device_connection()

    # Open the connection to the device.
    with device_connection as device:
        air_sensor = device.rpcs.air_sensor.AirSensor

        _LOG.info('Calling AirSensor.Measure()')
        status, response = air_sensor.Measure()
        if not status.ok():
            _LOG.error('Calling AirSensor.Measure()')
        _LOG.info('AirSensor.Measure() response:')
        for line in str(response).splitlines():
            _LOG.info('  %s', line)


if __name__ == '__main__':
    main()
