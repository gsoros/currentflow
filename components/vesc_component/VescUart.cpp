/*

* VescUart.cpp
*
* Non-blocking UART communication implementation for VESC devices,
* adapted for ESPHome.
*
* Based on the VescUart project:
* https://github.com/SolidGeek/VescUart
*
* Portions of the protocol structures and message handling logic
* ultimately originate from the VESC firmware project:
* https://github.com/vedderb/bldc
*
* Original authors:
* Benjamin Vedder (VESC firmware)
* SolidGeek (VescUart library)
*
* This version refactors the original blocking implementation into a
* cooperative, non-blocking state machine suitable for ESPHome's runtime
* environment.
*
* Licensed under the GNU General Public License v3.0 (GPL-3.0).
  */

#include "VescUart.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>

VescUart::VescUart(uint32_t timeout_ms) : _timeout_ms(timeout_ms) {
  // initialize parser state
  state = WAIT_START;
  rx_len = 0;
  expected_total_len = 0;
  expected_payload_len = 0;
}

VescUart::~VescUart() {}

void VescUart::setSerialPort(Stream *port) { serialPort = port; }
void VescUart::setDebugPort(Stream *port) { debugPort = port; }

int VescUart::processIncoming() {
  if (serialPort == NULL)
    return -1;
  // Read and parse bytes one at a time using parseByte(). This keeps a small
  // stateful parser and lets callers feed bytes either from a Stream or from
  // an external loop.
  if (serialPort->available()) {
    // ESP_LOGD("VescUart", "Processing incoming data from serial port...");
  }
  while (serialPort->available()) {
    int b = serialPort->read();
    int res = parseByte((uint8_t) b);
    if (res > 0) {
      return res;
    }
  }
  return 0;
}

int VescUart::parseByte(uint8_t b) {
  // 1. Basic Overflow Guard
  if (rx_len >= VESC_MAX_PACKET_SIZE) {
    popByte();  // Drop oldest if full to keep searching
  }

  // 2. Add to buffer
  rx_buffer[rx_head] = b;
  rx_head = (rx_head + 1) % VESC_MAX_PACKET_SIZE;
  rx_len++;

  // 3. Process State Machine
  return processByte();
};

int VescUart::processByte() {
  switch (state) {
    case WAIT_START:
      if (peekByte(0) == VESC_SHORT_START) {
        // ESP_LOGD("VescUart", "WAIT_START: Detected short packet start");
        state = READ_LENGTH;
      } else if (peekByte(0) == VESC_LONG_START) {
        // ESP_LOGD("VescUart", "WAIT_START: Detected long packet start");
        state = READ_EXT_LENGTH;
      } else {
        // ESP_LOGD("VescUart", "WAIT_START: Not a start byte, discarding");
        popByte();  // Not a start byte, discard
      }
      break;

    case READ_LENGTH:
      if (rx_len >= 2) {
        // Length + Start(1) + Len(1) + CRC(2) + Stop(1) = +5
        expected_payload_len = (size_t) peekByte(1);
        expected_total_len = expected_payload_len + 5;
        // ESP_LOGD("VescUart", "READ_LENGTH: Detected short packet with payload length %d, expecting total length %d",
        // peekByte(1), expected_total_len);
        validateAndMoveToPayload();
      }
      break;

    case READ_EXT_LENGTH:
      if (rx_len >= 3) {
        // (MSB << 8 | LSB) + Start(1) + Len(2) + CRC(2) + Stop(1) = +6
        expected_payload_len = (size_t) ((uint16_t) peekByte(1) << 8) | (uint16_t) peekByte(2);
        expected_total_len = expected_payload_len + 6;
        ESP_LOGD("VescUart", "READ_EXT_LENGTH: Detected long packet with payload length %d, expecting total length %d",
                 expected_payload_len, expected_total_len);
        validateAndMoveToPayload();
      }
      break;

    case READ_PAYLOAD:
      if (rx_len >= expected_total_len) {
        if (peekByte(expected_total_len - 1) == VESC_STOP_BYTE) {
          return finalizePacket();
        } else {
          // Malformed: the "Stop" isn't 0x03. Drop start and retry.
          popByte();
          state = WAIT_START;
        }
      }
      break;
  }
  return 0;
}

