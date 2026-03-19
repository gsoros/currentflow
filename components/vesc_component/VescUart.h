/*

* VescUart.h
*
* Interface for a non-blocking VESC UART communication implementation
* adapted for ESPHome.
*
* This implementation is based on concepts and portions of code from the
* VescUart library:
* https://github.com/SolidGeek/VescUart
*
* Original work:
* VescUart Library
* Copyright (c) SolidGeek
*
* The code has been significantly modified to operate in a non-blocking
* manner and to integrate with the ESPHome component architecture.
*
* This file is distributed under the terms of the GNU General Public License
* v3.0 (GPL-3.0).
  */

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "helpers.h"
#include "esphome/core/log.h"

// Define VESC constants
#define VESC_MAX_PACKET_SIZE 512
#define VESC_MAX_PAYLOAD_SIZE (VESC_MAX_PACKET_SIZE - 6)  // Max payload considering header and CRC
#define VESC_SHORT_START 0x02
#define VESC_LONG_START 0x03
#define VESC_STOP_BYTE 0x03

class VescUart {
 public:
  VescUart(uint32_t timeout_ms = 100);

  struct dataPackage {
    float avgMotorCurrent;
    float avgInputCurrent;
    float dutyCycleNow;
    float speed;
    float inpVoltage;
    float ampHours;
    float ampHoursCharged;
    float wattHours;
    float wattHoursCharged;
    long tachometer;
    long tachometerAbs;
    float tempMosfet;
    float tempMotor;
    float pidPos;
    uint8_t id;
    mc_fault_code error;
    char lispPrint[64];  // Last Lisp/terminal print output
  };

  void setSerialPort(Stream *port);
  void setDebugPort(Stream *port);
  void setSpeed(float erpm, uint8_t canId = 0);
  void setDuty(float duty, uint8_t canId = 0);
  void setCurrent(float current, uint8_t canId = 0);
  void sendKeepalive(uint8_t canId = 0);
  void sendReboot(uint8_t canId = 0);
  // Send a values request without blocking for the response.
  void requestValues(uint8_t canId = 0);
  // bool getFWVersion(uint8_t canId, char* versionBuffer, size_t bufferSize);
  //  Non-blocking processor: call regularly from the main loop to feed
  //  incoming bytes and detect full messages. Returns payload length
  //  when a complete message is parsed, 0 if no complete message yet,
  // and -1 on error (e.g. no serial port).
  int processIncoming();
  // Feed one byte into the parser. Returns payload length when a
  // complete packet is received and processed, 0 otherwise.
  int parseByte(uint8_t b);

  // Configuration setters
  void set_timeout_ms(uint32_t t) { _timeout_ms = t; }

  // Helper to reset the parser state and buffer (e.g. on overflow or desync).
  void reset_parser();

  ~VescUart();

  dataPackage data;

 private:
  Stream *serialPort = nullptr;
  Stream *debugPort = nullptr;
  uint32_t _timeout_ms;

  // RX buffer for non-blocking parsing (circular buffer)
  uint8_t rx_buffer[VESC_MAX_PACKET_SIZE];
  size_t rx_head = 0;  // Current write position
  size_t rx_len = 0;   // Number of bytes currently in buffer

  // Explicit parser state machine
  enum ParserState { WAIT_START, READ_LENGTH, READ_EXT_LENGTH, READ_PAYLOAD };
  ParserState state = WAIT_START;
  size_t expected_total_len = 0;
  size_t expected_payload_len = 0;

  // Internal helper to process the current buffer state after adding a byte.
  // returns payload length if a full packet is processed, 0 otherwise.
  int processByte();
  // Helper to peek into the circular buffer without modifying state.
  uint8_t peekByte(size_t index);
  // Helper to pop one byte from the buffer (after processing or on error).
  void popByte();
  // Helper to validate length and move to payload reading state, or reset on error.
  void validateAndMoveToPayload();
  // Helper to finalize a packet after full payload is received, including CRC check and unpacking.
  // Returns payload length if successful, 0 if CRC failed (in which case the byte is dropped and parser reset).
  int finalizePacket();

  // Helper to unpack payload from a full message after validating CRC. Returns true if successful.
  // payload buffer should be pre-allocated by caller with size at least VESC_MAX_PAYLOAD_SIZE.
  bool unpackPayload(uint8_t *message, int lenMes, uint8_t *payload);

  int packSendPayload(uint8_t *payload, int len);
  bool processReadPayload(uint8_t *payload, size_t len);
  void serialPrint(uint8_t *data, int len);
  void logPacket(uint8_t *buf, size_t len);
};
