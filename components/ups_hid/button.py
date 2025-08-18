import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import ups_hid_ns, CONF_UPS_HID_ID, UpsHidComponent

DEPENDENCIES = ["ups_hid"]

UpsHidButton = ups_hid_ns.class_("UpsHidButton", button.Button, cg.Component)

CONF_BEEPER_ACTION = "beeper_action"

BEEPER_ACTIONS = {
    "enable": "enable",
    "disable": "disable", 
    "mute": "mute",
    "test": "test"
}

CONFIG_SCHEMA = button.button_schema(UpsHidButton).extend({
    cv.GenerateID(CONF_UPS_HID_ID): cv.use_id(UpsHidComponent),
    cv.Required(CONF_BEEPER_ACTION): cv.enum(BEEPER_ACTIONS, lower=True),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_UPS_HID_ID])
    cg.add(var.set_ups_hid_parent(parent))
    cg.add(var.set_beeper_action(config[CONF_BEEPER_ACTION]))