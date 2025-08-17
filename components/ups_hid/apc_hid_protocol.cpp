#include "apc_hid_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace ups_hid {

static const char *const APC_HID_TAG = "ups_hid.apc_hid";

// APC HID Usage Pages and Usage IDs (based on NUT project research)
static const uint16_t APC_USAGE_PAGE_UPS = 0x84;
static const uint16_t APC_USAGE_PAGE_BATTERY = 0x85;
static const uint16_t APC_USAGE_PAGE_POWER = 0x80;

// HID Report IDs used by APC UPS devices (based on working ESP32 NUT server)
static const uint8_t APC_REPORT_ID_STATUS = 0x01;    // UPS status flags
static const uint8_t APC_REPORT_ID_BATTERY = 0x06;   // Battery level and runtime  
static const uint8_t APC_REPORT_ID_LOAD = 0x07;      // UPS load information
static const uint8_t APC_REPORT_ID_VOLTAGE = 0x0e;   // Input/output voltage
static const uint8_t APC_REPORT_ID_BEEPER = 0x1f;    // Beeper control

// APC-specific date conversion (hex-as-decimal format)
static std::string convert_apc_date(uint32_t apc_date) {
  if (apc_date == 0) return "Unknown";
  
  // APC uses hex-as-decimal format, e.g., 0x102202 = 10/22/02
  uint8_t month = (apc_date >> 16) & 0xFF;
  uint8_t day = (apc_date >> 8) & 0xFF;
  uint8_t year = apc_date & 0xFF;
  
  // Convert 2-digit year to 4-digit (Y2K handling)
  uint16_t full_year = (year <= 69) ? (2000 + year) : (1900 + year);
  
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", month, day, full_year);
  return std::string(date_str);
}

// APC HID Protocol implementation
ApcHidProtocol::ApcHidProtocol(UpsHidComponent *parent) : UpsProtocolBase(parent) {}

