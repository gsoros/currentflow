# ESPHome VESC Refactor Plan

Goal
----
Extract a minimal, well-tested, non-blocking VESC UART driver and implement a generic ESPHome component that is configurable, documented, and ready for publishing.

High-level milestones
---------------------
- 1. Scaffold repository and include original license/attribution (keep NOTICE/README).
- 2. Extract minimal UART parsing, CRC and ring-buffer logic from `VescUart` and rework to be non-blocking.
- 3. Implement an ESPHome component with configurable options: RX/TX pins, baud rate, timeout, hardware UART selection, and ID.
- 4. Provide runtime APIs: `set_rpm`, `set_duty`, `send_keepalive`, sensors for telemetry, and runtime log-level controls.
- 5. Add an optional BLE UART bridge (ESP32) to proxy VESC Tool connections to the physical UART.
- 6. Add CI: build checks for ESPHome examples and unit tests for parser/CRC (host-buildable).
- 7. Write docs, examples, wiring diagrams, and publish to GitHub with clear license and attribution.

Design notes
------------
- Keep the driver non-blocking: use a small state machine to parse frames, validate CRC, and extract data. Avoid long `delay()` or blocking reads.
- Expose configuration via YAML with reasonable defaults; validate/clamp unsafe inputs (e.g., RPM/duty limits).
- Use `Stream*` for serial abstraction so code works with `Serial`, `Serial2`, USB CDC, etc.
- For BLE bridge, implement a byte-forwarding queue and handle concurrency between BLE callbacks and the main loop.

YAML example (sketch)
----------------------
```yaml
vesc_uart:
  id: vesc0
  rx_pin: 16
  tx_pin: 17
  baud_rate: 115200
  timeout_ms: 30
  hardware_uart: UART2
```

Testing & CI
------------
- Add unit tests for CRC and parser (host-buildable C++ tests).
- Add GitHub Actions to build example ESPHome configs (or at least run `esphome compile`).

Licensing & attribution
-----------------------
- Preserve original license headers for any copied code. Add a `NOTICE` file summarizing third-party attributions and license text.

Next immediate steps (pick one)
------------------------------
1. I can scaffold the component skeleton and YAML schema.
2. I can extract and refactor the parser/CRC/buffer into a small, testable module (recommended first dev task).

Please tell me which of the two to start with and I will create the initial files and a PR-sized change.

--
Plan created on 2026-03-04
