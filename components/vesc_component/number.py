import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

from . import VescComponent, vesc_component_ns

# Use the ID from __init__.py to link numbers
CONF_VESC_ID = "vesc_id"

VescControlRpm = vesc_component_ns.class_("VescControlRpm", number.Number)
VescControlDuty = vesc_component_ns.class_("VescControlDuty", number.Number)
VescControlCurrent = vesc_component_ns.class_("VescControlCurrent", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("rpm_control"): number.number_schema(
            VescControlRpm,
        ).extend(
            {
                cv.Optional("min_value", default=-500.0): cv.float_,
                cv.Optional("max_value", default=500.0): cv.float_,
                cv.Optional("step", default=5.0): cv.float_,
            }
        ),
        cv.Optional("current_control"): number.number_schema(
            VescControlCurrent,
        ).extend(
            {
                cv.Optional("min_value", default=-10.0): cv.float_,
                cv.Optional("max_value", default=10.0): cv.float_,
                cv.Optional("step", default=0.1): cv.float_,
            }
        ),
        cv.Optional("duty_control"): number.number_schema(
            VescControlDuty,
        ).extend(
            {
                cv.Optional("min_value", default=-2.0): cv.float_,
                cv.Optional("max_value", default=2.0): cv.float_,
                cv.Optional("step", default=0.02): cv.float_,
            }
        ),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_VESC_ID])

    for key, setter in [
        ("rpm_control", var.set_rpm_control),
        ("current_control", var.set_current_control),
        ("duty_control", var.set_duty_control),
    ]:
        if key in config:
            conf = config[key]
            num = await number.new_number(
                conf,
                min_value=conf[CONF_MIN_VALUE],
                max_value=conf[CONF_MAX_VALUE],
                step=conf[CONF_STEP],
            )
            cg.add(setter(num))