bool ApcHidProtocol::detect() {
  ESP_LOGD(APC_HID_TAG, "Detecting APC HID Protocol...");
  
  // Give device time to initialize after connection
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Try multiple report IDs that are known to work with APC devices
  // Based on working ESP32 NUT server implementation and NUT drivers
  const uint8_t test_report_ids[] = {
    0x01, // Basic status (UPS.PowerSummary.APCStatusFlag) - most common
    0x06, // Battery info (UPS.PowerSummary.RemainingCapacity) 
    0x07, // More status (UPS.PowerSummary.PresentStatus)
    0x0e, // Input voltage
    0x1f  // Audible alarm control
  };
  
  HidReport test_report;
  
  for (uint8_t report_id : test_report_ids) {
    ESP_LOGD(APC_HID_TAG, "Testing report ID 0x%02X...", report_id);
    
    if (read_hid_report(report_id, test_report)) {
      ESP_LOGI(APC_HID_TAG, "SUCCESS: APC HID Protocol detected with report ID 0x%02X (%zu bytes)", 
               report_id, test_report.data.size());
      return true;
    }
    
    // Small delay between attempts
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  ESP_LOGD(APC_HID_TAG, "APC HID Protocol detection failed - no reports responded");
  return false;
}

bool ApcHidProtocol::initialize() {
  ESP_LOGD(APC_HID_TAG, "Initializing APC HID Protocol...");
  
  // Initialize HID communication
  if (!init_hid_communication()) {
    ESP_LOGE(APC_HID_TAG, "Failed to initialize HID communication");
    return false;
  }
  
  // Read basic device information
  read_device_info();
  
  ESP_LOGI(APC_HID_TAG, "APC HID Protocol initialized successfully");
  return true;
}

bool ApcHidProtocol::read_data(UpsData &data) {
  ESP_LOGV(APC_HID_TAG, "Reading APC HID UPS data...");
  
  bool success = false;
  
  // Set manufacturer and model information (based on NUT apc-hid.c format_mfr/format_model)
  data.manufacturer = "APC";
  data.model = "Back-UPS ES"; // Generic model name for APC VID=0x051d, PID=0x0002
  
  // Read status report (most important)
  HidReport status_report;
  if (read_hid_report(APC_REPORT_ID_STATUS, status_report)) {
    parse_status_report(status_report, data);
    success = true;
  } else {
    ESP_LOGV(APC_HID_TAG, "Failed to read status report");
  }
  
  // Read battery report
  HidReport battery_report;
  if (read_hid_report(APC_REPORT_ID_BATTERY, battery_report)) {
    parse_battery_report(battery_report, data);
    success = true;
  } else {
    ESP_LOGV(APC_HID_TAG, "Failed to read battery report");
  }
  
  // Read voltage report
  HidReport voltage_report;
  if (read_hid_report(APC_REPORT_ID_VOLTAGE, voltage_report)) {
    parse_voltage_report(voltage_report, data);
    success = true;
  } else {
    ESP_LOGV(APC_HID_TAG, "Failed to read voltage report");
  }
  
  // Read load report
  HidReport load_report;
  if (read_hid_report(APC_REPORT_ID_LOAD, load_report)) {
    parse_power_report(load_report, data);
    success = true;
  } else {
    ESP_LOGV(APC_HID_TAG, "Failed to read load report");
  }
  
  if (success) {
    ESP_LOGV(APC_HID_TAG, "Successfully read UPS data");
  }
  
  return success;
}

bool ApcHidProtocol::init_hid_communication() {
  // Set up HID communication parameters
  // For APC devices, we typically use standard HID interrupt transfers
  return true;
}

bool ApcHidProtocol::read_hid_report(uint8_t report_id, HidReport &report) {
  // Use Feature Report (0x03) for HID GET_REPORT requests
  const uint8_t report_type = 0x03; // Feature Report
  uint8_t buffer[64]; // Maximum HID report size
  size_t buffer_len = sizeof(buffer);
  
  ESP_LOGD(APC_HID_TAG, "Reading HID Feature report 0x%02X...", report_id);
  
  esp_err_t ret = parent_->hid_get_report(report_type, report_id, buffer, &buffer_len);
  if (ret != ESP_OK) {
    ESP_LOGD(APC_HID_TAG, "HID GET_REPORT (Feature 0x%02X) failed: %s", report_id, esp_err_to_name(ret));
    return false;
  }
  
  if (buffer_len == 0) {
    ESP_LOGD(APC_HID_TAG, "Empty HID report received for ID 0x%02X", report_id);
    return false;
  }
  
  report.report_id = report_id;
  report.data.assign(buffer, buffer + buffer_len);
  
  ESP_LOGD(APC_HID_TAG, "HID Feature report 0x%02X: received %zu bytes", report_id, buffer_len);
  
  // Log the raw data for debugging
  if (buffer_len > 0) {
    std::string hex_data;
    for (size_t i = 0; i < buffer_len; i++) {
      char hex_byte[4];
      snprintf(hex_byte, sizeof(hex_byte), "%02X ", buffer[i]);
      hex_data += hex_byte;
    }
    ESP_LOGD(APC_HID_TAG, "Raw data: %s", hex_data.c_str());
  }
  
  return true;
}

bool ApcHidProtocol::write_hid_report(const HidReport &report) {
  // Use HID Feature Report for UPS control commands  
  const uint8_t report_type = 0x03; // Feature Report
  
  esp_err_t ret = parent_->hid_set_report(report_type, report.report_id, 
                                          report.data.data(), report.data.size());
  if (ret != ESP_OK) {
    ESP_LOGD(APC_HID_TAG, "HID SET_REPORT failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  ESP_LOGD(APC_HID_TAG, "HID report 0x%02X: sent %zu bytes", report.report_id, report.data.size());
  return true;
}

void ApcHidProtocol::parse_status_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(APC_HID_TAG, "Status report too short: %zu bytes", report.data.size());
    return;
  }
  
  uint32_t status_flags = 0;
  
  // Parse APC HID status based on working NUT implementation
  // report.data[0] = report ID (0x01)
  // report.data[1] = status flags byte
  uint8_t status_byte = report.data[1];
  
  ESP_LOGD(APC_HID_TAG, "Status byte: 0x%02X", status_byte);
  
  // Parse status flags based on working NUT server implementation
  bool ac_present = status_byte & 0x01;           // Bit 0: AC present
  bool charging = status_byte & 0x04;             // Bit 2: Charging  
  bool discharging = status_byte & 0x10;          // Bit 4: Discharging
  bool good = status_byte & 0x20;                 // Bit 5: Battery good
  bool internal_failure = status_byte & 0x40;     // Bit 6: Internal failure
  bool need_replacement = status_byte & 0x80;     // Bit 7: Need replacement
  
  // Set primary status flags - prioritize discharging status for accurate battery detection
  if (discharging || !ac_present) {
    status_flags |= UPS_STATUS_ON_BATTERY;
  } else if (ac_present && !discharging) {
    status_flags |= UPS_STATUS_ONLINE;
  } else {
    // Default to online if status is ambiguous
    status_flags |= UPS_STATUS_ONLINE;
  }
  
  if (charging) {
    status_flags |= UPS_STATUS_CHARGING;
  }
  
  if (!good || internal_failure || need_replacement) {
    status_flags |= UPS_STATUS_FAULT;
  }
  
  // Check for additional status bytes if available (flexible parsing)
  if (report.data.size() >= 3) {
    uint8_t overload_byte = report.data[2];
    if (overload_byte > 0) {
      status_flags |= UPS_STATUS_OVERLOAD;
    }
  }
  
  if (report.data.size() >= 4) {
    uint8_t shutdown_byte = report.data[3];
    if (shutdown_byte > 0) {
      status_flags |= UPS_STATUS_LOW_BATTERY;
    }
  }
  
  data.status_flags = status_flags;
  
  ESP_LOGI(APC_HID_TAG, "UPS Status - AC:%s, Charging:%s, Discharging:%s, Good:%s, Flags:0x%02X", 
           ac_present ? "Yes" : "No", 
           charging ? "Yes" : "No",
           discharging ? "Yes" : "No",
           good ? "Yes" : "No",
           status_byte);
}

void ApcHidProtocol::parse_battery_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(APC_HID_TAG, "Battery report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Parse battery level based on working ESP32 NUT server implementation
  // Report 0x06: recv[1] = battery charge percentage
  // report.data[0] = report ID (0x06)  
  // report.data[1] = battery charge level percentage (direct value)
  data.battery_level = static_cast<float>(report.data[1]);
  ESP_LOGI(APC_HID_TAG, "Battery level: %.0f%%", data.battery_level);
  
  // Parse runtime if more bytes available (32-bit little-endian)
  // recv[2] + 256 * recv[3] + 256 * 256 * recv[4] + 256 * 256 * 256 * recv[5]
  if (report.data.size() >= 6) {
    uint32_t runtime_raw = report.data[2] + 
                          (report.data[3] << 8) + 
                          (report.data[4] << 16) + 
                          (report.data[5] << 24);
    if (runtime_raw > 0 && runtime_raw < 65535) { // Sanity check
      data.runtime_minutes = static_cast<float>(runtime_raw);
      ESP_LOGI(APC_HID_TAG, "Runtime: %.0f minutes", data.runtime_minutes);
    } else {
      // Set estimate based on battery level if raw value seems invalid
      data.runtime_minutes = data.battery_level * 0.5f; 
      ESP_LOGV(APC_HID_TAG, "Using estimated runtime: %.0f minutes", data.runtime_minutes);
    }
  } else {
    // Set reasonable default if runtime not available
    data.runtime_minutes = data.battery_level * 0.5f; // Rough estimate based on battery level
    ESP_LOGV(APC_HID_TAG, "Using estimated runtime: %.0f minutes", data.runtime_minutes);
  }
}

void ApcHidProtocol::parse_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(APC_HID_TAG, "Voltage report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Parse voltage - handle both 2-byte and 3-byte reports
  // report.data[0] = report ID (0x0E)
  // report.data[1] = voltage (primary byte)
  uint16_t voltage_raw = report.data[1];
  
  // If we have a second byte, treat it as high byte for 16-bit value
  if (report.data.size() >= 3) {
    voltage_raw += (report.data[2] << 8);
    ESP_LOGV(APC_HID_TAG, "16-bit voltage: 0x%04X", voltage_raw);
  } else {
    ESP_LOGV(APC_HID_TAG, "8-bit voltage: 0x%02X", voltage_raw);
  }
  
  // Voltage is typically reported directly in volts for APC devices
  data.input_voltage = static_cast<float>(voltage_raw);
  data.output_voltage = data.input_voltage; // Assume same for most UPS devices
  
  ESP_LOGI(APC_HID_TAG, "Voltage: %.0fV", data.input_voltage);
}

void ApcHidProtocol::parse_power_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(APC_HID_TAG, "Power report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Parse load - handle different report sizes
  // report.data[0] = report ID (0x07)
  
  if (report.data.size() >= 7) {
    // Working ESP32 NUT server format: recv[6] = UPS load percentage
    data.load_percent = static_cast<float>(report.data[6]);
    ESP_LOGI(APC_HID_TAG, "Load: %.0f%% (from byte 6)", data.load_percent);
  } else {
    // Shorter format - try different bytes to find load percentage
    // Current data: 07 39 4B (57, 75 in decimal)
    
    // Method 1: Try byte 1 (0x39 = 57%)
    uint8_t load_candidate1 = report.data[1];
    // Method 2: Try byte 2 (0x4B = 75%)
    uint8_t load_candidate2 = report.data[2];
    
    ESP_LOGI(APC_HID_TAG, "Load candidates - Byte1: %d%%, Byte2: %d%%", 
             load_candidate1, load_candidate2);
    
    // Use the first reasonable value (prefer byte 1 based on previous analysis)
    if (load_candidate1 <= 100) {
      data.load_percent = static_cast<float>(load_candidate1);
      ESP_LOGI(APC_HID_TAG, "Load: %.0f%% (from byte 1)", data.load_percent);
    } else if (load_candidate2 <= 100) {
      data.load_percent = static_cast<float>(load_candidate2);
      ESP_LOGI(APC_HID_TAG, "Load: %.0f%% (from byte 2)", data.load_percent);
    } else {
      ESP_LOGW(APC_HID_TAG, "No valid load percentage found");
    }
  }
}

void ApcHidProtocol::read_device_info() {
  // Skip reading device info for input-only devices
  if (parent_->is_input_only_device()) {
    ESP_LOGD(APC_HID_TAG, "Skipping device info read for input-only device");
    return;
  }
  
  // Read configuration report for device information
  HidReport config_report;
  if (read_hid_report(APC_REPORT_ID_BEEPER, config_report)) {
    parse_device_info_report(config_report);
  }
}

void ApcHidProtocol::parse_device_info_report(const HidReport &report) {
  if (report.data.size() < 16) {
    ESP_LOGW(APC_HID_TAG, "Device info report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Manufacturer date (APC hex-as-decimal format, typically at offset 8-11)
  if (report.data.size() >= 12) {
    uint32_t mfr_date = (report.data[11] << 24) | (report.data[10] << 16) |
                        (report.data[9] << 8) | report.data[8];
    if (mfr_date != 0) {
      std::string date_str = convert_apc_date(mfr_date);
      ESP_LOGD(APC_HID_TAG, "Manufacture Date: %s", date_str.c_str());
    }
  }
  
  // Battery replacement date (if available, typically at offset 12-15)
  if (report.data.size() >= 16) {
    uint32_t battery_date = (report.data[15] << 24) | (report.data[14] << 16) |
                            (report.data[13] << 8) | report.data[12];
    if (battery_date != 0) {
      std::string date_str = convert_apc_date(battery_date);
      ESP_LOGD(APC_HID_TAG, "Battery Date: %s", date_str.c_str());
    }
  }
}

} // namespace ups_hid
} // namespace esphome