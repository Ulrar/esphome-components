import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_POWER,
)

from . import nut_ups_ns, NutUpsComponent, CONF_NUT_UPS_ID

DEPENDENCIES = ["nut_ups"]

NutUpsBinarySensor = nut_ups_ns.class_("NutUpsBinarySensor", binary_sensor.BinarySensor, cg.Component)

BINARY_SENSOR_TYPES = {
    "online": {
        "device_class": DEVICE_CLASS_CONNECTIVITY,
    },
    "on_battery": {
        "device_class": DEVICE_CLASS_BATTERY,
    },
    "low_battery": {
        "device_class": DEVICE_CLASS_BATTERY,
    },
    "fault": {
        "device_class": DEVICE_CLASS_PROBLEM,
    },
    "overload": {
        "device_class": DEVICE_CLASS_POWER,
    },
    "charging": {
        "device_class": DEVICE_CLASS_BATTERY,
    },
}


CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(NutUpsBinarySensor).extend(
    {
        cv.GenerateID(CONF_NUT_UPS_ID): cv.use_id(NutUpsComponent),
        cv.Required(CONF_TYPE): cv.one_of(*BINARY_SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_NUT_UPS_ID])
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    sensor_type = config[CONF_TYPE]
    cg.add(var.set_sensor_type(sensor_type))
    cg.add(parent.register_binary_sensor(var, sensor_type))

    # Apply sensor type specific configuration
    if sensor_type in BINARY_SENSOR_TYPES:
        sensor_config = BINARY_SENSOR_TYPES[sensor_type]

        # Override config with sensor type defaults if not specified
        if "device_class" not in config and "device_class" in sensor_config:
            cg.add(var.set_device_class(sensor_config["device_class"]))