"""
ESPHome component config schema for ble_uart_bridge.

THE YAML/PYTHON MYSTERY DEMYSTIFIED
=====================================
ESPHome has a two-step build process:

  1. VALIDATION  — Python reads your .yaml, validates it against the schema
                   defined here in __init__.py (or config_flow.py).
                   If you typo a key or give a wrong type, it errors here.

  2. CODE GEN    — Python generates C++ code that calls your component's
                   constructor and setter methods. The `to_code()` function
                   below is what actually emits that C++ glue code.

So the Python here is NOT runtime code — it's a code GENERATOR.
Think of it as: YAML → Python → C++ → compiled firmware.

WHAT cv.Schema DOES
====================
cv = config_validation (imported as cv)
It's ESPHome's type-checking system for YAML values.

  cv.string          → must be a string
  cv.Required(...)   → key must be present
  cv.Optional(...)   → key may be absent (with a default)
  uart.UART_DEVICE_SCHEMA → injects the 'uart_id' key automatically
                             so your component can find the UART bus

WHAT cg.* DOES
===============
cg = code_generator
It emits C++ expressions into the generated main.cpp.

  cg.new_Pvariable(config[CONF_ID], ...)
    → emits:  auto *my_var = new BleUartBridge();

  await cg.register_component(var, config)
    → tells ESPHome this is a Component (has setup()/loop())
    → emits the call to App.register_component(my_var)

  await uart.register_uart_device(var, config)
    → wires up the UART bus to this component
    → emits:  my_var->set_uart_parent(uart0);

  cg.add(var.set_device_name("VESC BLE Bridge"))
    → emits:  my_var->set_device_name("VESC BLE Bridge");
    → this is how YAML values become C++ constructor arguments
"""

import esphome.codegen as cg  # C++ code generator
import esphome.config_validation as cv  # YAML validator
from esphome.components import uart  # For the UART mixin
from esphome.const import CONF_ID  # The 'id:' key every component has

# Declare our C++ namespace and class.
# This must exactly match the namespace/class in your .h file.
ble_uart_component_ns = cg.esphome_ns.namespace("ble_uart_component")
BleUartComponent = ble_uart_component_ns.class_(
    "BleUartComponent",
    cg.Component,  # inherits esphome::Component
    uart.UARTDevice,  # inherits esphome::uart::UARTDevice
)

# CONF_ constants are just strings, but using constants avoids typos
CONF_DEVICE_NAME = "device_name"

# This is the schema for your component's YAML block.
# uart.UART_DEVICE_SCHEMA adds 'uart_id' automatically.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(
            BleUartComponent
        ),  # auto-generate C++ variable name if not given
        cv.Optional(CONF_DEVICE_NAME, default="VESC BLE UART Bridge"): cv.string,
    }
).extend(
    uart.UART_DEVICE_SCHEMA
)  # ← this bolt-on adds the 'uart_id' key


# to_code is called by ESPHome's build system to EMIT C++ code.
# 'config' is a dict of your validated YAML values.
# 'await' is needed because ESPHome's code gen is async.
async def to_code(config):

    # This tells the compiler to include the BLE library (for BLEDevice.h, etc.)
    cg.add_library("BLE", None)  # Bluedroid
    # cg.add_library("h2zero/NimBLE-Arduino", "1.4.1")

    cg.add_library("Preferences", None)  # used by Bluedroid

    # These tell the compiler to include the right header files for this component
    cg.add_build_flag("-DCONFIG_BT_ENABLED")
    # cg.add_build_flag("-DCONFIG_BLUEDROID_ENABLED")
    cg.add_build_flag("-DCONFIG_BT_BLUEDROID_ENABLED")

    # Instantiate the C++ object and get a handle to it
    var = cg.new_Pvariable(config[CONF_ID])

    # Register it as a Component (adds to the App's component list)
    await cg.register_component(var, config)

    # Wire up the UART bus (uses the uart_id from YAML)
    await uart.register_uart_device(var, config)

    # Pass the device_name from YAML into C++:
    # Emits: var->set_device_name("whatever you put in YAML");
    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
