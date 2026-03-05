#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"  // Needed for sensor::Sensor
#include "esphome/core/log.h"
#include "VescUart.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace vesc_component {

// Adapter: wrap esphome::uart::UARTComponent with a minimal Arduino Stream
// implementation so existing VescUart (which expects Stream*) can use it.
class UARTComponentStream : public Stream {
   public:
    UARTComponentStream(esphome::uart::UARTComponent* uart) : uart_(uart) {}

    int available() override { return (int)(this->uart_ ? this->uart_->available() : 0); }
    int read() override {
        if (!this->uart_) return -1;
        uint8_t b;
        if (this->uart_->read_byte(&b))
            return (int)b;
        return -1;
    }
    int peek() override {
        if (!this->uart_) return -1;
        uint8_t b;
        if (this->uart_->peek_byte(&b))
            return (int)b;
        return -1;
    }
    void flush() override {
        if (this->uart_) this->uart_->flush();
    }
    size_t write(uint8_t c) override {
        if (!this->uart_) return 0;
        this->uart_->write_byte(c);
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        if (!this->uart_) return 0;
        this->uart_->write_array(buffer, size);
        return size;
    }

   protected:
    int availableForWrite() override { return 0; }

   private:
    esphome::uart::UARTComponent* uart_;
};

class VescComponent : public PollingComponent {
   public:
    VescUart vesc{30};  // 30ms timeout for UART responses
    VescUart::dataPackage latestData;
    unsigned long latestDataTime = 0;
    bool online = false;
    sensor::Sensor* voltage_sensor_{nullptr};
    sensor::Sensor* rpm_sensor_{nullptr};
    sensor::Sensor* duty_cycle_sensor_{nullptr};
    sensor::Sensor* input_current_sensor_{nullptr};
    sensor::Sensor* phase_current_sensor_{nullptr};
    sensor::Sensor* fet_temp_sensor_{nullptr};
    ulong setup_done = 0;

    void setup() override {
        // If a UARTComponent was provided via codegen, its adapter will
        // have been created in set_uart() and vesc will already be
        // pointed at it. Otherwise initialize and use Serial2 as the
        // default port.
        if (this->uart_adapter_ != nullptr) {
            vesc.setSerialPort(this->uart_adapter_);
        } else {
            // Fallback to hardware Serial2 if no UARTComponent adapter provided.
            // Keep only the fallback assignment here; avoid direct Serial2
            // operations elsewhere to prefer the adapter interface.
            vesc.setSerialPort(&Serial2);
        }
        setup_done = millis();
    }
    // Called from codegen: set the UART used by this component
    void set_uart(esphome::uart::UARTComponent* uart) {
        // Store pointer and create a Stream adapter for VescUart
        this->uart_ = uart;
        if (this->uart_adapter_)
            delete this->uart_adapter_;
        this->uart_adapter_ = (uart != nullptr) ? new UARTComponentStream(uart) : nullptr;
        if (this->uart_adapter_) {
            vesc.setSerialPort(this->uart_adapter_);
            ESP_LOGD("vesc", "UARTComponent provided; using it for VESC communication");
        } else {
            vesc.setSerialPort(&Serial2);
            ESP_LOGW("vesc", "No UARTComponent provided; using Serial2 as fallback. This may cause conflicts if Serial2 is used for other purposes.");
        }
    }

    void set_debug_uart(esphome::uart::UARTComponent* uart) {
        this->debug_uart_ = uart;
        if (this->debug_adapter_)
            delete this->debug_adapter_;
        this->debug_adapter_ = (uart != nullptr) ? new UARTComponentStream(uart) : nullptr;
        vesc.setDebugPort(this->debug_adapter_);
    }
    void set_timeout_ms(uint32_t t) { vesc.set_timeout_ms(t); }
    void set_rx_buffer_size(size_t s) { vesc.set_rx_buffer_size(s); }

