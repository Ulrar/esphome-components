#include "nut_ups.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <cstdlib>
#include <cctype>

namespace esphome
{
  namespace nut_ups
  {

    static const char *const APC_TAG = "nut_ups.apc";

    // APC Smart Protocol Commands
    static const char APC_CMD_STATUS = 'Q';         // Status inquiry
    static const char APC_CMD_BATTERY_LEVEL = 'f';  // Battery level
    static const char APC_CMD_INPUT_VOLTAGE = 'L';  // Input line voltage
    static const char APC_CMD_OUTPUT_VOLTAGE = 'O'; // Output voltage
    static const char APC_CMD_LOAD = 'P';           // Output load in %
    static const char APC_CMD_RUNTIME = 'j';        // Runtime remaining
    static const char APC_CMD_FREQUENCY = 'F';      // Line frequency
    static const char APC_CMD_MODEL = '\x01';       // UPS model
    static const char APC_CMD_FIRMWARE = 'V';       // Firmware version
    static const char APC_CMD_SERIAL = 'n';         // Serial number
    static const char APC_CMD_SELFTEST = 'A';       // Self test

    // Helper function to safely parse float from string
    static bool parse_float_safe(const std::string &str, float &result)
    {
      if (str.empty())
      {
        return false;
      }

      // Simple validation - check if string contains valid float characters
      bool has_digit = false;
      bool has_dot = false;
      size_t start_pos = 0;

      // Skip leading whitespace
      while (start_pos < str.length() && std::isspace(str[start_pos]))
      {
        start_pos++;
      }

      if (start_pos >= str.length())
      {
        return false;
      }

      // Check for optional negative sign
      if (str[start_pos] == '-' || str[start_pos] == '+')
      {
        start_pos++;
      }

      // Validate characters
      for (size_t i = start_pos; i < str.length(); i++)
      {
        char c = str[i];
        if (std::isdigit(c))
        {
          has_digit = true;
        }
        else if (c == '.' && !has_dot)
        {
          has_dot = true;
        }
        else if (std::isspace(c))
        {
          // Allow trailing whitespace
          for (size_t j = i; j < str.length(); j++)
          {
            if (!std::isspace(str[j]))
            {
              return false;
            }
          }
          break;
        }
        else
        {
          return false;
        }
      }

      if (!has_digit)
      {
        return false;
      }

      // Use atof for conversion (doesn't throw exceptions)
      char *endptr = nullptr;
      const char *cstr = str.c_str();
      result = strtof(cstr, &endptr);

      // Check if conversion was successful
      if (endptr == cstr || (*endptr != '\0' && !std::isspace(*endptr)))
      {
        return false;
      }

      return true;
    }

    // Helper function to safely parse unsigned long from string (for hex parsing)
    static bool parse_hex_safe(const std::string &str, uint8_t &result)
    {
      if (str.empty() || str.length() > 2)
      {
        return false;
      }

      // Validate hex characters
      for (char c : str)
      {
        if (!std::isxdigit(c))
        {
          return false;
        }
      }

      char *endptr = nullptr;
      unsigned long val = strtoul(str.c_str(), &endptr, 16);

      if (endptr == str.c_str() || *endptr != '\0' || val > 0xFF)
      {
        return false;
      }

      result = static_cast<uint8_t>(val);
      return true;
    }

    bool ApcSmartProtocol::detect()
    {
      ESP_LOGD(APC_TAG, "Detecting APC Smart Protocol...");

      // Try to get a response from the UPS with a simple status command
      std::string response;
      if (send_smart_command(APC_CMD_STATUS, response))
      {
        ESP_LOGD(APC_TAG, "APC response received: %s", response.c_str());
        return true;
      }

      ESP_LOGD(APC_TAG, "No APC Smart Protocol response");
      return false;
    }

    bool ApcSmartProtocol::initialize()
    {
      ESP_LOGD(APC_TAG, "Initializing APC Smart Protocol...");

      // Get basic device information
      std::string model, firmware, serial;

      if (send_smart_command(APC_CMD_MODEL, model))
      {
        ESP_LOGD(APC_TAG, "UPS Model: %s", model.c_str());
      }

      if (send_smart_command(APC_CMD_FIRMWARE, firmware))
      {
        ESP_LOGD(APC_TAG, "Firmware: %s", firmware.c_str());
      }

      if (send_smart_command(APC_CMD_SERIAL, serial))
      {
        ESP_LOGD(APC_TAG, "Serial: %s", serial.c_str());
      }

      ESP_LOGI(APC_TAG, "APC Smart Protocol initialized successfully");
      return true;
    }

