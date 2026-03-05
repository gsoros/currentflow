/* VescUart.cpp -- renamed from refactored_VescUart.cpp */

#include "VescUart.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>

VescUart::VescUart(uint32_t timeout_ms) : _TIMEOUT(timeout_ms) {
    nunchuck.valueX = 127;
    nunchuck.valueY = 127;
    nunchuck.lowerButton = false;
    nunchuck.upperButton = false;
    // initialize parser state
    parser_state = WAIT_START;
    rx_len = 0;
    expected_len = 0;
    // default buffer
    rx_capacity = 512;
    rx_buffer = (uint8_t*)malloc(rx_capacity);
    if (rx_buffer == nullptr) rx_capacity = 0;
}

VescUart::~VescUart() {
    if (rx_buffer) free(rx_buffer);
    rx_buffer = nullptr;
    rx_capacity = 0;
}

void VescUart::setSerialPort(Stream* port) { serialPort = port; }
void VescUart::setDebugPort(Stream* port) { debugPort = port; }

int VescUart::processIncoming() {
    if (serialPort == NULL)
        return -1;
    // Read and parse bytes one at a time using parseByte(). This keeps a small
    // stateful parser and lets callers feed bytes either from a Stream or from
    // an external loop.
    while (serialPort->available()) {
        int b = serialPort->read();
        int res = parseByte((uint8_t)b);
        if (res > 0) {
            return res;
        }
    }
    return 0;
}

int VescUart::parseByte(uint8_t b) {
    // Append byte with overflow handling
    if (rx_capacity == 0) return 0;
    if (rx_len < rx_capacity) {
        rx_buffer[rx_len++] = b;
    } else {
        if (debugPort != NULL) debugPort->println("RX buffer overflow");
        memmove(rx_buffer, rx_buffer + 1, rx_capacity - 1);
        rx_buffer[rx_capacity - 1] = b;
        rx_len = rx_capacity;
    }

    switch (parser_state) {
        case WAIT_START:
            if (b == 2) {
                parser_state = READ_LENGTH;
                expected_len = 0;
            } else if (b == 3) {
                parser_state = READ_EXT_LENGTH;
                expected_len = 0;
            } else {
                // not a start byte, drop it
                if (rx_len > 0) {
                    memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
                    rx_len--;
                }
            }
            return 0;

        case READ_LENGTH:
            if (rx_len < 2) return 0;  // need start-byte + length-byte
            expected_len = (size_t)rx_buffer[1] + 5;
            if (expected_len > rx_capacity) {
                if (debugPort != NULL) debugPort->println("Message > buffer size");
                // reset parser
                memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
                rx_len--;
                parser_state = WAIT_START;
                expected_len = 0;
                return 0;
            }
            parser_state = READ_PAYLOAD;
            return 0;

        case READ_EXT_LENGTH:
            if (rx_len < 3) return 0;  // need start + two length bytes
            expected_len = ((size_t)rx_buffer[1] << 8) + (size_t)rx_buffer[2] + 5;
            if (expected_len > rx_capacity) {
                if (debugPort != NULL) debugPort->println("Message > buffer size");
                memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
                rx_len--;
                parser_state = WAIT_START;
                expected_len = 0;
                return 0;
            }
            parser_state = READ_PAYLOAD;
            return 0;

        case READ_PAYLOAD:
            if (expected_len == 0) return 0;
            if (rx_len < expected_len) return 0;  // wait

            // Full message available
            if (rx_buffer[expected_len - 1] != 3) {
                // malformed end byte; drop first byte and reset
                memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
                rx_len--;
                parser_state = WAIT_START;
                expected_len = 0;
                return 0;
            }

            // Copy into temporary buffer for validation
            uint8_t message[256];
            if (expected_len > sizeof(message)) {
                // unsupported size; drop start
                memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
                rx_len--;
                parser_state = WAIT_START;
                expected_len = 0;
                return 0;
            }
            memcpy(message, rx_buffer, expected_len);

            uint8_t payload[256];
            bool ok = unpackPayload(message, (int)expected_len, payload);
            if (ok) {
                processReadPacket(payload);
                uint16_t lenPayload = message[1];
                // remove processed bytes
                size_t remaining = rx_len - expected_len;
                if (remaining > 0) memmove(rx_buffer, rx_buffer + expected_len, remaining);
                rx_len = remaining;
                parser_state = WAIT_START;
                expected_len = 0;
                return (int)lenPayload;
            }

            // CRC failed: drop first byte and reset
            memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
            rx_len--;
            parser_state = WAIT_START;
            expected_len = 0;
            return 0;
    }

    return 0;
}

