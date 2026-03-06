import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import number
from . import vesc_component_ns, VescComponent

# Use the ID from __init__.py to link numbers
CONF_VESC_ID = "vesc_id"

VescControlRpm = vesc_component_ns.class_("VescControlRpm", number.Number)
VescControlDutyCycle = vesc_component_ns.class_("VescControlDutyCycle", number.Number)
VescControlCurrent = vesc_component_ns.class_("VescControlCurrent", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VESC_ID): cv.use_id(VescComponent),
        cv.Optional("rpm_control"): number.number_schema(
            VescControlRpm,
        ).extend(
            {
                cv.GenerateID(): cv.declare_id(VescControlRpm),
            }
        ),
        cv.Optional("current_control"): number.number_schema(
            VescControlCurrent,
        ).extend(
            {
                cv.GenerateID(): cv.declare_id(VescControlCurrent),
            }
        ),
        cv.Optional("duty_cycle_control"): number.number_schema(
            VescControlDutyCycle,
        ).extend(
            {
                cv.GenerateID(): cv.declare_id(VescControlDutyCycle),
            }
        ),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_VESC_ID])

    if "rpm_control" in config:
        num = await number.new_number(
            config["rpm_control"],
            min_value=0,
            max_value=476,
            step=1,
        )
        cg.add(var.set_rpm_control(num))
    if "current_control" in config:
        num = await number.new_number(
            config["current_control"],
            min_value=0,
            max_value=1.25,
            step=0.005,
        )
        cg.add(var.set_current_control(num))
    if "duty_cycle_control" in config:
        num = await number.new_number(
            config["duty_cycle_control"],
            min_value=0,
            max_value=0.5,
            step=0.005,
        )
        cg.add(var.set_duty_cycle_control(num))