    bool ApcSmartProtocol::read_data(UpsData &data)
    {
      ESP_LOGV(APC_TAG, "Reading APC UPS data...");

      std::string response;
      bool success = true;

      // Read status flags first
      if (send_smart_command(APC_CMD_STATUS, response))
      {
        data.status_flags = parse_status_flags(response);
        ESP_LOGV(APC_TAG, "Status: %s (flags: 0x%08X)", response.c_str(), data.status_flags);
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read status");
        success = false;
      }

      // Read battery level
      if (send_smart_command(APC_CMD_BATTERY_LEVEL, response))
      {
        data.battery_level = parse_percentage(response);
        ESP_LOGV(APC_TAG, "Battery Level: %.1f%%", data.battery_level);
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read battery level");
        data.battery_level = NAN;
      }

      // Read input voltage
      if (send_smart_command(APC_CMD_INPUT_VOLTAGE, response))
      {
        data.input_voltage = parse_voltage(response);
        ESP_LOGV(APC_TAG, "Input Voltage: %.1fV", data.input_voltage);
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read input voltage");
        data.input_voltage = NAN;
      }

      // Read output voltage
      if (send_smart_command(APC_CMD_OUTPUT_VOLTAGE, response))
      {
        data.output_voltage = parse_voltage(response);
        ESP_LOGV(APC_TAG, "Output Voltage: %.1fV", data.output_voltage);
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read output voltage");
        data.output_voltage = NAN;
      }

      // Read load percentage
      if (send_smart_command(APC_CMD_LOAD, response))
      {
        data.load_percent = parse_percentage(response);
        ESP_LOGV(APC_TAG, "Load: %.1f%%", data.load_percent);
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read load");
        data.load_percent = NAN;
      }

      // Read runtime
      if (send_smart_command(APC_CMD_RUNTIME, response))
      {
        // Runtime is typically returned in minutes
        if (!response.empty())
        {
          float runtime;
          if (parse_float_safe(response, runtime))
          {
            data.runtime_minutes = runtime;
            ESP_LOGV(APC_TAG, "Runtime: %.1f min", data.runtime_minutes);
          }
          else
          {
            ESP_LOGW(APC_TAG, "Failed to parse runtime: %s", response.c_str());
            data.runtime_minutes = NAN;
          }
        }
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read runtime");
        data.runtime_minutes = NAN;
      }

      // Read frequency
      if (send_smart_command(APC_CMD_FREQUENCY, response))
      {
        if (!response.empty())
        {
          float frequency;
          if (parse_float_safe(response, frequency))
          {
            data.frequency = frequency;
            ESP_LOGV(APC_TAG, "Frequency: %.1f Hz", data.frequency);
          }
          else
          {
            ESP_LOGW(APC_TAG, "Failed to parse frequency: %s", response.c_str());
            data.frequency = NAN;
          }
        }
      }
      else
      {
        ESP_LOGW(APC_TAG, "Failed to read frequency");
        data.frequency = NAN;
      }

      // Read device information (less frequently updated)
      static uint32_t last_info_read = 0;
      uint32_t now = millis();

      if (now - last_info_read > 60000)
      { // Update every 60 seconds
        if (send_smart_command(APC_CMD_MODEL, response))
        {
          data.model = response;
          data.manufacturer = "APC";
        }

        if (send_smart_command(APC_CMD_FIRMWARE, response))
        {
          data.firmware_version = response;
        }

        if (send_smart_command(APC_CMD_SERIAL, response))
        {
          data.serial_number = response;
        }

        last_info_read = now;
      }

      return success;
    }

    bool ApcSmartProtocol::send_smart_command(char cmd, std::string &response)
    {
      ESP_LOGVV(APC_TAG, "Sending APC command: 0x%02X ('%c')", cmd, cmd);

      std::vector<uint8_t> command = {static_cast<uint8_t>(cmd)};
      std::vector<uint8_t> raw_response;

      if (!send_command(command, raw_response, 2000))
      {
        ESP_LOGW(APC_TAG, "Failed to send command 0x%02X", cmd);
        return false;
      }

      response = bytes_to_string(raw_response);

      // Remove any trailing newlines or carriage returns
      while (!response.empty() && (response.back() == '\n' || response.back() == '\r'))
      {
        response.pop_back();
      }

      ESP_LOGVV(APC_TAG, "Command 0x%02X response: '%s'", cmd, response.c_str());
      return true;
    }

