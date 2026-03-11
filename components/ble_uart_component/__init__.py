import esphome.codegen as cg
from esphome.components import uart
from esphome.components.esp32 import add_idf_sdkconfig_option
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
        cv.Optional(CONF_DEVICE_NAME, default="VESC BLE UART Bridge"): cv.string,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):

    cg.add_library("h2zero/NimBLE-Arduino", None)

    cg.add_library("Preferences", None)

    cg.add_build_flag("-DCONFIG_BT_ENABLED")

    # technically IDF-flavoured even on Arduino framework — it still controls
    # what gets compiled into the BT stack. A bit cheeky but that's how ESPHome rolls.
    # add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ROLE_PERIPHERAL", True)
    # add_idf_sdkconfig_option("CONFIG_NIMBLE_CPP_LOG_LEVEL", 0)  # 0=debug!!!

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
