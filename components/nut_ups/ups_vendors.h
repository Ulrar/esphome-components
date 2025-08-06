#pragma once

/**
 * @file ups_vendors.h
 * @brief Known UPS vendor IDs for device identification and validation
 * 
 * This file contains the definitive list of known UPS vendor IDs
 * shared between C++ runtime code and Python configuration validation.
 */

namespace esphome {
namespace nut_ups {

// Known UPS vendor IDs with their names
struct UpsVendor {
  uint16_t vendor_id;
  const char* name;
  const char* description;
};

// Definitive list of known UPS vendors
static constexpr UpsVendor KNOWN_UPS_VENDORS[] = {
  {0x051D, "APC", "American Power Conversion"},
  {0x0764, "CyberPower", "CyberPower Systems"},
  {0x09AE, "Tripp Lite", "Tripp Lite"},
  {0x06DA, "MGE UPS Systems", "MGE UPS Systems (now Eaton)"},
  {0x0665, "Cypress/Other", "Cypress or other UPS devices"},
  {0x0463, "MGE Office Protection Systems", "MGE Office Protection Systems"},
  {0x06da, "Phoenixtec Power", "Phoenixtec Power Co., Ltd."},
  {0x0001, "Fiskars", "Fiskars Corporation (some UPS models)"},
};

static constexpr size_t KNOWN_UPS_VENDORS_COUNT = sizeof(KNOWN_UPS_VENDORS) / sizeof(KNOWN_UPS_VENDORS[0]);

/**
 * Check if a vendor ID is in the known UPS vendors list
 * @param vendor_id The USB vendor ID to check
 * @return True if vendor ID is known, false otherwise
 */
inline bool is_known_ups_vendor(uint16_t vendor_id) {
  for (size_t i = 0; i < KNOWN_UPS_VENDORS_COUNT; i++) {
    if (KNOWN_UPS_VENDORS[i].vendor_id == vendor_id) {
      return true;
    }
  }
  return false;
}

/**
 * Get vendor name for a given vendor ID
 * @param vendor_id The USB vendor ID
 * @return Vendor name or nullptr if unknown
 */
inline const char* get_ups_vendor_name(uint16_t vendor_id) {
  for (size_t i = 0; i < KNOWN_UPS_VENDORS_COUNT; i++) {
    if (KNOWN_UPS_VENDORS[i].vendor_id == vendor_id) {
      return KNOWN_UPS_VENDORS[i].name;
    }
  }
  return nullptr;
}

} // namespace nut_ups
} // namespace esphome