    float ApcSmartProtocol::parse_voltage(const std::string &response)
    {
      if (response.empty())
      {
        return NAN;
      }

      float voltage;
      if (!parse_float_safe(response, voltage))
      {
        ESP_LOGW(APC_TAG, "Failed to parse voltage: '%s'", response.c_str());
        return NAN;
      }

      // Sanity check - typical UPS voltages are between 80-300V
      if (voltage >= 80.0f && voltage <= 300.0f)
      {
        return voltage;
      }
      else
      {
        ESP_LOGW(APC_TAG, "Voltage value out of range: %.1f", voltage);
        return NAN;
      }
    }

    float ApcSmartProtocol::parse_percentage(const std::string &response)
    {
      if (response.empty())
      {
        return NAN;
      }

      float percentage;
      if (!parse_float_safe(response, percentage))
      {
        ESP_LOGW(APC_TAG, "Failed to parse percentage: '%s'", response.c_str());
        return NAN;
      }

      // Sanity check - percentages should be 0-100
      if (percentage >= 0.0f && percentage <= 100.0f)
      {
        return percentage;
      }
      else
      {
        ESP_LOGW(APC_TAG, "Percentage value out of range: %.1f", percentage);
        return std::max(0.0f, std::min(100.0f, percentage)); // Clamp to valid range
      }
    }

    uint32_t ApcSmartProtocol::parse_status_flags(const std::string &response)
    {
      if (response.empty())
      {
        return UPS_STATUS_UNKNOWN;
      }

      uint32_t flags = 0;

      // APC Smart Protocol status response format varies by model
      // Common responses include status bits or text responses

      // Check for common status indicators in the response
      std::string upper_response = response;
      std::transform(upper_response.begin(), upper_response.end(), upper_response.begin(), ::toupper);

      // Parse hex status byte if present (some APC models return hex status)
      if (response.length() == 2 || response.length() == 1)
      {
        uint8_t status_byte;
        if (parse_hex_safe(response, status_byte))
        {
          // APC status bits (varies by model, these are common interpretations)
          if (status_byte & 0x08)
            flags |= UPS_STATUS_ONLINE;
          if (status_byte & 0x10)
            flags |= UPS_STATUS_ON_BATTERY;
          if (status_byte & 0x01)
            flags |= UPS_STATUS_LOW_BATTERY;
          if (status_byte & 0x40)
            flags |= UPS_STATUS_REPLACE_BATTERY;
          if (status_byte & 0x04)
            flags |= UPS_STATUS_CHARGING;

          return flags;
        }
        // If hex parsing failed, continue with text parsing
      }

      // Parse text-based status responses
      if (upper_response.find("ONLINE") != std::string::npos)
      {
        flags |= UPS_STATUS_ONLINE;
      }

      if (upper_response.find("ONBATT") != std::string::npos ||
          upper_response.find("ON BATTERY") != std::string::npos)
      {
        flags |= UPS_STATUS_ON_BATTERY;
      }

      if (upper_response.find("LOWBATT") != std::string::npos ||
          upper_response.find("LOW BATTERY") != std::string::npos)
      {
        flags |= UPS_STATUS_LOW_BATTERY;
      }

      if (upper_response.find("CHARGING") != std::string::npos)
      {
        flags |= UPS_STATUS_CHARGING;
      }

      if (upper_response.find("REPLACE") != std::string::npos)
      {
        flags |= UPS_STATUS_REPLACE_BATTERY;
      }

      if (upper_response.find("OVERLOAD") != std::string::npos)
      {
        flags |= UPS_STATUS_OVERLOAD;
      }

      if (upper_response.find("FAULT") != std::string::npos ||
          upper_response.find("ERROR") != std::string::npos)
      {
        flags |= UPS_STATUS_FAULT;
      }

      // If no status detected but we got a response, assume online
      if (flags == 0 && !response.empty())
      {
        flags |= UPS_STATUS_ONLINE;
      }

      return flags;
    }

  } // namespace nut_ups
} // namespace esphome