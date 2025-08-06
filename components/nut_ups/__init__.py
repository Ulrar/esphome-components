import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_SIMULATION_MODE = "simulation_mode"
CONF_USB_VENDOR_ID = "usb_vendor_id"
CONF_USB_PRODUCT_ID = "usb_product_id"
CONF_PROTOCOL_TIMEOUT = "protocol_timeout"
CONF_AUTO_DETECT_PROTOCOL = "auto_detect_protocol"
CONF_NUT_UPS_ID = "nut_ups_id"

nut_ups_ns = cg.esphome_ns.namespace("nut_ups")
NutUpsComponent = nut_ups_ns.class_("NutUpsComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NutUpsComponent),
        cv.Optional(CONF_UPDATE_INTERVAL, default="30s"): cv.update_interval,
        cv.Optional(CONF_SIMULATION_MODE, default=False): cv.boolean,
        cv.Optional(CONF_USB_VENDOR_ID, default=0x051D): cv.hex_uint16_t,
        cv.Optional(CONF_USB_PRODUCT_ID, default=0x0002): cv.hex_uint16_t,
        cv.Optional(CONF_PROTOCOL_TIMEOUT, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_AUTO_DETECT_PROTOCOL, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_simulation_mode(config[CONF_SIMULATION_MODE]))
    cg.add(var.set_usb_vendor_id(config[CONF_USB_VENDOR_ID]))
    cg.add(var.set_usb_product_id(config[CONF_USB_PRODUCT_ID]))
    cg.add(var.set_protocol_timeout(config[CONF_PROTOCOL_TIMEOUT]))
    cg.add(var.set_auto_detect_protocol(config[CONF_AUTO_DETECT_PROTOCOL]))