    void loop() override {
        static bool first_run = true;

        if (first_run && (millis() - setup_done < 5000)) {
            // Wait 5 seconds after setup before trying to read, to give the VESC time to boot up and respond, and the WiFi connection become established.
            // Rate-limit this debug message to at most once per second to avoid
            // spamming the log during early boot where loop() runs frequently.
            static uint32_t vesc_wait_log_last = 0;
            uint32_t vesc_wait_log_now = millis();
            if (vesc_wait_log_now - vesc_wait_log_last >= 1000) {
                ESP_LOGD("vesc", "Waiting for VESC to boot and WiFi to connect...");
                vesc_wait_log_last = vesc_wait_log_now;
            }
            if (this->uart_adapter_)
                this->uart_adapter_->flush();
            // If no adapter is present we rely on the fallback Serial2
            // assigned in setup; avoid calling Serial2 methods directly here.
            return;
        }
        first_run = false;

        // Safety: if the hardware buffer becomes extremely full, discard a
        // bounded number of bytes and reset the parser so we can recover.
        // Reasons:
        // - Unbounded drains can block the loop and cause watchdog resets.
        // - `flush()` only affects TX, not RX, so we must read to clear RX.
        // Strategy: discard up to `max_discard` bytes (128) per loop call,
        // then reset the VESC parser state so parsing resumes at the next
        // valid start byte.
        size_t avail = (this->uart_adapter_ ? this->uart_adapter_->available() : 0);
        const size_t max_discard = 128;
        if (avail > 200) {
            if (this->uart_adapter_) {
                size_t to_discard = (avail > max_discard) ? max_discard : avail;
                for (size_t i = 0; i < to_discard && this->uart_adapter_->available(); ++i)
                    this->uart_adapter_->read();
                // Reset parser so we don't attempt to interpret leftover
                // partial data as the start of a valid packet.
                vesc.reset_parser();
            }
            ESP_LOGW("vesc", "RX buffer exceeded threshold — discarded data and reset parser");
            return;
        }

        // Ask VescUart to process incoming bytes from its configured port.
        int res = vesc.processIncoming();
        if (res > 0) {
            latestData = vesc.data;
            latestDataTime = millis();
        }
    }

    void update() override {
        // Update diagnostic
        online = (millis() - latestDataTime) < 2000;

        if (this->voltage_sensor_)
            this->voltage_sensor_->publish_state(latestData.inpVoltage);
        if (this->rpm_sensor_)
            this->rpm_sensor_->publish_state(latestData.rpm / 21.0f);
        if (this->duty_cycle_sensor_)
            this->duty_cycle_sensor_->publish_state(latestData.dutyCycleNow);
        if (this->input_current_sensor_)
            this->input_current_sensor_->publish_state(latestData.avgInputCurrent);
        if (this->phase_current_sensor_)
            this->phase_current_sensor_->publish_state(latestData.avgMotorCurrent);
        if (this->fet_temp_sensor_)
            this->fet_temp_sensor_->publish_state(latestData.tempMosfet);

        // Keepalive + request fresh values; parsing remains non-blocking
        this->vesc.sendKeepalive();
        this->vesc.requestValues();
    }

    // Setters for the Python code to use
    void set_voltage_sensor(sensor::Sensor* s) { voltage_sensor_ = s; }
    void set_rpm_sensor(sensor::Sensor* s) { rpm_sensor_ = s; }
    void set_duty_cycle_sensor(sensor::Sensor* s) { duty_cycle_sensor_ = s; }
    void set_input_current_sensor(sensor::Sensor* s) { input_current_sensor_ = s; }
    void set_phase_current_sensor(sensor::Sensor* s) { phase_current_sensor_ = s; }
    void set_fet_temp_sensor(sensor::Sensor* s) { fet_temp_sensor_ = s; }

    void set_target_rpm(float mechanical_rpm) {
        // Multiply by 21 for your 42-pole motor
        float target_erpm = mechanical_rpm * 21.0f;

        // Hard clamp to your VESC Tool 10k limit for safety
        if (target_erpm > 10000.0f) {
            ESP_LOGW("vesc", "Requested RPM %.0f exceeds VESC limit. Clamping to 10,000 eRPM.", target_erpm);
            target_erpm = 10000.0f;
        }
        if (target_erpm < -10000.0f) {
            ESP_LOGW("vesc", "Requested RPM %.0f is below VESC limit. Clamping to -10,000 eRPM.", target_erpm);
            target_erpm = -10000.0f;
        }

        this->vesc.setRPM(target_erpm);
        ESP_LOGI("vesc", "Setting Target RPM: %.0f (eRPM: %.0f)", mechanical_rpm, target_erpm);
    }

    void set_target_duty_cycle(float duty_cycle) {
        // ESP_LOGD("vesc", "set_target_duty_cycle is disabled");
        // return;

        // Clamp duty cycle to -1% to 1% for safety
        if (duty_cycle > 1.0f) {
            ESP_LOGW("vesc", "Requested duty cycle %.3f%% exceeds limit. Clamping to 1%%.", duty_cycle);
            duty_cycle = 1.0f;
        }
        if (duty_cycle < -1.0f) {
            ESP_LOGW("vesc", "Requested duty cycle %.3f%% is below limit. Clamping to -1%%.", duty_cycle);
            duty_cycle = -1.0f;
        }

        this->vesc.setDuty(duty_cycle);
        ESP_LOGI("vesc", "Setting Target Duty Cycle: %.3f%%", duty_cycle);
    }

   private:
    esphome::uart::UARTComponent* uart_{nullptr};
    esphome::uart::UARTComponent* debug_uart_{nullptr};
    UARTComponentStream* uart_adapter_{nullptr};
    UARTComponentStream* debug_adapter_{nullptr};
};  // class VescComponent

};  // namespace vesc_component
};  // namespace esphome

// Glue the .cpp files here so that the build system can find them without needing the absolute path in C++
// #include "VescUart.cpp"
// #include "buffer.cpp"
// #include "crc.cpp"