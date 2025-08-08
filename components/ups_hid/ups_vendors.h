#pragma once

/**
 * @file ups_vendors.h
 * @brief Known UPS vendor IDs for device identification and validation
 *
 * This file contains the definitive list of known UPS vendor IDs
 * shared between C++ runtime code and Python configuration validation.
 */

namespace esphome
{
  namespace ups_hid
  {

    // Known UPS vendor IDs with their names
    struct UpsVendor
    {
      uint16_t vendor_id;
      const char *name;
      const char *description;
    };

    // Definitive list of known UPS vendors (based on NUT project driver.list)
    static constexpr UpsVendor KNOWN_UPS_VENDORS[] = {
        //{0x0001, "Generic/Other", "Generic UPS vendors (multiple brands)"},
        {0x0463, "MGE Office Protection Systems", "MGE Office Protection Systems"},
        {0x050D, "Belkin", "Belkin (older USB UPS models)"},
        {0x051D, "APC", "American Power Conversion"},
        {0x05B8, "SVEN", "SVEN UPS devices"},
        {0x0665, "Cypress/Belkin", "Cypress or Belkin UPS devices"},
        {0x06DA, "MGE UPS Systems", "MGE UPS Systems (now Eaton)"},
        {0x0764, "CyberPower", "CyberPower Systems"},
        {0x0925, "Richcomm", "Richcomm Technologies (Digitus, Sweex)"},
        {0x09AE, "Tripp Lite", "Tripp Lite"},
        {0x09D6, "Micropower", "Micropower UPS devices"},
    };

    static constexpr size_t KNOWN_UPS_VENDORS_COUNT = sizeof(KNOWN_UPS_VENDORS) / sizeof(KNOWN_UPS_VENDORS[0]);

    /**
     * Check if a vendor ID is in the known UPS vendors list
     * @param vendor_id The USB vendor ID to check
     * @return True if vendor ID is known, false otherwise
     */
    inline bool is_known_ups_vendor(uint16_t vendor_id)
    {
      for (size_t i = 0; i < KNOWN_UPS_VENDORS_COUNT; i++)
      {
        if (KNOWN_UPS_VENDORS[i].vendor_id == vendor_id)
        {
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
    inline const char *get_ups_vendor_name(uint16_t vendor_id)
    {
      for (size_t i = 0; i < KNOWN_UPS_VENDORS_COUNT; i++)
      {
        if (KNOWN_UPS_VENDORS[i].vendor_id == vendor_id)
        {
          return KNOWN_UPS_VENDORS[i].name;
        }
      }
      return nullptr;
    }

  } // namespace ups_hid
} // namespace esphome