Goals
----
Extract a minimal, well-tested, non-blocking VESC UART driver and implement a generic ESPHome Vesc component that is configurable and documented

High-level milestones
---------------------
- Scaffold repository and include original license/attribution (keep NOTICE/README).
- Extract minimal UART parsing, CRC and ring-buffer logic from `VescUart` and rework to be non-blocking.
- Implement an ESPHome component with configurable options: RX/TX pins, baud rate, timeout, hardware UART selection, and ID.
- Provide runtime APIs: `set_rpm`, `set_duty`, `send_keepalive`, sensors for telemetry, and runtime log-level controls.
- Add an optional BLE UART bridge (ESP32) to proxy VESC Tool connections to the physical UART.
- Add CI: build checks for ESPHome examples and unit tests for parser/CRC (host-buildable).
- Write docs, examples, wiring diagrams, and publish to GitHub with clear license and attribution.
- Consider using esp-idf instead of arduino as the framework
- Handle BLE bonding/security to prevent random people from connecting to the VESC
- Expose the fan-specific controls (like specific RPM presets) as standard ESPHome Fan components
- Wattage sensor

Design notes
------------
- Keep the driver non-blocking: use a small state machine to parse frames, validate CRC, and extract data. Avoid long `delay()` or blocking reads.
- Expose configuration via YAML with reasonable defaults; validate/clamp unsafe inputs (e.g., RPM/duty limits).
- Use `Stream*` for serial abstraction so code works with `Serial`, `Serial2`, USB CDC, etc.
- For BLE bridge, implement a byte-forwarding queue and handle concurrency between BLE callbacks and the main loop.
- In docs and comments, don't miss any opportunity to warn users about the consequences of incorrect settings


Testing & CI
------------
- Add unit tests for CRC and parser (host-buildable C++ tests).
- Add GitHub Actions to build example ESPHome configs (or at least run `esphome compile`).

Licensing & attribution
-----------------------
- Preserve original license headers for any copied code.
