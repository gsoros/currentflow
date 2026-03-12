import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import STATE_CLASS_MEASUREMENT, UNIT_AMPERE, UNIT_VOLT, UNIT_WATT

from . import CONF_VESC_ID, VescComponent

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("voltage"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            icon="mdi:flash-triangle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("rpm"): sensor.sensor_schema(
            unit_of_measurement="RPM",
            icon="mdi:rotate-360",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("duty"): sensor.sensor_schema(
            unit_of_measurement="%",
            icon="mdi:hammer",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("input_current"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon="mdi:waves-arrow-left",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("phase_current"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon="mdi:waves-arrow-right",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fet_temp"): sensor.sensor_schema(
            unit_of_measurement="°C",
            icon="mdi:thermometer",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("wattage"): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            icon="mdi:arm-flex",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fault_code"): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_VESC_ID])

    if "voltage" in config:
        sens = await sensor.new_sensor(config["voltage"])
        cg.add(var.set_voltage_sensor(sens))
    if "rpm" in config:
        sens = await sensor.new_sensor(config["rpm"])
        cg.add(var.set_rpm_sensor(sens))
    if "duty" in config:
        sens = await sensor.new_sensor(config["duty"])
        cg.add(var.set_duty_sensor(sens))
    if "input_current" in config:
        sens = await sensor.new_sensor(config["input_current"])
        cg.add(var.set_input_current_sensor(sens))
    if "phase_current" in config:
        sens = await sensor.new_sensor(config["phase_current"])
        cg.add(var.set_phase_current_sensor(sens))
    if "fet_temp" in config:
        sens = await sensor.new_sensor(config["fet_temp"])
        cg.add(var.set_fet_temp_sensor(sens))
    if "wattage" in config:
        sens = await sensor.new_sensor(config["wattage"])
        cg.add(var.set_wattage_sensor(sens))
    if "fault_code" in config:
        sens = await sensor.new_sensor(config["fault_code"])
        cg.add(var.set_fault_code_sensor(sens))
