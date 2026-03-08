#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace esphome {
namespace ble_uart_component {

static const char* const TAG = "ble_uart_component";

// Nordic UART Service UUIDs - these are the standard ones VESC Tool expects
// Don't change these or VESC Tool won't find the device
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Phone writes here
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Phone reads from here

class BleUartComponent : public Component, public uart::UARTDevice {
   public:
    // ---- Lifecycle ----
    void setup() override;
    void loop() override;

    // ---- Config setters (called from Python/YAML codegen) ----
    void set_device_name(const std::string& name) { device_name_ = name; }

    // ---- Public API for VescComponent to query ----
    bool is_ble_connected() const {
        // ESP_LOGD(TAG, "is_ble_connected() connected_count_: %d", connected_count_);
        return connected_count_ > 0;
    }

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool state) {
        this->enabled_ = state;
        ESP_LOGI(TAG, "State: %s", state ? "ENABLED" : "DISABLED");

        if (this->is_enabled()) {
            // Start advertising if toggled ON
            // this->ble_server_->get_advertising()->start();
            BLEDevice::startAdvertising();
            ESP_LOGD(TAG, "Advertising started.");
        } else {
            // 2. Disconnect ALL existing peers
            for (auto& kv : ble_server_->getPeerDevices(true)) {
                ESP_LOGI(TAG, "Disconnecting client");
                ble_server_->disconnect(kv.first);  // kv.first is the connId
            }
        }
    }

   protected:
    std::string device_name_{"VESC BLE Bridge"};
    bool enabled_ = true;  // Default to ON

    BLEServer* ble_server_{nullptr};
    BLECharacteristic* tx_char_{nullptr};  // ESP → Phone
    BLECharacteristic* rx_char_{nullptr};  // Phone → ESP
    BLEService* nus{nullptr};              // Nordic UART Service

    size_t conn_id_{0};  // ID of the current BLE connection, if any
    void set_conn_id(size_t id) { conn_id_ = id; }
    size_t connected_count_{0};  // 0 or 1 since we reject multiple connections
    void set_connected_count(size_t count) { connected_count_ = count; }
    size_t current_mtu_payload_size_{20};  // Default BLE MTU is 23, 3 bytes are used for overhead
    void set_mtu(size_t mtu);

    // Callbacks are inner classes so they can access our private members
    class ServerCallbacks : public BLEServerCallbacks {
       public:
        BleUartComponent* parent_;

        void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
            // 1. Immediate check: If we already have a connection, kill the intruder.
            if (parent_->connected_count_ >= 1 || !parent_->is_enabled()) {
                pServer->disconnect(param->connect.conn_id);
                ESP_LOGW(TAG, "Rejected client (conn_id: %d)", param->connect.conn_id);
                return;
            }

            // 2. Schedule the 'acceptance' tasks on the main thread
            parent_->defer([this]() {
                // Double-check we haven't disconnected in the few ms it took to defer
                if (parent_->connected_count_ == 0) {
                    parent_->set_connected_count(1);
                    // BLEDevice::getAdvertising()->stop();
                    BLEDevice::stopAdvertising();
                    ESP_LOGI(TAG, "Connection accepted. Advertising stopped.");
                }
            });
        }
        void onDisconnect(BLEServer* pServer) override {
            if (!parent_->is_enabled()) {
                BLEDevice::stopAdvertising();
                // parent_->ble_server_->get_advertising()->stop();
                ESP_LOGD("ble", "Client disconnected; BLE is disabled: not advertising");
            }
            // 1. Flush UART incoming buffer
            // parent_->flush_input();

            // 2. Set count to 0 so the loop knows we are disconnected
            parent_->set_connected_count(0);

            // 3. Wait 300ms before restarting BLE to let WiFi/API catch up
            parent_->set_timeout("restart_ble_adv", 300, [this]() {
                if (!parent_->is_enabled()) return;
                // One last flush just in case the VESC sent more junk
                parent_->flush_input();
                BLEDevice::startAdvertising();
                // parent_->ble_server_->get_advertising()->start();
                ESP_LOGI(TAG, "Advertising started.");
            });
        }
        void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
            if (param->mtu.mtu > 23) {
                ESP_LOGI(TAG, "MTU changed to %d", param->mtu.mtu);
                parent_->set_mtu((size_t)param->mtu.mtu);
            }
        }
    };

    class RxCallbacks : public BLECharacteristicCallbacks {
       public:
        BleUartComponent* parent_;
        // VESC Tool wrote something → forward it to the physical UART
        void onWrite(BLECharacteristic* characteristic) override {
            uint8_t* rawData = characteristic->getData();
            size_t len = characteristic->getLength();
            ESP_LOGD(TAG, "onWrite() Received %d bytes via BLE", len);
            if (rawData != nullptr && len > 0) {
                // Flush UART incoming buffer to give a chance for the reply
                // to go through in case of telemetry flood.
                parent_->flush_input();
                parent_->write_array(rawData, len);
            }
        }
    };

    ServerCallbacks server_callbacks_;
    RxCallbacks rx_callbacks_;

    void flush_input() {
        if (!this->available()) return;
        ulong now = millis();
        size_t count = 0;
        uint8_t dummy;
        while (count++ && this->available()) this->read_byte(&dummy);
        ESP_LOGD(TAG, "Flushed %d bytes from UART incoming buffer in %d ms", count, millis() - now);
    }
};

}  // namespace ble_uart_component
}  // namespace esphome