uint8_t VescUart::peekByte(size_t index) {
  // Safely look into circular buffer relative to the "start" of current search
  size_t start_idx = (rx_head + VESC_MAX_PACKET_SIZE - rx_len) % VESC_MAX_PACKET_SIZE;
  return rx_buffer[(start_idx + index) % VESC_MAX_PACKET_SIZE];
}

void VescUart::popByte() {
  if (rx_len > 0)
    rx_len--;
}

void VescUart::validateAndMoveToPayload() {
  if (expected_total_len > VESC_MAX_PACKET_SIZE || expected_total_len < 6) {
    popByte();  // Invalid length logic, drop the 0x02/0x03 and reset
    state = WAIT_START;
  } else {
    state = READ_PAYLOAD;
  }
}

int VescUart::finalizePacket() {
  // Copy circular buffer to flat array for CRC check and Unpacking
  uint8_t flat_packet[VESC_MAX_PACKET_SIZE];
  for (size_t i = 0; i < expected_total_len; i++) {
    flat_packet[i] = peekByte(i);
  }

  uint8_t payload[VESC_MAX_PAYLOAD_SIZE];
  bool ok = unpackPayload(flat_packet, (int) expected_total_len, payload);
  if (ok) {
    // ESP_LOGD("VescUart", "READ_PAYLOAD: Payload unpacked successfully, processing...");
    processReadPayload(payload, expected_payload_len);

    // Remove the processed packet from circular buffer
    // TODO: Why not just reset rx_len instead of iterating and popping? We know exactly how many bytes we processed, so
    // we can just reduce the length accordingly. The current popByte() in a loop is O(n) in the packet size, while we
    // could do this in O(1).
    for (size_t i = 0; i < expected_total_len; i++)
      popByte();

    // TODO: reset_parser() here?
    int ret = expected_payload_len;

    // TODO should we return the actual payload length instead of the expected? In theory
    // they should be the same if everything is correct, but if we want to be extra safe we
    // could return the length we got from the packet instead of what we expected based on
    // the header. This would also allow us to detect cases where the header length was wrong
    // but the CRC still matched (e.g. if there was an extra byte in the payload that got
    // ignored by the header length but included in the CRC).

    expected_payload_len = 0;
    expected_total_len = 0;
    state = WAIT_START;
    return ret;
  }

  // CRC failed: drop first byte and reset
  ESP_LOGD("VescUart", "READ_PAYLOAD: CRC failed, dropping byte and retrying");
  popByte();
  state = WAIT_START;
  expected_total_len = 0;
  expected_payload_len = 0;
  return 0;
}

void VescUart::reset_parser() {
  state = WAIT_START;
  expected_total_len = 0;
  rx_len = 0;

  ESP_LOGD("VescUart", "Parser reset");
  if (debugPort != NULL) {
    debugPort->println("Parser reset");
  }
}

void VescUart::requestValues(uint8_t canId) {
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 1 : 3);
  uint8_t payload[8];
  if (canId != 0) {
    payload[index++] = (uint8_t) COMM_FORWARD_CAN;
    payload[index++] = canId;
  }
  payload[index++] = (uint8_t) COMM_GET_VALUES;
  packSendPayload(payload, payloadSize);
}

