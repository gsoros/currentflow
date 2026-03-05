/*
  VescUart.h -- renamed from refactored_VescUart.h
*/

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "helpers.h"

class VescUart {
   public:
    VescUart(uint32_t timeout_ms = 100);

    struct dataPackage {
        float avgMotorCurrent;
        float avgInputCurrent;
        float dutyCycleNow;
        float rpm;
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
    };

    struct nunchuckPackage {
        int valueX;
        int valueY;
        bool upperButton;
        bool lowerButton;
    };

    struct FWversionPackage {
        uint8_t major;
        uint8_t minor;
    };

    void setSerialPort(Stream* port);
    void setDebugPort(Stream* port);
    void setRPM(float rpm, uint8_t canId = 0);
    void setDuty(float duty, uint8_t canId = 0);
    void sendKeepalive(uint8_t canId = 0);
    // Non-blocking processor: call regularly from the main loop to feed
    // incoming bytes and detect full messages. Returns payload length
    // when a complete message is parsed, 0 otherwise.
    int processIncoming();
    // Feed one byte into the parser. Returns payload length when a
    // complete packet is received and processed, 0 otherwise.
    int parseByte(uint8_t b);
    // Send a values request without blocking for the response.
    void requestValues(uint8_t canId = 0);

    // Configuration setters
    void set_timeout_ms(uint32_t t) { _TIMEOUT = t; }
    void set_rx_buffer_size(size_t size);
    // Reset parser state and clear RX buffer. Safe to call when discarding
    // data due to overflow or protocol errors so the parser resumes cleanly.
    void reset_parser();
    ~VescUart();

    dataPackage data;
    nunchuckPackage nunchuck;
    FWversionPackage fw_version;

   private:
    Stream* serialPort = nullptr;
    Stream* debugPort = nullptr;
    uint32_t _TIMEOUT;

    // RX buffering for non-blocking parsing (dynamically allocated)
    uint8_t* rx_buffer = nullptr;
    size_t rx_capacity = 0;
    size_t rx_len = 0;
    // Explicit parser state machine
    enum ParserState { WAIT_START,
                       READ_LENGTH,
                       READ_EXT_LENGTH,
                       READ_PAYLOAD };
    ParserState parser_state = WAIT_START;
    size_t expected_len = 0;

    bool unpackPayload(uint8_t* message, int lenMes, uint8_t* payload);
    int packSendPayload(uint8_t* payload, int lenPay);
    bool processReadPacket(uint8_t* message);
    void serialPrint(uint8_t* data, int len);
};
