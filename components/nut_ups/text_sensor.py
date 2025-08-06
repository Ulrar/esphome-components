import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
)

from . import nut_ups_ns, NutUpsComponent, CONF_NUT_UPS_ID

DEPENDENCIES = ["nut_ups"]

NutUpsTextSensor = nut_ups_ns.class_("NutUpsTextSensor", text_sensor.TextSensor, cg.Component)

TEXT_SENSOR_TYPES = [
    "model",
    "manufacturer", 
    "status",
    "protocol",
    "serial_number",
    "firmware_version",
]

CONF_NUT_UPS_ID = "nut_ups_id"

CONFIG_SCHEMA = text_sensor.text_sensor_schema(NutUpsTextSensor).extend(
    {
        cv.GenerateID(CONF_NUT_UPS_ID): cv.use_id(NutUpsComponent),
        cv.Required(CONF_TYPE): cv.one_of(*TEXT_SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_NUT_UPS_ID])
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    
    sensor_type = config[CONF_TYPE]
    cg.add(var.set_sensor_type(sensor_type))
    cg.add(parent.register_text_sensor(var, sensor_type))