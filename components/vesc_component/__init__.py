import os

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL

# DEPENDENCIES = ["sensor", "text_sensor", "number", "uart"]

CONF_UART = "uart"
CONF_DEBUG_PORT = "debug_port"
CONF_TIMEOUT_MS = "timeout_ms"
CONF_RX_BUF = "rx_buffer_size"
CONF_BLE_UART_COMPONENT_ID = "ble_uart_component_id"
CONF_POLE_PAIRS = "motor_pole_pairs"

# 1. Define namespace and class
vesc_component_ns = cg.esphome_ns.namespace("vesc_component")
VescComponent = vesc_component_ns.class_("VescComponent", cg.PollingComponent)

# 2. Schema: This is what you see in the YAML
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VescComponent),
        cv.Optional(CONF_UART): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_DEBUG_PORT): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_TIMEOUT_MS, default=100): cv.int_,
        cv.Optional(CONF_RX_BUF, default=512): cv.int_,
        cv.Optional(CONF_BLE_UART_COMPONENT_ID): cv.use_id(
            "ble_uart_component.BleUartComponent"
        ),
        cv.Optional(CONF_POLE_PAIRS, default=15): cv.int_range(min=1, max=100),
    }
).extend(cv.polling_component_schema("1s"))


# 3. Code Generation
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # UART
    uart_obj = await cg.get_variable(config[CONF_UART])
    cg.add(var.set_uart(uart_obj))

    # Optional debug UART
    if CONF_DEBUG_PORT in config:
        debug_obj = await cg.get_variable(config[CONF_DEBUG_PORT])
        cg.add(var.set_debug_uart(debug_obj))

    # Optional timeout and rx buffer size
    cg.add(var.set_timeout_ms(config[CONF_TIMEOUT_MS]))
    cg.add(var.set_rx_buffer_size(config[CONF_RX_BUF]))

    # Optional update interval (duration)
    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if CONF_BLE_UART_COMPONENT_ID in config:
        ble_uart_obj = await cg.get_variable(config[CONF_BLE_UART_COMPONENT_ID])
        cg.add(var.set_ble_uart_component(ble_uart_obj))

    # Optional number of motor pole pairs (for erpm <-> rpm conversion)
    cg.add(var.set_motor_pole_pairs(config[CONF_POLE_PAIRS]))

    # This tells the compiler to look inside the current component folder for headers like helpers.h
    cg.add_build_flag("-I" + os.path.dirname(__file__))