// TODO Add support for long packet format.
// unpackPayload assumes short packet format — it hardcodes message[1]
// as the length byte and &message[2] as payload start, which only works
// for short packets (start byte 0x02). Long packets (0x03) have a 2-byte
// length field, so this will silently misparse them
bool VescUart::unpackPayload(uint8_t *message, int len, uint8_t *payload) {
  // ESP_LOGD("VescUart", "Unpacking payload from message of length %d", len);
  // logPacket(message, (size_t)len);

  // COMM_PACKET_ID id = (COMM_PACKET_ID)message[0];  // First byte of payload is the packet ID, used for dispatching
  // after CRC check

  uint16_t crcMessage = 0;
  uint16_t crcPayload = 0;

  // CRC is big-endian in the message: first byte is MSB, second byte is LSB
  crcMessage = message[len - 3] << 8;

  // Mask to ensure we only have the upper byte (in case of sign extension issues)
  crcMessage &= 0xFF00;

  // Add the LSB to complete the CRC from the message
  crcMessage += message[len - 2];

  // Extract the payload from the message for CRC calculation and processing
  memcpy(payload, &message[2], message[1]);

  // Calculate CRC of the extracted payload
  crcPayload = crc16(payload, message[1]);

  // ESP_LOGD("VescUart", "Calculated CRC: 0x%04X, Message CRC: 0x%04X", crcPayload, crcMessage);

  if (crcPayload == crcMessage) {
    // ESP_LOGD("VescUart", "Received valid message with payload length %d", len);
    if (debugPort != NULL) {
      debugPort->print("Received: ");
      serialPrint(message, len);
      debugPort->println();
    }
    return true;
  }
  return false;
}

int VescUart::packSendPayload(uint8_t *payload, int len) {
  uint16_t crcPayload = crc16(payload, len);
  int count = 0;
  uint8_t packet[256];

  if (len <= 256) {
    packet[count++] = 2;
    packet[count++] = len;
  } else {
    packet[count++] = 3;
    packet[count++] = (uint8_t) (len >> 8);
    packet[count++] = (uint8_t) (len & 0xFF);
  }

  memcpy(packet + count, payload, len);
  count += len;

  packet[count++] = (uint8_t) (crcPayload >> 8);
  packet[count++] = (uint8_t) (crcPayload & 0xFF);
  packet[count++] = 3;

  // ESP_LOGD("VescUart", "Sending message with payload length %d", count);
  // logPacket(packet, (size_t)count);

  if (serialPort != NULL)
    serialPort->write(packet, count);
  return count;
}

bool VescUart::processReadPayload(uint8_t *payload, size_t len) {
  int32_t index = 0;

  COMM_PACKET_ID id = (COMM_PACKET_ID) payload[0];
  payload++;  // Move past ID for easier parsing of the rest of the payload

  // ESP_LOGD("VescUart", "Payload without ID:");
  // logPacket(payload, len - 1);

  switch (id) {
    case COMM_GET_VALUES:
      // ESP_LOGD("VescUart", "Processing COMM_GET_VALUES payload, index: %d", index);
      data.tempMosfet = buffer_get_float16(payload, 10.0, &index);
      // ESP_LOGD("VescUart", "Parsed tempMosfet: %f, next index: %d", data.tempMosfet, index);
      data.tempMotor = buffer_get_float16(payload, 10.0, &index);
      // ESP_LOGD("VescUart", "Parsed tempMotor: %f, next index: %d", data.tempMotor, index);
      data.avgMotorCurrent = buffer_get_float32(payload, 100.0, &index);
      data.avgInputCurrent = buffer_get_float32(payload, 100.0, &index);
      index += 4;  // skip id
      index += 4;
      data.dutyCycleNow = buffer_get_float16(payload, 1000.0, &index);
      data.rpm = buffer_get_float32(payload, 1.0, &index);
      data.inpVoltage = buffer_get_float16(payload, 10.0, &index);
      data.ampHours = buffer_get_float32(payload, 10000.0, &index);
      data.ampHoursCharged = buffer_get_float32(payload, 10000.0, &index);
      data.wattHours = buffer_get_float32(payload, 10000.0, &index);
      data.wattHoursCharged = buffer_get_float32(payload, 10000.0, &index);
      data.tachometer = buffer_get_int32(payload, &index);
      data.tachometerAbs = buffer_get_int32(payload, &index);
      data.error = (mc_fault_code) payload[index++];
      data.pidPos = buffer_get_float32(payload, 1000000.0, &index);
      data.id = payload[index++];
      return true;

    case COMM_LISP_PRINT:  // Lisp / Terminal Print
      // Note: payload here already points past the ID byte (the payload++
      // at the top of processReadPayload took care of that), and len is
      // expected_payload_len which still includes the ID. So len - 1 is the
      // actual string length.
      if (len > 1) {  // len includes the ID byte already consumed
        strncpy(data.lispPrint, reinterpret_cast<char *>(payload), len - 1);
        ESP_LOGI("vesc_lisp", "%s", data.lispPrint);
      }
      return true;

    default:
      ESP_LOGD("VescUart", "Received unhandled packet with ID %d, payload:", id);
      logPacket(payload, len);
      if (debugPort != NULL) {
        debugPort->print("Received unhandled packet with ID ");
        debugPort->print((int) id);
        debugPort->println();
      }
      return false;
  }
}

