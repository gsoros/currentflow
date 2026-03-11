/*

  ble_uart_component.h

  */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <NimBLEDevice.h>

namespace esphome {
namespace ble_uart_component {

static const char *const TAG = "ble_uart_component";

// Nordic UART Service UUIDs - these are the standard ones VESC Tool expects
// Don't change these or VESC Tool won't find the device
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Phone writes here
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Phone reads from here

class BleUartComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  // ---- Config setters (called from Python/YAML codegen) ----
  void set_device_name(const std::string &name) { device_name_ = name; }

  // ---- Public API for VescComponent to query ----
  bool is_ble_connected() const {
    // ESP_LOGD(TAG, "is_ble_connected() connected_count_: %d", connected_count_);
    return connected_count_ > 0;
  }

  bool is_enabled() const { return enabled_; }

  // ---- Public API for Python/YAML codegen ----
  // Sets the BLE state and start advertising, if necessary
  // Disconnects all clients if disabled
  void set_enabled(bool state) {
    this->enabled_ = state;
    ESP_LOGI(TAG, "Requested state: %s", state ? "ENABLED" : "DISABLED");

    if (this->defer_start_) {
      ESP_LOGD(TAG, "Defer start active, not starting advertising");
      return;
    }
    if (!this->ble_server_) {
      ESP_LOGD(TAG, "BLE not ready");
      return;
    }
    if (this->is_enabled()) {
      this->startAdvertising();
      return;
    }
    for (auto conn_handle : ble_server_->getPeerDevices()) {
      ESP_LOGI(TAG, "Disconnecting client %d", conn_handle);
      ble_server_->disconnect(conn_handle);
    }
  }

 protected:
  std::string device_name_{"VESC BLE Bridge"};
  bool enabled_ = true;      // Default to ON
  bool defer_start_ = true;  // If true, startAdvertising() is deferred

  NimBLEServer *ble_server_{nullptr};
  NimBLECharacteristic *tx_char_{nullptr};  // ESP → Phone
  NimBLECharacteristic *rx_char_{nullptr};  // Phone → ESP
  NimBLEService *nus{nullptr};              // Nordic UART Service

  size_t conn_id_{0};  // ID of the current BLE connection, if any
  void set_conn_id(size_t id) { conn_id_ = id; }
  size_t connected_count_{0};  // 0 or 1 since we reject multiple connections
  void set_connected_count(size_t count) { connected_count_ = count; }
  size_t current_mtu_payload_size_{20};  // Default BLE MTU is 23, 3 bytes are used for overhead
  void set_mtu(size_t mtu);

  // Callbacks are inner classes so they can access our private members
  class ServerCallbacks : public NimBLEServerCallbacks {
   public:
    BleUartComponent *parent_;

    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
      // 1. Immediate check: If we already have a connection, kill the intruder.
      if (parent_->connected_count_ >= 1 || !parent_->is_enabled()) {
        pServer->disconnect(connInfo.getConnHandle());
        ESP_LOGW(TAG, "Rejected client");
        return;
      }

      // 2. Schedule the 'acceptance' tasks on the main thread
      parent_->defer([this]() {
        // Double-check we haven't disconnected in the few ms it took to defer
        if (parent_->connected_count_ == 0) {
          parent_->set_connected_count(1);
          ESP_LOGI(TAG, "Connection accepted.");
          parent_->stopAdvertising();
        }
      });
    }
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {
      ESP_LOGI(TAG, "Client disconnected; reason: %d", reason);
      if (!parent_->is_enabled())
        parent_->stopAdvertising();

      // Set count to 0 so the loop knows we are disconnected
      parent_->set_connected_count(0);

      // Wait 300ms before restarting advertising
      parent_->set_timeout("restart_ble_adv", 300, [this]() {
        if (!parent_->is_enabled())
          return;
        if (!parent_->startAdvertising())
          ESP_LOGW(TAG, "Failed to restart advertising");
      });
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override {
      if (MTU > 23) {
        ESP_LOGI(TAG, "MTU changed to %d", MTU);
        parent_->set_mtu((size_t) MTU);
      }
    }
  };

  class RxCallbacks : public NimBLECharacteristicCallbacks {
   public:
    BleUartComponent *parent_;
    // VESC Tool wrote something → forward it to the physical UART
    void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &connInfo) override {
      NimBLEAttValue val = c->getValue();
      ESP_LOGD(TAG, "onWrite() Received %d bytes via BLE", val.length());
      if (val.length() <= 0)
        return;
      // Flush UART incoming buffer to give a chance for the reply
      // to go through in case of telemetry flood.
      parent_->flush_input();
      parent_->write_array(val.data(), val.length());
    }
  };

  ServerCallbacks server_callbacks_;
  RxCallbacks rx_callbacks_;

  bool startAdvertising() {
    if (!this->is_enabled()) {
      ESP_LOGD(TAG, "BLE is disabled: not advertising");
      return false;
    }
    if (!this->ble_server_) {
      ESP_LOGW(TAG, "BLE not ready");
      return false;
    }
    auto adv = this->ble_server_->getAdvertising();
    if (!adv) {
      ESP_LOGW(TAG, "BLE adv not ready");
      return false;
    }
    if (adv->isAdvertising()) {
      ESP_LOGD(TAG, "Already advertising");
      return false;
    }

    bool ret = adv->start();
    if (ret)
      ESP_LOGI(TAG, "Advertising started.");
    else
      ESP_LOGE(TAG, "Advertising failed");
    return ret;
  }

  bool stopAdvertising() {
    if (!this->ble_server_) {
      ESP_LOGW(TAG, "BLE not ready");
      return false;
    }
    auto adv = this->ble_server_->getAdvertising();
    if (!adv) {
      ESP_LOGW(TAG, "BLE adv not ready");
      return false;
    }
    if (!adv->isAdvertising()) {
      ESP_LOGD(TAG, "Not advertising");
      return false;
    }
    bool ret = adv->stop();
    if (ret)
      ESP_LOGI(TAG, "Advertising stopped.");
    else
      ESP_LOGE(TAG, "Advertising stop failed");
    return ret;
  }

  void flush_input() {
    if (!this->available())
      return;
    ulong now = millis();
    size_t count = 0;
    uint8_t dummy;
    while (count++ && this->available())
      this->read_byte(&dummy);
    ESP_LOGD(TAG, "Flushed %d bytes from UART incoming buffer in %d ms", count, millis() - now);
  }
};

}  // namespace ble_uart_component
}  // namespace esphome
