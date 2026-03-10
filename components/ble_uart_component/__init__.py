import esphome.codegen as cg  # C++ code generator
from esphome.components import uart  # For the UART mixin
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv  # YAML validator
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


# AUTO_LOAD = ["esp32_ble"]


# DEPENDENCIES = ["esp32"]


# This is the schema for your component's YAML block.
# uart.UART_DEVICE_SCHEMA adds 'uart_id' automatically.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(
            BleUartComponent
        ),  # auto-generate C++ variable name if not given
        cv.Optional(CONF_DEVICE_NAME, default="VESC BLE UART Bridge"): cv.string,
    }
).extend(uart.UART_DEVICE_SCHEMA)  # ← this bolt-on adds the 'uart_id' key


# to_code is called by ESPHome's build system to EMIT C++ code.
# 'config' is a dict of your validated YAML values.
# 'await' is needed because ESPHome's code gen is async.
async def to_code(config):

    # This tells the compiler to include the BLE library (for BLEDevice.h, etc.)
    # cg.add_library("ESP32 BLE Arduino", None)
    # cg.add_library("BLE", None)  # Arduino-ESP32 bundled BLE
    cg.add_library("h2zero/NimBLE-Arduino", None)

    cg.add_library("Preferences", None)  # used by Bluedroid

    # These tell the compiler to include the right header files for this component
    cg.add_build_flag("-DCONFIG_BT_ENABLED")

    # technically IDF-flavoured even on Arduino framework — it still controls
    # what gets compiled into the BT stack. A bit cheeky but that's how ESPHome rolls.
    # add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ROLE_PERIPHERAL", True)
    # add_idf_sdkconfig_option("CONFIG_NIMBLE_CPP_LOG_LEVEL", 0)  # 0=debug!!!

    # Instantiate the C++ object and get a handle to it
    var = cg.new_Pvariable(config[CONF_ID])

    # Register it as a Component (adds to the App's component list)
    await cg.register_component(var, config)

    # Wire up the UART bus (uses the uart_id from YAML)
    await uart.register_uart_device(var, config)

    # Pass the device_name from YAML into C++:
    # Emits: var->set_device_name("whatever you put in YAML");
    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
