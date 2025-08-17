import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_EMPTY,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_MINUTE,
    UNIT_HERTZ,
    UNIT_WATT,
    UNIT_SECOND,
)

# Handle STATE_CLASS_MEASUREMENT import for different ESPHome versions
try:
    from esphome.const import STATE_CLASS_MEASUREMENT
except ImportError:
    from esphome.components.sensor import StateClass
    STATE_CLASS_MEASUREMENT = StateClass.MEASUREMENT

from . import ups_hid_ns, UpsHidComponent, CONF_UPS_HID_ID

DEPENDENCIES = ["ups_hid"]

UpsHidSensor = ups_hid_ns.class_("UpsHidSensor", sensor.Sensor, cg.Component)

SENSOR_TYPES = {
    "battery_level": {
        "unit": UNIT_PERCENT,
        "device_class": DEVICE_CLASS_BATTERY,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "input_voltage": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "output_voltage": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "load_percent": {
        "unit": UNIT_PERCENT,
        "device_class": DEVICE_CLASS_POWER_FACTOR,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "runtime": {
        "unit": UNIT_MINUTE,
        "device_class": DEVICE_CLASS_DURATION,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "frequency": {
        "unit": UNIT_HERTZ,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "battery_voltage": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "battery_voltage_nominal": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "input_voltage_nominal": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "input_transfer_low": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "input_transfer_high": {
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "ups_realpower_nominal": {
        "unit": UNIT_WATT,
        "device_class": DEVICE_CLASS_POWER,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "ups_delay_shutdown": {
        "unit": UNIT_SECOND,
        "device_class": DEVICE_CLASS_DURATION,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "ups_delay_start": {
        "unit": UNIT_SECOND,
        "device_class": DEVICE_CLASS_DURATION,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
}


CONFIG_SCHEMA = sensor.sensor_schema(
    UpsHidSensor,
    accuracy_decimals=1,
).extend(
    {
        cv.GenerateID(CONF_UPS_HID_ID): cv.use_id(UpsHidComponent),
        cv.Required(CONF_TYPE): cv.one_of(*SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_UPS_HID_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sensor_type = config[CONF_TYPE]
    cg.add(var.set_sensor_type(sensor_type))
    cg.add(parent.register_sensor(var, sensor_type))

    # Apply sensor type specific configuration
    if sensor_type in SENSOR_TYPES:
        sensor_config = SENSOR_TYPES[sensor_type]

        # Override config with sensor type defaults if not specified
        if "unit_of_measurement" not in config and "unit" in sensor_config:
            cg.add(var.set_unit_of_measurement(sensor_config["unit"]))

        if "device_class" not in config and "device_class" in sensor_config:
            cg.add(var.set_device_class(sensor_config["device_class"]))

        if "state_class" not in config and "state_class" in sensor_config:
            cg.add(var.set_state_class(sensor_config["state_class"]))

        if "accuracy_decimals" not in config and "accuracy_decimals" in sensor_config:
            cg.add(var.set_accuracy_decimals(sensor_config["accuracy_decimals"]))
