#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/core/log.h"
#include "VescUart.h"
#include "esphome/components/uart/uart.h"
#include "../ble_uart_component/ble_uart_component.h"

namespace esphome {
namespace vesc_component {

static const char* const TAG = "vesc_component";

#define MOTOR_POLE_PAIRS 21

class VescControlRpm : public number::Number {
   public:
    void control(float mechanical_rpm) override {
        // Hard clamp to 500 RPM limit for safety (adjust based on your setup)
        if (mechanical_rpm > 500.0f) {
            ESP_LOGW(TAG, "Requested RPM %.0f exceeds VESC limit. Clamping to 500 RPM.", mechanical_rpm);
            mechanical_rpm = 500.0f;
        } else if (mechanical_rpm < -500.0f) {
            ESP_LOGW(TAG, "Requested RPM %.0f is below VESC limit. Clamping to -500 RPM.", mechanical_rpm);
            mechanical_rpm = -500.0f;
        }
        target = mechanical_rpm;
    }

    float target = 0.0f;
};

class VescControlDutyCycle : public number::Number {
   public:
    void control(float duty_cycle) override {
        // Clamp duty cycle to -1% to 1% for safety
        if (duty_cycle > 1.0f) {
            ESP_LOGW(TAG, "Requested duty cycle %.3f%% exceeds limit. Clamping to 1%%.", duty_cycle);
            duty_cycle = 1.0f;
        } else if (duty_cycle < -1.0f) {
            ESP_LOGW(TAG, "Requested duty cycle %.3f%% is below limit. Clamping to -1%%.", duty_cycle);
            duty_cycle = -1.0f;
        }
        target = duty_cycle;
    }

    float target = 0.0f;
};

class VescControlCurrent : public number::Number {
   public:
    void control(float current) override {
        // Clamp current to 0A to 10A for safety (adjust based on your setup)
        if (current > 10.0f) {
            ESP_LOGW(TAG, "Requested current %.2f A exceeds limit. Clamping to 10 A.", current);
            current = 10.0f;
        } else if (current < 0.0f) {
            ESP_LOGW(TAG, "Requested current %.2f A is below limit. Clamping to 0 A.", current);
            current = 0.0f;
        }
        target = current;
    }

    float target = 0.0f;
};

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
    VescUart vesc{30};  // default 30ms timeout for UART responses

    VescUart::dataPackage latestData;
    unsigned long latestDataTime = 0;

   protected:
    sensor::Sensor* voltage_sensor_{nullptr};
    sensor::Sensor* rpm_sensor_{nullptr};
    sensor::Sensor* duty_cycle_sensor_{nullptr};
    sensor::Sensor* input_current_sensor_{nullptr};
    sensor::Sensor* phase_current_sensor_{nullptr};
    sensor::Sensor* fet_temp_sensor_{nullptr};
    binary_sensor::BinarySensor* uart_activity_sensor_{nullptr};

    VescControlRpm* rpm_control_{nullptr};
    VescControlDutyCycle* duty_cycle_control_{nullptr};
    VescControlCurrent* current_control_{nullptr};

    ulong setup_done = 0;

   public:
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
            ESP_LOGW(TAG, "No uart_adapter_, using Serial2 as fallback");
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
            ESP_LOGD(TAG, "UARTComponent provided; using it for VESC communication");
        } else {
            vesc.setSerialPort(&Serial2);
            ESP_LOGW(TAG, "No UARTComponent provided; using Serial2 as fallback. This may cause conflicts if Serial2 is used for other purposes.");
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

    // Called from codegen: set the RX buffer size for the UARTComponent (if provided)
    void set_rx_buffer_size(size_t s) {
        if (this->uart_) this->uart_->set_rx_buffer_size(s);
    }

    void loop() override {
        uint32_t now = millis();

        // When VESC Tool is connected over BLE, it owns the UART.
        // If we also try to read/parse UART frames here, we'll corrupt the stream.
        // So we just bail. The bridge's loop() runs separately and handles forwarding.
        if (ble_active_()) {
            static uint32_t last_log_time = 0;
            if (now - last_log_time >= 1000) {  // rate limit logs to 1 per second
                ESP_LOGD(TAG, "BLE is active, skipping ESPHome loop to avoid UART conflicts with VESC Tool");
                last_log_time = now;
            }
            return;
        }

        static bool first_run = true;
        if (first_run && (now - setup_done < 5000)) {
            // Wait 5 seconds after setup before trying to read, to give the VESC time to boot up and
            // respond, and the WiFi connection become established.
            static uint32_t vesc_wait_log_last = 0;
            if (now - vesc_wait_log_last >= 1000) {  // rate limit logs to 1 per second
                ESP_LOGD(TAG, "Waiting for VESC to boot and WiFi to connect...");
                vesc_wait_log_last = now;
            }
            if (this->uart_adapter_)
                this->uart_adapter_->flush();
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
                ESP_LOGW(TAG, "RX buffer exceeded threshold — discarded data and reset parser");
            }
            return;
        }

        // Process incoming bytes and update latest data when a full packet is received
        int res = vesc.processIncoming();
        if (res > 0) {
            latestData = vesc.data;
            latestDataTime = millis();
        }

        // Rate limit outgoing commands to at most 4 per second to avoid
        // overwhelming the VESC or the serial connection
        // now = millis(); // Updated at the start of loop()
        if (now - last_send_time_ > 250) {
            if (rpm_control_->target > 10.0f) {  // Small deadzone
                control_mode_ = 'R';
                send_rpm_command();
                // Clear duty cycle and current to avoid conflicts
                duty_cycle_control_->target = 0.0f;
                current_control_->target = 0.0f;
            } else if (duty_cycle_control_->target > 0.01f) {  // Small deadzone
                control_mode_ = 'D';
                send_duty_cycle_command();
                // Clear RPM and current to avoid conflicts
                rpm_control_->target = 0.0f;
                current_control_->target = 0.0f;
            } else if (current_control_->target > 0.01f) {  // Small deadzone
                control_mode_ = 'C';
                send_current_command();
                // Clear RPM and duty cycle to avoid conflicts
                rpm_control_->target = 0.0f;
                duty_cycle_control_->target = 0.0f;
            } else {
                control_mode_ = 'N';
                // Force 0.0 Amps (Coast) to protect the DC Source
                vesc.setCurrent(0.0f);
            }
            last_send_time_ = now;
        }
    }

