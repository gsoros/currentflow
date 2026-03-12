import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_VESC_ID, VescComponent

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("control_mode"): text_sensor.text_sensor_schema(
            icon="mdi:electric-switch",
        ),
        cv.Optional("fault_text"): text_sensor.text_sensor_schema(
            icon="mdi:alert-circle",
        ),
        cv.Optional("lisp_print"): text_sensor.text_sensor_schema(
            icon="mdi:script-text",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_VESC_ID])
    if "control_mode" in config:
        conf = config["control_mode"]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.set_control_mode_sensor(sens))
    if "fault_text" in config:
        sens = await text_sensor.new_text_sensor(config["fault_text"])
        cg.add(var.set_fault_text_sensor(sens))
    if "lisp_print" in config:
        sens = await text_sensor.new_text_sensor(config["lisp_print"])
        cg.add(var.set_lisp_print_sensor(sens))
