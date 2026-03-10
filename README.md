# ESPHome VESC Telemetry and Control Component

Custom ESPHome component for monitoring telemetry and sending commands to a VESC over UART.

The component communicates with a VESC motor controller using the VESC UART protocol, provides controls and exposes telemetry to Home Assistant via ESPHome sensors.

The implementation is designed to be **non-blocking** and compatible with ESPHome’s cooperative runtime model. An optional BLE UART bridge is also available for connecting the Vesc Tool app.

## Features

* UART communication with VESC controllers
* Non-blocking protocol handling
* Telemetry exposed as ESPHome sensors
* Suitable for Home Assistant integration
* Lightweight and ESP32-friendly
* Optional BLE UART bridge

Example telemetry values that can be exposed:

* Input voltage
* Motor current
* RPM
* Duty cycle
* FET Temperature

## Example ESPHome Configuration

*TODO provide a working example*

```yaml
external_components:
  - source: github://gsoros/currentflow

uart:
  id: vesc_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

vesc:
  uart_id: vesc_uart
  update_interval: 1s
```

(Exact configuration may vary depending on the sensors you expose.)

## Design Notes

The original `VescUart` Arduino library performs blocking UART reads.
This component reworks the protocol logic into a **non-blocking state machine** suitable for ESPHome’s `loop()` execution model.

This avoids watchdog issues and allows ESPHome to continue servicing Wi-Fi, BLE, Home Assistant communication, and other components.

## Third-Party Code and Attribution

This project incorporates concepts, structures, and portions of code derived from the following open-source projects.

### VESC Firmware

Some data structures, constants, and protocol definitions originate from:

VESC BLDC Firmware
https://github.com/vedderb/bldc

Copyright (c) Benjamin Vedder

These elements were adapted for use in this ESPHome component and stripped of firmware-specific dependencies.

### VescUart Library

The UART communication logic was originally based on the VescUart Arduino library:

https://github.com/SolidGeek/VescUart

Copyright (c) SolidGeek

The implementation in this project has been **significantly modified**, including:

* Conversion to a non-blocking architecture
* Refactoring for ESPHome component structure
* Removal of Arduino-specific assumptions
* Integration with ESPHome UART handling

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).

Portions of this code are derived from:

- VESC BLDC Firmware — https://github.com/vedderb/bldc
  Copyright (c) Benjamin Vedder

- VescUart Library — https://github.com/SolidGeek/VescUart
  Copyright (c) SolidGeek

Modifications include refactoring for ESPHome and conversion to a non-blocking
implementation compatible with ESPHome’s execution model.

See the LICENSE file for full details.

## Acknowledgements

* Benjamin Vedder for the VESC firmware and protocol design
* SolidGeek for the original VescUart library
