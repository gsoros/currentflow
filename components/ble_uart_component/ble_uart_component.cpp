#include "ble_uart_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"  // for App.get_name()
// #include "esp_bt.h"

namespace esphome {
namespace ble_uart_component {

void BleUartComponent::setup() {
    // Wire up back-pointers before registering callbacks
    server_callbacks_.parent_ = this;
    rx_callbacks_.parent_ = this;

    // If no name was provided in YAML, use the global node name
    if (this->device_name_.empty()) {
        this->device_name_ = App.get_name();
    }
    ESP_LOGI(TAG, "Starting BLE as '%s'", this->device_name_.c_str());

    if (!NimBLEDevice::init(this->device_name_)) ESP_LOGE(TAG, "BLE init failed");
    // delay(200);  // DEBUG TEST

    ESP_LOGD(TAG, "Controller status: %d", esp_bt_controller_get_status());

    // Bump MTU so VESC Tool can send bigger packets (default 23 bytes is painful)
    if (!NimBLEDevice::setMTU(517)) ESP_LOGE(TAG, "BLE setMTU failed");

    ble_server_ = NimBLEDevice::createServer();
    ble_server_->setCallbacks(&server_callbacks_);

    // Create the Nordic UART Service
    nus = ble_server_->createService(NUS_SERVICE_UUID);

    // TX characteristic: ESP → Phone (NOTIFY)
    tx_char_ = nus->createCharacteristic(
        NUS_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY);
    // tx_char_->addDescriptor(new NimBLE2902()); // Handled by Nimble automatically

    // RX characteristic: Phone → ESP (WRITE)
    rx_char_ = nus->createCharacteristic(
        NUS_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx_char_->setCallbacks(&rx_callbacks_);

    if (!nus->start()) ESP_LOGE(TAG, "NUS start failed");

    // Advertise so VESC Tool (or any NUS scanner) can find us
    auto adv = ble_server_->getAdvertising();
    adv->setName(this->device_name_);
    adv->setAppearance(0x0240);  // Generic Keyring
    adv->addServiceUUID(NUS_SERVICE_UUID);
    adv->enableScanResponse(true);  // Needed to send the full device name in scan
    // Set the preferred connection interval range (Min, Max)
    // 7.5ms to 22.5ms (very fast for VESC telemetry, if your HW can do it)
    // adv->setMinInterval(0x06);
    // adv->setMaxInterval(0x12);
    // 100ms to 150ms (industry standard for reliable discovery)
    adv->setMinInterval(0xA0);
    adv->setMaxInterval(0xF0);

    ble_server_->start();

    ESP_LOGD(TAG, "setup() done");
}

/*
// This version sends multiple packets per loop

    uint8_t packets_sent = 0;
    while (available && connected_count_) {
        size_t to_read = std::min(available, current_mtu_payload_size_);

        uint8_t chunk[512];
        if (!read_array(chunk, to_read))
            break;

        ESP_LOGD(TAG, "Read %dB from UART, sending to BLE", to_read);
        tx_char_->setValue(chunk, to_read);
        tx_char_->notify();

        available -= to_read;
        delay(1);

        if (++packets_sent >= 3) {
            ESP_LOGD(TAG, "Reached limit of %d packets per loop, %dB remaining", packets_sent, available);
            break;
        }
        ESP_LOGD(TAG, "Packets sent in this loop: %d, %dB remaining", packets_sent, available);
    }

*/

void BleUartComponent::loop() {
    if (!this->enabled_) return;

    if (this->defer_start_ && millis() > 3000) {
        // apply saved state after 3 seconds
        // this will start advertising if necessary
        this->defer_start_ = false;
        this->set_enabled(this->enabled_);
        ESP_LOGD(TAG, "Controller status: %d", esp_bt_controller_get_status());
    }

    if (!this->connected_count_) return;

    size_t available = this->available();
    if (available == 0) return;

    size_t to_read = std::min(available, (size_t)current_mtu_payload_size_);

    uint8_t buffer[to_read];
    this->read_array(buffer, to_read);

    // Use notify(data, len) instead of setValue + notify
    // This is more efficient in NimBLE v2.x
    this->tx_char_->notify(buffer, to_read);

    ESP_LOGD(TAG, "Sent %d bytes over BLE, %d bytes remaining in UART buffer", to_read, this->available());
}

void BleUartComponent::set_mtu(size_t mtu) {
    // BLE minimum is 23. Max for ESP32 is typically 517.
    if (mtu < 23) mtu = 23;
    if (mtu > 517) mtu = 517;

    // We store the usable data length
    this->current_mtu_payload_size_ = mtu - 3;
    ESP_LOGD(TAG, "Current MTU payload size %d", this->current_mtu_payload_size_);
}

}  // namespace ble_uart_component
}  // namespace esphome