/*
int VescUart::parseByte(uint8_t b) {
    switch (parser_state) {
        case WAIT_START:
            return handleWaitStart(b);
        case READ_LENGTH:
            return handleReadLength(b);
        case READ_PAYLOAD:
            return handleReadPayload(b);
        default:
            return 0;
    }
}

int VescUart::handleWaitStart(uint8_t b) {
    if (b == 2) {
        parser_state = READ_LENGTH;
        expected_len = 0;
    } else if (b == 3) {
        parser_state = READ_EXT_LENGTH;
        expected_len = 0;
    } else {
        // not a start byte, drop it
        if (rx_len > 0) {
            memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
            rx_len--;
        }
    }
    return 0;
}

int VescUart::handleReadLength(uint8_t b) {
    if (rx_len >= 2) {
        expected_len = (size_t)rx_buffer[1] + 5;
        if (expected_len > rx_capacity) {
            if (debugPort != NULL) debugPort->println("Message > buffer size");
            // reset parser
            memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
            rx_len--;
            parser_state = WAIT_START;
            expected_len = 0;
            return 0;
        }
        parser_state = READ_PAYLOAD;
    }
    return 0;
}

int VescUart::handleReadPayload(uint8_t b) {
    if (expected_len == 0) return 0;
    if (rx_len < expected_len) return 0;  // wait

    // Full message available
    if (rx_buffer[expected_len - 1] != 3) {
        // malformed end byte; drop first byte and reset
        memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
        rx_len--;
        parser_state = WAIT_START;
        expected_len = 0;
        return 0;
    }

    // Copy into temporary buffer for validation
    uint8_t message[256];
    if (expected_len > sizeof(message)) {
        // unsupported size; drop start
        memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
        rx_len--;
        parser_state = WAIT_START;
        expected_len = 0;
        return 0;
    }
    memcpy(message, rx_buffer, expected_len);

    uint8_t payload[256];
    bool ok = unpackPayload(message, (int)expected_len, payload);
    if (ok) {
        processReadPacket(payload);
        uint16_t lenPayload = message[1];
        // remove processed bytes
        size_t remaining = rx_len - expected_len;
        if (remaining > 0) memmove(rx_buffer, rx_buffer + expected_len, remaining);
        rx_len = remaining;
        parser_state = WAIT_START;
        expected_len = 0;
        return (int)lenPayload;
    }

    // CRC failed: drop first byte and reset
    memmove(rx_buffer, rx_buffer + 1, rx_len - 1);
    rx_len--;
    parser_state = WAIT_START;
    expected_len = 0;
    return 0;
}
*/

void VescUart::reset_parser() {
    // Clear any buffered bytes and reset state so parsing restarts
    // cleanly at the next start byte. We do not free the buffer,
    // only reset lengths and state to avoid realloc churn.
    parser_state = WAIT_START;
    expected_len = 0;
    rx_len = 0;
    // Optionally zero the buffer for easier debugging, but not required.
    if (rx_buffer && rx_capacity > 0) memset(rx_buffer, 0, rx_capacity);
}

void VescUart::requestValues(uint8_t canId) {
    int32_t index = 0;
    int payloadSize = (canId == 0 ? 1 : 3);
    uint8_t payload[8];
    if (canId != 0) {
        payload[index++] = (uint8_t)COMM_FORWARD_CAN;
        payload[index++] = canId;
    }
    payload[index++] = (uint8_t)COMM_GET_VALUES;
    packSendPayload(payload, payloadSize);
}