void VescUart::setRPM(float rpm, uint8_t canId) {
  // ESP_LOGD("VescUart", "VescUart::setRPM called with rpm: %.0f, canId: %d", rpm, canId);
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 5 : 7);
  uint8_t payload[payloadSize];
  if (canId != 0) {
    payload[index++] = {COMM_FORWARD_CAN};
    payload[index++] = canId;
  }
  payload[index++] = {COMM_SET_RPM};
  buffer_append_int32(payload, (int32_t) (rpm), &index);
  packSendPayload(payload, payloadSize);
}

void VescUart::setDuty(float duty, uint8_t canId) {
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 5 : 7);
  uint8_t payload[payloadSize];
  if (canId != 0) {
    payload[index++] = {COMM_FORWARD_CAN};
    payload[index++] = canId;
  }
  payload[index++] = {COMM_SET_DUTY};
  buffer_append_int32(payload, (int32_t) (duty * 100000), &index);
  packSendPayload(payload, payloadSize);
}

void VescUart::setCurrent(float current, uint8_t canId) {
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 5 : 7);
  uint8_t payload[payloadSize];
  if (canId != 0) {
    payload[index++] = {COMM_FORWARD_CAN};
    payload[index++] = canId;
  }
  payload[index++] = {COMM_SET_CURRENT};
  buffer_append_int32(payload, (int32_t) (current * 1000), &index);
  packSendPayload(payload, payloadSize);
}

void VescUart::sendKeepalive(uint8_t canId) {
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 1 : 3);
  uint8_t payload[payloadSize];
  if (canId != 0) {
    payload[index++] = {COMM_FORWARD_CAN};
    payload[index++] = canId;
  }
  payload[index++] = {COMM_ALIVE};
  packSendPayload(payload, payloadSize);
}

void VescUart::sendReboot(uint8_t canId) {
  int32_t index = 0;
  int payloadSize = (canId == 0 ? 1 : 3);
  uint8_t payload[payloadSize];
  if (canId != 0) {
    payload[index++] = {COMM_FORWARD_CAN};
    payload[index++] = canId;
  }
  payload[index++] = {COMM_REBOOT};
  packSendPayload(payload, payloadSize);
}

void VescUart::serialPrint(uint8_t *data, int len) {
  if (debugPort != NULL) {
    for (int i = 0; i <= len; i++) {
      debugPort->print(data[i]);
      debugPort->print(" ");
    }
    debugPort->println("");
  }
}

void VescUart::logPacket(uint8_t *buf, size_t len) {
  // ESP_LOG_BUFFER_HEX("vesc_packet", buf, len);
  // char hex_str[len * 3 + 1]; // VLA
  char hex_str[VESC_MAX_PACKET_SIZE * 3 + 1];
  for (size_t i = 0; i < len; i++) {
    sprintf(&hex_str[i * 3], "%02X ", buf[i]);
  }
  ESP_LOGD("vesc_packet", "Data: %s", hex_str);
}
