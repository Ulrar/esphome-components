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
  if (report.data.size() < 4) {
    ESP_LOGW(APC_HID_TAG, "Status report too short: %zu bytes", report.data.size());
    return;
  }
  
  uint32_t status_flags = 0;
  
  // Parse common APC HID status bits
  uint8_t status_byte_1 = report.data[0];
  uint8_t status_byte_2 = report.data[1];
  
  // Online/AC Present
  if (status_byte_1 & 0x04) {
    status_flags |= UPS_STATUS_ONLINE;
  }
  
  // On Battery
  if (status_byte_1 & 0x10) {
    status_flags |= UPS_STATUS_ON_BATTERY;
  }
  
  // Low Battery
  if (status_byte_1 & 0x02) {
    status_flags |= UPS_STATUS_LOW_BATTERY;
  }
  
  // Replace Battery
  if (status_byte_1 & 0x08) {
    status_flags |= UPS_STATUS_REPLACE_BATTERY;
  }
  
  // Charging
  if (status_byte_2 & 0x01) {
    status_flags |= UPS_STATUS_CHARGING;
  }
  
  // Overload
  if (status_byte_2 & 0x20) {
    status_flags |= UPS_STATUS_OVERLOAD;
  }
  
  // Fault/Shutdown
  if (status_byte_2 & 0x40) {
    status_flags |= UPS_STATUS_FAULT;
  }
  
  data.status_flags = status_flags;
  
  ESP_LOGV(APC_HID_TAG, "Status flags: 0x%08X (bytes: 0x%02X 0x%02X)", 
           status_flags, status_byte_1, status_byte_2);
}

void ApcHidProtocol::parse_battery_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 8) {
    ESP_LOGW(APC_HID_TAG, "Battery report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Battery charge level (typically at offset 0-1, 16-bit value, percentage)
  uint16_t battery_raw = (report.data[1] << 8) | report.data[0];
  if (battery_raw <= 100) {
    data.battery_level = static_cast<float>(battery_raw);
    ESP_LOGV(APC_HID_TAG, "Battery Level: %.1f%%", data.battery_level);
  } else {
    ESP_LOGW(APC_HID_TAG, "Battery level out of range: %u", battery_raw);
  }
  
  // Runtime remaining (typically at offset 2-3, 16-bit value, minutes)
  uint16_t runtime_raw = (report.data[3] << 8) | report.data[2];
  if (runtime_raw > 0 && runtime_raw < 65535) {
    data.runtime_minutes = static_cast<float>(runtime_raw);
    ESP_LOGV(APC_HID_TAG, "Runtime: %.1f min", data.runtime_minutes);
  }
  
  // Battery voltage (if available, typically at offset 4-5)
  if (report.data.size() >= 6) {
    uint16_t battery_voltage_raw = (report.data[5] << 8) | report.data[4];
    if (battery_voltage_raw > 0) {
      // Convert from raw value to voltage (scaling depends on UPS model)
      float battery_voltage = static_cast<float>(battery_voltage_raw) / 100.0f;
      ESP_LOGV(APC_HID_TAG, "Battery Voltage: %.2fV", battery_voltage);
    }
  }
}

void ApcHidProtocol::parse_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 8) {
    ESP_LOGW(APC_HID_TAG, "Voltage report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Input voltage (typically at offset 0-1, 16-bit value)
  uint16_t input_voltage_raw = (report.data[1] << 8) | report.data[0];
  if (input_voltage_raw > 0) {
    // Most APC devices report voltage in tenths of volts
    float voltage = static_cast<float>(input_voltage_raw) / 10.0f;
    // Sanity check - typical UPS voltages are between 80-300V
    if (voltage >= 80.0f && voltage <= 300.0f) {
      data.input_voltage = voltage;
      ESP_LOGV(APC_HID_TAG, "Input Voltage: %.1fV", data.input_voltage);
    } else {
      ESP_LOGW(APC_HID_TAG, "Input voltage out of range: %.1fV", voltage);
    }
  }
  
  // Output voltage (typically at offset 2-3, 16-bit value)
  uint16_t output_voltage_raw = (report.data[3] << 8) | report.data[2];
  if (output_voltage_raw > 0) {
    float voltage = static_cast<float>(output_voltage_raw) / 10.0f;
    // Sanity check - typical UPS voltages are between 80-300V
    if (voltage >= 80.0f && voltage <= 300.0f) {
      data.output_voltage = voltage;
      ESP_LOGV(APC_HID_TAG, "Output Voltage: %.1fV", data.output_voltage);
    } else {
      ESP_LOGW(APC_HID_TAG, "Output voltage out of range: %.1fV", voltage);
    }
  }
  
  // Line frequency (if available, typically at offset 4-5)
  if (report.data.size() >= 6) {
    uint16_t frequency_raw = (report.data[5] << 8) | report.data[4];
    if (frequency_raw > 0) {
      data.frequency = static_cast<float>(frequency_raw) / 10.0f;
      ESP_LOGV(APC_HID_TAG, "Frequency: %.1f Hz", data.frequency);
    }
  }
}

void ApcHidProtocol::parse_power_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 4) {
    ESP_LOGW(APC_HID_TAG, "Power report too short: %zu bytes", report.data.size());
    return;
  }
  
  // Load percentage (typically at offset 0-1, 16-bit value)
  uint16_t load_raw = (report.data[1] << 8) | report.data[0];
  if (load_raw <= 100) {
    data.load_percent = static_cast<float>(load_raw);
    ESP_LOGV(APC_HID_TAG, "Load: %.1f%%", data.load_percent);
  } else {
    ESP_LOGW(APC_HID_TAG, "Load percentage out of range: %u", load_raw);
    // Clamp to valid range like Smart Protocol does
    data.load_percent = std::max(0.0f, std::min(100.0f, static_cast<float>(load_raw)));
  }
  
  // Apparent power (if available, typically at offset 2-3)
  if (report.data.size() >= 4) {
    uint16_t apparent_power_raw = (report.data[3] << 8) | report.data[2];
    if (apparent_power_raw > 0) {
      ESP_LOGV(APC_HID_TAG, "Apparent Power: %u VA", apparent_power_raw);
    }
  }
}

void ApcHidProtocol::read_device_info() {
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