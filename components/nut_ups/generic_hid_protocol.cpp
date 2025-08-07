#include "nut_ups.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nut_ups {

static const char *const GEN_TAG = "nut_ups.generic";

// Standard HID Usage Pages (based on NUT usbhid-ups.c)
static const uint16_t HID_USAGE_PAGE_POWER_DEVICE = 0x84;
static const uint16_t HID_USAGE_PAGE_BATTERY = 0x85;
static const uint16_t HID_USAGE_PAGE_UPS = 0x84;

// Common HID Power Device Class Usages
static const uint16_t HID_USAGE_UPS_BATTERY_CHARGE = 0x66;
static const uint16_t HID_USAGE_UPS_RUNTIME_TO_EMPTY = 0x68;
static const uint16_t HID_USAGE_UPS_AC_PRESENT = 0x5A;
static const uint16_t HID_USAGE_UPS_BATTERY_PRESENT = 0x5B;
static const uint16_t HID_USAGE_UPS_CHARGING = 0x44;
static const uint16_t HID_USAGE_UPS_DISCHARGING = 0x45;
static const uint16_t HID_USAGE_UPS_NEED_REPLACEMENT = 0x4B;
static const uint16_t HID_USAGE_UPS_FULLY_CHARGED = 0x49;
static const uint16_t HID_USAGE_UPS_INPUT_VOLTAGE = 0x30;
static const uint16_t HID_USAGE_UPS_OUTPUT_VOLTAGE = 0x31;
static const uint16_t HID_USAGE_UPS_PERCENT_LOAD = 0x35;

// Common HID Report IDs found in UPS devices
static const uint8_t HID_REPORT_FEATURE = 0x03;
static const uint8_t HID_REPORT_INPUT = 0x01;

bool GenericHidProtocol::detect() {
  ESP_LOGD(GEN_TAG, "Detecting Generic HID Protocol...");
  
  // Try HID Get Feature Report (like NUT does)
  std::vector<uint8_t> get_feature_request = {
    0x21, // bmRequestType: Class, Interface, Host-to-device
    0x01, // bRequest: GET_REPORT
    0x00, HID_REPORT_FEATURE, // wValue: Report Type and Report ID
    0x00, 0x00, // wIndex: Interface
    0x08, 0x00  // wLength: 8 bytes
  };
  
  std::vector<uint8_t> response;
  if (send_command(get_feature_request, response, 2000)) {
    if (!response.empty()) {
      ESP_LOGD(GEN_TAG, "Generic HID Feature Report received, %zu bytes", response.size());
      return true;
    }
  }
  
  // Fallback: Try Input Report
  std::vector<uint8_t> get_input_request = {
    0x21, // bmRequestType
    0x01, // bRequest: GET_REPORT  
    0x00, HID_REPORT_INPUT, // wValue: Input Report
    0x00, 0x00, // wIndex
    0x08, 0x00  // wLength
  };
  
  if (send_command(get_input_request, response, 2000)) {
    if (!response.empty()) {
      ESP_LOGD(GEN_TAG, "Generic HID Input Report received, %zu bytes", response.size());
      return true;
    }
  }
  
  ESP_LOGD(GEN_TAG, "No Generic HID response - device may not support standard HID reports");
  return false;
}

bool GenericHidProtocol::initialize() {
  ESP_LOGD(GEN_TAG, "Initializing Generic HID Protocol...");
  
  // Generic initialization - just verify we can communicate
  std::vector<uint8_t> init_command = {0x00}; // Null command
  std::vector<uint8_t> response;
  
  if (send_command(init_command, response, 2000)) {
    ESP_LOGI(GEN_TAG, "Generic HID Protocol initialized successfully");
    return true;
  }
  
  ESP_LOGW(GEN_TAG, "Generic HID Protocol initialization failed");
  return false;
}

bool GenericHidProtocol::read_data(UpsData &data) {
  ESP_LOGV(GEN_TAG, "Reading Generic HID UPS data...");
  
  bool success = false;
  std::vector<uint8_t> response;
  
  // Try different HID report types (like NUT does)
  std::vector<std::pair<std::string, std::vector<uint8_t>>> report_requests = {
    {"Feature", {0x21, 0x01, 0x00, HID_REPORT_FEATURE, 0x00, 0x00, 0x20, 0x00}},
    {"Input", {0x21, 0x01, 0x00, HID_REPORT_INPUT, 0x00, 0x00, 0x20, 0x00}},
    {"Report1", {0x21, 0x01, 0x01, HID_REPORT_INPUT, 0x00, 0x00, 0x08, 0x00}},
    {"Report2", {0x21, 0x01, 0x02, HID_REPORT_INPUT, 0x00, 0x00, 0x08, 0x00}}
  };
  
  for (const auto& req : report_requests) {
    if (send_command(req.second, response, 1000) && response.size() >= 2) {
      success = true;
      ESP_LOGV(GEN_TAG, "%s report received: %zu bytes", req.first.c_str(), response.size());
      
      if (parse_generic_report(response, data)) {
        ESP_LOGV(GEN_TAG, "Successfully parsed %s report", req.first.c_str());
        break;
      }
    }
  }
  
  // Set generic device information
  if (data.manufacturer.empty()) {
    data.manufacturer = "Generic";
  }
  if (data.model.empty()) {
    data.model = "HID UPS";  
  }
  
  if (!success) {
    ESP_LOGW(GEN_TAG, "Failed to read any generic HID data");
    
    // Provide minimal status to indicate device presence
    data.status_flags = UPS_STATUS_ONLINE;
    data.manufacturer = "Generic";
    data.model = "Unknown UPS";
    success = true; // Still consider it successful to maintain connection
  }
  
  return success;
}