    void update() override {
        // update() is called on a timer ('update_interval' in YAML).
        // If VESC Tool is connected, it's doing that itself — don't interfere.
        if (ble_active_()) return;

        if (this->voltage_sensor_)
            this->voltage_sensor_->publish_state(latestData.inpVoltage);
        if (this->rpm_sensor_)
            this->rpm_sensor_->publish_state(round(latestData.rpm / MOTOR_POLE_PAIRS));  // mRPM
        if (this->duty_cycle_sensor_)
            this->duty_cycle_sensor_->publish_state(latestData.dutyCycleNow);
        if (this->input_current_sensor_)
            this->input_current_sensor_->publish_state(latestData.avgInputCurrent);
        if (this->phase_current_sensor_)
            this->phase_current_sensor_->publish_state(latestData.avgMotorCurrent);
        if (this->fet_temp_sensor_)
            this->fet_temp_sensor_->publish_state(latestData.tempMosfet);
        if (this->uart_activity_sensor_)
            this->uart_activity_sensor_->publish_state((millis() - latestDataTime) < 2000);

        if (this->rpm_control_) {
            // ESP_LOGD(TAG, "Publishing control RPM: %.0f RPM", latestData.rpm / MOTOR_POLE_PAIRS);
            this->rpm_control_->publish_state(round(latestData.rpm / MOTOR_POLE_PAIRS));  // Publish mechanical RPM for feedback
        }
        if (this->duty_cycle_control_)
            this->duty_cycle_control_->publish_state(latestData.dutyCycleNow);
        if (this->current_control_)
            this->current_control_->publish_state(latestData.avgMotorCurrent);

        // ESP_LOGD(TAG, "Requesting values");
        this->vesc.requestValues();
    }

    // Setters for the Python code to use
    void set_voltage_sensor(sensor::Sensor* s) { voltage_sensor_ = s; }
    void set_rpm_sensor(sensor::Sensor* s) { rpm_sensor_ = s; }
    void set_duty_cycle_sensor(sensor::Sensor* s) { duty_cycle_sensor_ = s; }
    void set_input_current_sensor(sensor::Sensor* s) { input_current_sensor_ = s; }
    void set_phase_current_sensor(sensor::Sensor* s) { phase_current_sensor_ = s; }
    void set_fet_temp_sensor(sensor::Sensor* s) { fet_temp_sensor_ = s; }
    void set_uart_activity_sensor(binary_sensor::BinarySensor* s) { uart_activity_sensor_ = s; }

    void set_rpm_control(VescControlRpm* n) { rpm_control_ = n; }
    void set_duty_cycle_control(VescControlDutyCycle* n) { duty_cycle_control_ = n; }
    void set_current_control(VescControlCurrent* n) { current_control_ = n; }

    // Called from codegen to set the BLE UART component, so we can check its connection
    // status and avoid conflicts with VESC Tool when it's active
    void set_ble_uart_component(ble_uart_component::BleUartComponent* c) {
        ESP_LOGD(TAG, "Setting BLE UART component: %p", c);
        ble_uart_component_ = c;
    }

    void send_rpm_command() {
        float erpm = rpm_control_->target * MOTOR_POLE_PAIRS;
        // ESP_LOGI(TAG, "Sending RPM command: %.0f ERPM (%.0f mRPM)", erpm, target_mrpm);
        this->vesc.setRPM(erpm);
    }

    void send_duty_cycle_command() {
        // ESP_LOGI(TAG, "Sending Duty Cycle command: %.3f%%", target_duty_cycle);
        this->vesc.setDuty(duty_cycle_control_->target);
    }

    void send_current_command() {
        // ESP_LOGI(TAG, "Sending Current command: %.2f A", target_current);
        this->vesc.setCurrent(current_control_->target);
    }

   protected:
    esphome::uart::UARTComponent* uart_{nullptr};
    esphome::uart::UARTComponent* debug_uart_{nullptr};
    UARTComponentStream* uart_adapter_{nullptr};
    UARTComponentStream* debug_adapter_{nullptr};
    ble_uart_component::BleUartComponent* ble_uart_component_{nullptr};

    uint32_t last_send_time_{0};
    char control_mode_ = 'N';  // 'R' for RPM, 'D' for Duty Cycle, 'C' for Current, 'N' for None

    bool ble_active_() const {
        // return ble_uart_component_ != nullptr && ble_uart_component_->is_ble_connected();
        if (ble_uart_component_ == nullptr) {
            ESP_LOGD(TAG, "BLE UART component not set, treating BLE as inactive");
            return false;
        }
        return ble_uart_component_->is_ble_connected();
    }

};  // class VescComponent

};  // namespace vesc_component
};  // namespace esphome
