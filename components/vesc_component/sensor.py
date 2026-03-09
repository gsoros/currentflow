import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import UNIT_VOLT, UNIT_AMPERE, STATE_CLASS_MEASUREMENT
from . import vesc_component_ns, VescComponent

# Use the ID from __init__.py to link sensors
CONF_VESC_ID = "vesc_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("voltage"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("rpm"): sensor.sensor_schema(
            unit_of_measurement="RPM",
            icon="mdi:engine",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("duty_cycle"): sensor.sensor_schema(
            unit_of_measurement="%",
            icon="mdi:engine",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("input_current"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon="mdi:engine",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("phase_current"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            icon="mdi:engine",
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fet_temp"): sensor.sensor_schema(
            unit_of_measurement="°C",
            icon="mdi:engine",
            accuracy_decimals=1,
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
    if "duty_cycle" in config:
        sens = await sensor.new_sensor(config["duty_cycle"])
        cg.add(var.set_duty_cycle_sensor(sens))
    if "input_current" in config:
        sens = await sensor.new_sensor(config["input_current"])
        cg.add(var.set_input_current_sensor(sens))
    if "phase_current" in config:
        sens = await sensor.new_sensor(config["phase_current"])
        cg.add(var.set_phase_current_sensor(sens))
    if "fet_temp" in config:
        sens = await sensor.new_sensor(config["fet_temp"])
        cg.add(var.set_fet_temp_sensor(sens))