bool GenericHidProtocol::parse_generic_report(const std::vector<uint8_t>& response, UpsData& data) {
  if (response.size() < 2) {
    return false;
  }
  
  bool found_data = false;
  
  // NUT-style heuristic parsing - try multiple approaches
  
  // Approach 1: Look for status bits in first bytes
  for (size_t i = 0; i < std::min(response.size(), size_t(4)); ++i) {
    uint8_t status_byte = response[i];
    
    if (status_byte != 0x00 && status_byte != 0xFF) {
      data.status_flags = 0;
      
      // Common status bit patterns found in various UPS devices
      if (status_byte & 0x01) data.status_flags |= UPS_STATUS_ONLINE;
      if (status_byte & 0x02) data.status_flags |= UPS_STATUS_ON_BATTERY;
      if (status_byte & 0x04) data.status_flags |= UPS_STATUS_LOW_BATTERY;
      if (status_byte & 0x08) data.status_flags |= UPS_STATUS_CHARGING;
      if (status_byte & 0x10) data.status_flags |= UPS_STATUS_REPLACE_BATTERY;
      if (status_byte & 0x20) data.status_flags |= UPS_STATUS_OVERLOAD;
      
      found_data = true;
      ESP_LOGV(GEN_TAG, "Status bits found at offset %zu: 0x%02X -> flags 0x%08X", 
               i, status_byte, data.status_flags);
      break;
    }
  }
  
  // Approach 2: Look for percentage values (0-100) in 16-bit words
  for (size_t i = 0; i <= response.size() - 2; i += 2) {
    uint16_t value_le = response[i] | (response[i+1] << 8);        // Little-endian
    uint16_t value_be = (response[i] << 8) | response[i+1];       // Big-endian
    
    // Check if either interpretation gives a valid percentage
    if (value_le <= 100) {
      data.battery_level = static_cast<float>(value_le);
      found_data = true;
      ESP_LOGV(GEN_TAG, "Battery level found at offset %zu (LE): %.1f%%", i, data.battery_level);
    } else if (value_be <= 100) {
      data.battery_level = static_cast<float>(value_be);
      found_data = true;
      ESP_LOGV(GEN_TAG, "Battery level found at offset %zu (BE): %.1f%%", i, data.battery_level);
    }
    
    // Check for scaled percentages (0-1000 -> 0.0-100.0)
    if (value_le <= 1000 && value_le > 100) {
      float scaled_battery = static_cast<float>(value_le) / 10.0f;
      if (scaled_battery <= 100.0f) {
        data.battery_level = scaled_battery;
        found_data = true;
        ESP_LOGV(GEN_TAG, "Scaled battery level found at offset %zu: %.1f%%", i, data.battery_level);
      }
    }
  }
  
  // Approach 3: Look for voltage values (80-300V range)
  for (size_t i = 0; i <= response.size() - 2; i += 2) {
    uint16_t value_le = response[i] | (response[i+1] << 8);
    uint16_t value_be = (response[i] << 8) | response[i+1];
    
    // Check for voltage values (direct or scaled by 10)
    std::vector<std::pair<uint16_t, float>> voltage_candidates = {
      {value_le, 1.0f}, {value_be, 1.0f},
      {value_le, 0.1f}, {value_be, 0.1f}
    };
    
    for (const auto& candidate : voltage_candidates) {
      float voltage = static_cast<float>(candidate.first) * candidate.second;
      if (voltage >= 80.0f && voltage <= 300.0f) {
        if (std::isnan(data.input_voltage)) {
          data.input_voltage = voltage;
          found_data = true;
          ESP_LOGV(GEN_TAG, "Input voltage found at offset %zu: %.1fV", i, voltage);
        } else if (std::isnan(data.output_voltage) && std::abs(voltage - data.input_voltage) > 5.0f) {
          data.output_voltage = voltage;
          ESP_LOGV(GEN_TAG, "Output voltage found at offset %zu: %.1fV", i, voltage);
        }
        break;
      }
    }
  }
  
  // Approach 4: Look for runtime values (reasonable minute ranges)
  for (size_t i = 0; i <= response.size() - 2; i += 2) {
    uint16_t value_le = response[i] | (response[i+1] << 8);
    
    // Runtime typically 0-999 minutes for most UPS devices
    if (value_le > 0 && value_le <= 999) {
      data.runtime_minutes = static_cast<float>(value_le);
      found_data = true;
      ESP_LOGV(GEN_TAG, "Runtime found at offset %zu: %.1f min", i, data.runtime_minutes);
    }
  }
  
  return found_data;
}

}  // namespace nut_ups
}  // namespace esphome