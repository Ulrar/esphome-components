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

    // Definitive list of known UPS vendors (based on NUT project HID driver support)
    static constexpr UpsVendor KNOWN_UPS_VENDORS[] = {
        // Vendors with dedicated HID drivers and ESPHome implementations
        {0x051D, "APC", "American Power Conversion (dedicated ESPHome protocol)"},
        {0x0764, "CyberPower", "CyberPower Systems (dedicated ESPHome protocol)"},

        // Vendors with dedicated NUT HID drivers (Generic HID protocol support)
        {0x0463, "MGE Office Protection Systems", "MGE Office Protection Systems (Eaton)"},
        {0x047C, "Dell", "Dell UPS devices (via MGE HID driver)"},
        {0x04B3, "IBM", "IBM UPS devices (via MGE HID driver)"},
        {0x04D8, "OpenUPS", "OpenUPS HID devices"},
        {0x050D, "Belkin", "Belkin USB UPS models"},
        {0x0592, "Powerware", "Powerware UPS devices (via MGE HID driver)"},
        {0x05DD, "Delta Electronics", "Delta UPS devices (Tripp Lite OEM)"},
        {0x06DA, "MGE UPS Systems", "MGE UPS Systems / Liebert / Phoenixtec (now Eaton)"},
        {0x075D, "Idowell", "Idowell HID UPS devices"},
        {0x09AE, "Tripp Lite", "Tripp Lite UPS devices"},
        {0x09D6, "KSTAR", "KSTAR/Micropower UPS devices"},

        // STMicroelectronics OEM vendor (used by multiple UPS manufacturers)
        {0x0483, "STMicroelectronics", "STMicro OEM UPS devices (CPS, Ever, others)"},
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