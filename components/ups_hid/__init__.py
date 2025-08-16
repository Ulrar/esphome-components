"""UPS HID Component for ESPHome - Enhanced validation and configuration."""

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
CONF_UPS_HID_ID = "ups_hid_id"

# Known UPS vendor IDs for validation
KNOWN_VENDOR_IDS = {
    0x0463: "MGE Office Protection Systems",
    0x050D: "Belkin",
    0x051D: "APC",
    0x05B8: "SVEN",
    0x0665: "Cypress/Belkin",
    0x06DA: "MGE UPS Systems",
    0x0764: "CyberPower",
    0x0925: "Richcomm",
    0x09AE: "Tripp Lite",
    0x09D6: "Micropower",
}

ups_hid_ns = cg.esphome_ns.namespace("ups_hid")
UpsHidComponent = ups_hid_ns.class_("UpsHidComponent", cg.PollingComponent)


def validate_usb_config(config):
    """Validate USB configuration with helpful warnings."""
    vendor_id = config.get(CONF_USB_VENDOR_ID, 0x051D)
    product_id = config.get(CONF_USB_PRODUCT_ID, 0x0002)

    # Validate non-zero IDs
    if vendor_id == 0 or product_id == 0:
        raise cv.Invalid("USB vendor and product IDs must be non-zero")

    # Warn about unknown vendor IDs
    if vendor_id not in KNOWN_VENDOR_IDS:
        raise cv.Invalid("Unknown vendor id")

    return config


def validate_protocol_timeout(value):
    """Validate protocol timeout with reasonable bounds."""
    value = cv.positive_time_period_milliseconds(value)
    ms = value.total_milliseconds

    if ms < 5000:
        raise cv.Invalid(
            "Protocol timeout must be at least 5 seconds for reliable communication"
        )
    if ms > 60000:
        raise cv.Invalid(
            "Protocol timeout must be at most 60 seconds to avoid blocking"
        )

    return value


def validate_update_interval(value):
    """Validate update interval for UPS HID monitoring."""
    value = cv.update_interval(value)
    ms = value.total_milliseconds

    if ms < 5000:
        cg.global_ns.logger.LOGGER.warning(
            "Update interval less than 5 seconds may cause excessive USB traffic. "
            "Consider using 10s or higher for production."
        )

    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UpsHidComponent),
            cv.Optional(CONF_SIMULATION_MODE, default=False): cv.boolean,
            cv.Optional(CONF_USB_VENDOR_ID, default=0x051D): cv.hex_uint16_t,
            cv.Optional(CONF_USB_PRODUCT_ID, default=0x0002): cv.hex_uint16_t,
            cv.Optional(
                CONF_PROTOCOL_TIMEOUT, default="15s"
            ): validate_protocol_timeout,
            cv.Optional(CONF_AUTO_DETECT_PROTOCOL, default=True): cv.boolean,
        }
    ).extend(cv.polling_component_schema("30s"))
     .extend(cv.COMPONENT_SCHEMA),
    validate_usb_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_simulation_mode(config[CONF_SIMULATION_MODE]))
    cg.add(var.set_usb_vendor_id(config[CONF_USB_VENDOR_ID]))
    cg.add(var.set_usb_product_id(config[CONF_USB_PRODUCT_ID]))
    cg.add(var.set_protocol_timeout(config[CONF_PROTOCOL_TIMEOUT]))
    cg.add(var.set_auto_detect_protocol(config[CONF_AUTO_DETECT_PROTOCOL]))
