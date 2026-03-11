import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import VescComponent

# Use the ID from __init__.py to link sensors
CONF_VESC_ID = "vesc_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("control_mode"): text_sensor.text_sensor_schema(
            icon="mdi:electric-switch",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_VESC_ID])
    if "control_mode" in config:
        conf = config["control_mode"]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.set_control_mode_sensor(sens))
