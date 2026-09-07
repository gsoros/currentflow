import esphome.codegen as cg
from esphome.components import uart
from esphome.components.esp32 import (
    add_idf_sdkconfig_option,
    include_builtin_idf_component,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID

ble_uart_component_ns = cg.esphome_ns.namespace("ble_uart_component")
BleUartComponent = ble_uart_component_ns.class_(
    "BleUartComponent",
    cg.Component,
    uart.UARTDevice,
)


CONF_DEVICE_NAME = "device_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(
            BleUartComponent
        ),  # auto-generate C++ variable name if not given
        cv.Optional(CONF_DEVICE_NAME, default="VESC BLE UART"): cv.string,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):

    cg.add_library("Preferences", None)
    cg.add_library("h2zero/NimBLE-Arduino", None)

    # NimBLE-Arduino includes esp_bt.h, which lives in the IDF 'bt' component.
    # 'bt' is excluded from builds by default (it's large); opting back in also
    # puts it on the converted library's REQUIRES list, so the CMake dependency
    # check passes. Replaces the old -DCONFIG_BT_ENABLED build-flag hack.
    include_builtin_idf_component("bt")

    # Controls what gets compiled into the BT stack.
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ROLE_PERIPHERAL", True)

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