bool VescUart::unpackPayload(uint8_t* message, int lenMes, uint8_t* payload) {
    uint16_t crcMessage = 0;
    uint16_t crcPayload = 0;

    crcMessage = message[lenMes - 3] << 8;
    crcMessage &= 0xFF00;
    crcMessage += message[lenMes - 2];

    memcpy(payload, &message[2], message[1]);

    crcPayload = crc16(payload, message[1]);

    if (crcPayload == crcMessage) {
        if (debugPort != NULL) {
            debugPort->print("Received: ");
            serialPrint(message, lenMes);
            debugPort->println();
        }
        return true;
    }
    return false;
}

int VescUart::packSendPayload(uint8_t* payload, int lenPay) {
    uint16_t crcPayload = crc16(payload, lenPay);
    int count = 0;
    uint8_t messageSend[256];

    if (lenPay <= 256) {
        messageSend[count++] = 2;
        messageSend[count++] = lenPay;
    } else {
        messageSend[count++] = 3;
        messageSend[count++] = (uint8_t)(lenPay >> 8);
        messageSend[count++] = (uint8_t)(lenPay & 0xFF);
    }

    memcpy(messageSend + count, payload, lenPay);
    count += lenPay;

    messageSend[count++] = (uint8_t)(crcPayload >> 8);
    messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
    messageSend[count++] = 3;

    if (serialPort != NULL) serialPort->write(messageSend, count);
    return count;
}

bool VescUart::processReadPacket(uint8_t* message) {
    COMM_PACKET_ID packetId;
    int32_t index = 0;

    packetId = (COMM_PACKET_ID)message[0];
    message++;  // drop packetId byte

    switch (packetId) {
        case COMM_FW_VERSION:
            fw_version.major = message[index++];
            fw_version.minor = message[index++];
            return true;

        case COMM_GET_VALUES:
            data.tempMosfet = buffer_get_float16(message, 10.0, &index);
            data.tempMotor = buffer_get_float16(message, 10.0, &index);
            data.avgMotorCurrent = buffer_get_float32(message, 100.0, &index);
            data.avgInputCurrent = buffer_get_float32(message, 100.0, &index);
            index += 4;  // skip id
            index += 4;
            data.dutyCycleNow = buffer_get_float16(message, 1000.0, &index);
            data.rpm = buffer_get_float32(message, 1.0, &index);
            data.inpVoltage = buffer_get_float16(message, 10.0, &index);
            data.ampHours = buffer_get_float32(message, 10000.0, &index);
            data.ampHoursCharged = buffer_get_float32(message, 10000.0, &index);
            data.wattHours = buffer_get_float32(message, 10000.0, &index);
            data.wattHoursCharged = buffer_get_float32(message, 10000.0, &index);
            data.tachometer = buffer_get_int32(message, &index);
            data.tachometerAbs = buffer_get_int32(message, &index);
            data.error = (mc_fault_code)message[index++];
            data.pidPos = buffer_get_float32(message, 1000000.0, &index);
            data.id = message[index++];
            return true;
        default:
            return false;
    }
}

void VescUart::setRPM(float rpm, uint8_t canId) {
    int32_t index = 0;
    int payloadSize = (canId == 0 ? 5 : 7);
    uint8_t payload[payloadSize];
    if (canId != 0) {
        payload[index++] = {COMM_FORWARD_CAN};
        payload[index++] = canId;
    }
    payload[index++] = {COMM_SET_RPM};
    buffer_append_int32(payload, (int32_t)(rpm), &index);
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
    buffer_append_int32(payload, (int32_t)(duty * 100000), &index);
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

void VescUart::serialPrint(uint8_t* data, int len) {
    if (debugPort != NULL) {
        for (int i = 0; i <= len; i++) {
            debugPort->print(data[i]);
            debugPort->print(" ");
        }
        debugPort->println("");
    }
}

void VescUart::set_rx_buffer_size(size_t size) {
    if (size == 0) return;
    uint8_t* new_buf = (uint8_t*)realloc(rx_buffer, size);
    if (new_buf == nullptr) {
        if (debugPort != NULL) debugPort->println("Failed to allocate rx buffer");
        return;
    }
    rx_buffer = new_buf;
    rx_capacity = size;
    if (rx_len > rx_capacity) rx_len = rx_capacity;
}
