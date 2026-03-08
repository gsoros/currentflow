#include "ble_uart_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"  // for App.get_name()

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
    BLEDevice::init(String(this->device_name_.c_str()));

    // Bump MTU so VESC Tool can send bigger packets (default 23 bytes is painful)
    BLEDevice::setMTU(512);

    ble_server_ = BLEDevice::createServer();
    ble_server_->setCallbacks(&server_callbacks_);

    // Create the Nordic UART Service
    nus = ble_server_->createService(NUS_SERVICE_UUID);

    // TX characteristic: ESP → Phone (NOTIFY)
    tx_char_ = nus->createCharacteristic(
        NUS_TX_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);
    // BLE2902 is the Client Characteristic Configuration Descriptor.
    // Without it, notifications don't work. Annoying but mandatory.
    tx_char_->addDescriptor(new BLE2902());

    // RX characteristic: Phone → ESP (WRITE)
    rx_char_ = nus->createCharacteristic(
        NUS_RX_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rx_char_->setCallbacks(&rx_callbacks_);

    nus->start();

    // Advertise so VESC Tool (or any NUS scanner) can find us
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);
    adv->setScanResponse(true);  // Needed to send the full device name in scan
    adv->setMinPreferred(0x06);  // These two help with iPhone connection stability
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    ESP_LOGI(TAG, "BLE advertising started");
}

/*
// This version sends multiple packets per loop
void BleUartComponent::loop() {
    if (!connected_count_) return;  // Don't do anything if we're not connected

    size_t available = this->available();
    if (available == 0) return;

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
}
*/

void BleUartComponent::loop() {
    if (!this->enabled_ || !this->connected_count_) return;

    size_t available = this->available();
    if (available == 0) return;

    size_t to_read = std::min(available, current_mtu_payload_size_);

    std::vector<uint8_t> chunk;
    while (this->available() > 0 && chunk.size() < to_read) {
        chunk.push_back(this->read());
    }

    this->tx_char_->setValue(chunk.data(), chunk.size());
    this->tx_char_->notify();

    ESP_LOGD(TAG, "Sent %d bytes over BLE, %d bytes remaining in UART buffer", chunk.size(), this->available());
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