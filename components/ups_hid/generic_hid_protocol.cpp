#include "ups_hid.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <set>
#include <map>
#include <algorithm>
#include <cmath>

namespace esphome {
namespace ups_hid {

static const char *const GEN_TAG = "ups_hid.generic";

// Common HID Power Device report IDs based on NUT analysis
// These are the most frequently used report IDs across different UPS vendors
static const uint8_t COMMON_REPORT_IDS[] = {
  0x01, // General status (widely used)
  0x06, // Battery status (APC and others)
  0x0C, // Power summary (battery % + runtime)
  0x16, // Present status bitmap
  0x30, // Input measurements
  0x31, // Output measurements  
  0x40, // Battery system
  0x50, // Load percentage
};

// Extended search range for enumeration
static const uint8_t EXTENDED_REPORT_IDS[] = {
  0x02, 0x03, 0x04, 0x05, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
  0x20, 0x21, 0x22, 0x32, 0x33, 0x35, 0x42, 0x43, 0x44, 0x45,
};

// Report type constants for HID
static const uint8_t HID_REPORT_TYPE_INPUT = 0x01;
static const uint8_t HID_REPORT_TYPE_OUTPUT = 0x02;
static const uint8_t HID_REPORT_TYPE_FEATURE = 0x03;

bool GenericHidProtocol::detect() {
  ESP_LOGD(GEN_TAG, "Detecting Generic HID Protocol...");
  
  // Check device connection status first
  if (!parent_->is_device_connected()) {
    ESP_LOGD(GEN_TAG, "Device not connected, skipping protocol detection");
    return false;
  }
  
  // Check if this is a known vendor that should use a specific protocol
  uint16_t vid = parent_->get_usb_vendor_id();
  if (vid == 0x051D || vid == 0x0764) { // APC or CyberPower
    ESP_LOGD(GEN_TAG, "Known vendor 0x%04X should use specific protocol", vid);
    return false;
  }
  
  // Try common report IDs to detect HID Power Device
  uint8_t buffer[8];
  size_t buffer_len;
  
  for (uint8_t report_id : COMMON_REPORT_IDS) {
    // Check connection status before each attempt
    if (!parent_->is_device_connected()) {
      ESP_LOGD(GEN_TAG, "Device disconnected during protocol detection");
      return false;
    }
    
    // Try Input Report first (real-time data)
    buffer_len = sizeof(buffer);
    esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_INPUT, report_id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      available_input_reports_.insert(report_id);
      ESP_LOGI(GEN_TAG, "Found Input report 0x%02X (%zu bytes)", report_id, buffer_len);
      report_sizes_[report_id] = buffer_len;
      return true;
    }
    
    // Check connection again before trying Feature report
    if (!parent_->is_device_connected()) {
      ESP_LOGD(GEN_TAG, "Device disconnected during protocol detection");
      return false;
    }
    
    // Try Feature Report (static/configuration data)
    buffer_len = sizeof(buffer);
    ret = parent_->hid_get_report(HID_REPORT_TYPE_FEATURE, report_id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      available_feature_reports_.insert(report_id);
      ESP_LOGI(GEN_TAG, "Found Feature report 0x%02X (%zu bytes)", report_id, buffer_len);
      report_sizes_[report_id] = buffer_len;
      return true;
    }
    
    // Small delay to avoid overwhelming the device
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  ESP_LOGD(GEN_TAG, "No standard HID Power Device reports found");
  return false;
}

bool GenericHidProtocol::initialize() {
  ESP_LOGD(GEN_TAG, "Initializing Generic HID Protocol...");
  
  // Clear any previous state
  available_input_reports_.clear();
  available_feature_reports_.clear();
  report_sizes_.clear();
  
  // Enumerate available reports
  enumerate_reports();
  
  if (available_input_reports_.empty() && available_feature_reports_.empty()) {
    ESP_LOGE(GEN_TAG, "No HID reports found during initialization");
    return false;
  }
  
  ESP_LOGI(GEN_TAG, "Generic HID initialized with %zu input and %zu feature reports",
           available_input_reports_.size(), available_feature_reports_.size());
  
  // Log discovered reports for debugging
  ESP_LOGD(GEN_TAG, "Input reports:");
  for (uint8_t id : available_input_reports_) {
    ESP_LOGD(GEN_TAG, "  0x%02X: %zu bytes", id, report_sizes_[id]);
  }
  ESP_LOGD(GEN_TAG, "Feature reports:");
  for (uint8_t id : available_feature_reports_) {
    ESP_LOGD(GEN_TAG, "  0x%02X: %zu bytes", id, report_sizes_[id]);
  }
  
  return true;
}

bool GenericHidProtocol::read_data(UpsData &data) {
  ESP_LOGV(GEN_TAG, "Reading Generic HID UPS data...");
  
  bool success = false;
  uint8_t buffer[64];
  size_t buffer_len;
  
  // Try to read known report types in priority order
  
  // 1. Power Summary (0x0C) - Battery % and runtime (highest priority)
  if (read_report(0x0C, buffer, buffer_len)) {
    parse_power_summary(buffer, buffer_len, data);
    success = true;
  }
  
  // 2. Battery status (0x06) - Alternative battery info
  if (read_report(0x06, buffer, buffer_len)) {
    parse_battery_status(buffer, buffer_len, data);
    success = true;
  }
  
  // 3. Present Status (0x16) - Status flags
  if (read_report(0x16, buffer, buffer_len)) {
    parse_present_status(buffer, buffer_len, data);
    success = true;
  }
  
  // 4. General status (0x01) - Common status report
  if (read_report(0x01, buffer, buffer_len)) {
    parse_general_status(buffer, buffer_len, data);
    success = true;
  }
  
  // 5. Input voltage (0x30 or 0x31)
  if (read_report(0x30, buffer, buffer_len)) {
    parse_voltage(buffer, buffer_len, data, true);
    success = true;
  } else if (read_report(0x31, buffer, buffer_len)) {
    parse_voltage(buffer, buffer_len, data, false);
    success = true;
  }
  
  // 6. Load percentage (0x50)
  if (read_report(0x50, buffer, buffer_len)) {
    parse_load(buffer, buffer_len, data);
    success = true;
  }
  
  // 7. Input sensitivity (0x1a - CyberPower style, 0x35 - APC style)
  if (read_report(0x1a, buffer, buffer_len)) {
    parse_input_sensitivity(buffer, buffer_len, data, "CyberPower-style");
    success = true;
  } else if (read_report(0x35, buffer, buffer_len)) {
    parse_input_sensitivity(buffer, buffer_len, data, "APC-style");
    success = true;
  }
  
  // 8. Try any other discovered reports with heuristic parsing
  if (!success) {
    for (uint8_t id : available_input_reports_) {
      if (id == 0x01 || id == 0x06 || id == 0x0C || id == 0x16 || 
          id == 0x30 || id == 0x31 || id == 0x50 || id == 0x1A || id == 0x35) {
        continue; // Already tried
      }
      
      if (read_report(id, buffer, buffer_len)) {
        ESP_LOGV(GEN_TAG, "Trying heuristic parsing for report 0x%02X", id);
        if (parse_unknown_report(buffer, buffer_len, data)) {
          success = true;
          break;
        }
      }
    }
  }
  
  // Set generic manufacturer/model if not already set
  if (data.manufacturer.empty()) {
    data.manufacturer = "Generic";
  }
  if (data.model.empty()) {
    uint16_t vid = parent_->get_usb_vendor_id();
    uint16_t pid = parent_->get_usb_product_id();
    char model_str[32];
    snprintf(model_str, sizeof(model_str), "HID UPS %04X:%04X", vid, pid);
    data.model = model_str;
  }
  
  // Ensure we have at least basic status
  if (success && data.status_flags == 0) {
    // If we got data but no status flags, assume online
    data.status_flags = UPS_STATUS_ONLINE;
  }
  
  // Set default test result
  data.ups_test_result = "No test initiated";
  
  return success;
}

void GenericHidProtocol::enumerate_reports() {
  ESP_LOGD(GEN_TAG, "Enumerating HID reports...");
  
  uint8_t buffer[64];
  size_t buffer_len;
  int discovered_count = 0;
  
  // First try common report IDs
  for (uint8_t id : COMMON_REPORT_IDS) {
    // Check device connection before each report
    if (!parent_->is_device_connected()) {
      ESP_LOGD(GEN_TAG, "Device disconnected during report enumeration");
      return;
    }
    
    // Check Input reports
    buffer_len = sizeof(buffer);
    esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_INPUT, id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      available_input_reports_.insert(id);
      report_sizes_[id] = buffer_len;
      discovered_count++;
      ESP_LOGV(GEN_TAG, "Found Input report 0x%02X (%zu bytes)", id, buffer_len);
    }
    
    // Check connection again before Feature report
    if (!parent_->is_device_connected()) {
      ESP_LOGD(GEN_TAG, "Device disconnected during report enumeration");
      return;
    }
    
    // Check Feature reports
    buffer_len = sizeof(buffer);
    ret = parent_->hid_get_report(HID_REPORT_TYPE_FEATURE, id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      available_feature_reports_.insert(id);
      if (report_sizes_.find(id) == report_sizes_.end()) {
        report_sizes_[id] = buffer_len;
      }
      discovered_count++;
      ESP_LOGV(GEN_TAG, "Found Feature report 0x%02X (%zu bytes)", id, buffer_len);
    }
    
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  
  // If we found enough reports, skip extended search
  if (discovered_count >= 3) {
    ESP_LOGD(GEN_TAG, "Found %d reports, skipping extended search", discovered_count);
    return;
  }
  
  // Extended search for less common report IDs
  ESP_LOGD(GEN_TAG, "Performing extended report search...");
  for (uint8_t id : EXTENDED_REPORT_IDS) {
    // Check device connection before each extended report
    if (!parent_->is_device_connected()) {
      ESP_LOGD(GEN_TAG, "Device disconnected during extended report search");
      return;
    }
    
    // Limit total discovery time
    if (discovered_count >= 10) {
      break;
    }
    
    buffer_len = sizeof(buffer);
    esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_INPUT, id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      available_input_reports_.insert(id);
      report_sizes_[id] = buffer_len;
      discovered_count++;
      ESP_LOGV(GEN_TAG, "Found Input report 0x%02X (%zu bytes)", id, buffer_len);
    }
    
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  
  ESP_LOGD(GEN_TAG, "Enumeration complete: found %d reports", discovered_count);
}

bool GenericHidProtocol::read_report(uint8_t report_id, uint8_t* buffer, size_t& buffer_len) {
  // Try Input report first if available (real-time data)
  if (available_input_reports_.count(report_id)) {
    buffer_len = 64;
    esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_INPUT, report_id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      ESP_LOGV(GEN_TAG, "Read Input report 0x%02X: %zu bytes", report_id, buffer_len);
      return true;
    }
  }
  
  // Try Feature report (static/configuration data)
  if (available_feature_reports_.count(report_id)) {
    buffer_len = 64;
    esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_FEATURE, report_id, buffer, &buffer_len);
    if (ret == ESP_OK && buffer_len > 0) {
      ESP_LOGV(GEN_TAG, "Read Feature report 0x%02X: %zu bytes", report_id, buffer_len);
      return true;
    }
  }
  
  return false;
}

// Parse methods following HID Power Device Class specification patterns
void GenericHidProtocol::parse_power_summary(uint8_t* data, size_t len, UpsData& ups_data) {
  // Report 0x0C typically contains battery % and runtime
  // Format observed: [0x0C, battery%, runtime_low, runtime_high, ...]
  if (len >= 2) {
    // Battery percentage typically at byte 1
    uint8_t battery = data[1];
    if (battery <= 100) {
      ups_data.battery_level = static_cast<float>(battery);
      ESP_LOGD(GEN_TAG, "Power summary: Battery %d%%", battery);
    } else if (battery <= 200 && battery > 100) {
      // Some devices use 0-200 scale
      ups_data.battery_level = static_cast<float>(battery) / 2.0f;
      ESP_LOGD(GEN_TAG, "Power summary: Battery %.1f%% (scaled from %d)", ups_data.battery_level, battery);
    }
  }
  
  if (len >= 4) {
    // Runtime in minutes (16-bit little-endian at bytes 2-3)
    uint16_t runtime = data[2] | (data[3] << 8);
    if (runtime > 0 && runtime < 10000) {
      ups_data.runtime_minutes = static_cast<float>(runtime);
      ESP_LOGD(GEN_TAG, "Power summary: Runtime %d minutes", runtime);
    }
  }
}

void GenericHidProtocol::parse_battery_status(uint8_t* data, size_t len, UpsData& ups_data) {
  // Report 0x06 typically contains battery status information
  if (len >= 2) {
    uint8_t status = data[1];
    
    // Common battery status bits (varies by vendor)
    if (status != 0xFF && status != 0x00) { // Valid status
      if (status & 0x01) ups_data.status_flags |= UPS_STATUS_ONLINE;
      if (status & 0x02) ups_data.status_flags |= UPS_STATUS_ON_BATTERY;
      if (status & 0x04) ups_data.status_flags |= UPS_STATUS_LOW_BATTERY;
      if (status & 0x08) ups_data.status_flags |= UPS_STATUS_CHARGING;
      if (status & 0x10) ups_data.status_flags |= UPS_STATUS_REPLACE_BATTERY;
      ESP_LOGD(GEN_TAG, "Battery status: 0x%02X -> flags 0x%08X", status, ups_data.status_flags);
    }
  }
  
  // Some devices include battery level here too
  if (len >= 3 && std::isnan(ups_data.battery_level)) {
    uint8_t battery = data[2];
    if (battery <= 100) {
      ups_data.battery_level = static_cast<float>(battery);
      ESP_LOGD(GEN_TAG, "Battery status: Battery %d%%", battery);
    }
  }
}

void GenericHidProtocol::parse_present_status(uint8_t* data, size_t len, UpsData& ups_data) {
  // Report 0x16 typically contains present status bitmap
  // Based on HID Power Device spec, these are common bit positions
  if (len >= 2) {
    uint8_t status = data[1];
    ups_data.status_flags = 0;
    
    // Standard HID Power Device status bits
    if (status & 0x01) ups_data.status_flags |= UPS_STATUS_CHARGING;
    if (status & 0x02) ups_data.status_flags |= UPS_STATUS_ON_BATTERY; 
    if (status & 0x04) ups_data.status_flags |= UPS_STATUS_ONLINE;
    if (status & 0x08) ups_data.status_flags |= UPS_STATUS_LOW_BATTERY;
    if (status & 0x10) ups_data.status_flags |= UPS_STATUS_REPLACE_BATTERY;
    if (status & 0x20) ups_data.status_flags |= UPS_STATUS_OVERLOAD;
    if (status & 0x40) ups_data.status_flags |= UPS_STATUS_FAULT;
    
    ESP_LOGD(GEN_TAG, "Present status: 0x%02X -> flags 0x%08X", status, ups_data.status_flags);
  }
}

void GenericHidProtocol::parse_general_status(uint8_t* data, size_t len, UpsData& ups_data) {
  // Report 0x01 is often a general status report
  if (len >= 2) {
    // Try to extract basic status
    uint8_t byte1 = data[1];
    
    // Different vendors use different bit patterns, try common ones
    if (byte1 & 0x01) {
      ups_data.status_flags |= UPS_STATUS_ONLINE;
    }
    if (byte1 & 0x10) {
      ups_data.status_flags |= UPS_STATUS_ON_BATTERY;
    }
    
    ESP_LOGV(GEN_TAG, "General status byte: 0x%02X", byte1);
  }
}

void GenericHidProtocol::parse_voltage(uint8_t* data, size_t len, UpsData& ups_data, bool is_input) {
  if (len >= 3) {
    // Common pattern: 16-bit value at bytes 1-2 (little-endian)
    uint16_t voltage_raw = data[1] | (data[2] << 8);
    float voltage = static_cast<float>(voltage_raw);
    
    // Auto-detect scaling
    if (voltage > 1000) {
      voltage /= 10.0f; // Some devices use 0.1V units
    }
    
    // Sanity check
    if (voltage >= 80.0f && voltage <= 300.0f) {
      if (is_input) {
        ups_data.input_voltage = voltage;
        ESP_LOGD(GEN_TAG, "Input voltage: %.1fV", voltage);
      } else {
        ups_data.output_voltage = voltage;
        ESP_LOGD(GEN_TAG, "Output voltage: %.1fV", voltage);
      }
    }
  }
}

void GenericHidProtocol::parse_load(uint8_t* data, size_t len, UpsData& ups_data) {
  if (len >= 2) {
    uint8_t load = data[1];
    if (load <= 100) {
      ups_data.load_percent = static_cast<float>(load);
      ESP_LOGD(GEN_TAG, "Load: %d%%", load);
    } else if (load <= 200) {
      // Some devices use 0-200 scale
      ups_data.load_percent = static_cast<float>(load) / 2.0f;
      ESP_LOGD(GEN_TAG, "Load: %.1f%% (scaled from %d)", ups_data.load_percent, load);
    }
  }
}

bool GenericHidProtocol::parse_unknown_report(uint8_t* data, size_t len, UpsData& ups_data) {
  // Heuristic parsing for unknown reports
  bool found_data = false;
  
  // Look for percentage values (0-100)
  for (size_t i = 1; i < len && i < 4; i++) {
    if (data[i] <= 100 && data[i] > 0) {
      if (std::isnan(ups_data.battery_level)) {
        ups_data.battery_level = static_cast<float>(data[i]);
        ESP_LOGV(GEN_TAG, "Heuristic: Found possible battery level %d%% at byte %zu", data[i], i);
        found_data = true;
      } else if (std::isnan(ups_data.load_percent)) {
        ups_data.load_percent = static_cast<float>(data[i]);
        ESP_LOGV(GEN_TAG, "Heuristic: Found possible load %d%% at byte %zu", data[i], i);
        found_data = true;
      }
    }
  }
  
  // Look for voltage values (16-bit, 80-300V range)
  for (size_t i = 1; i <= len - 2; i++) {
    uint16_t value = data[i] | (data[i+1] << 8);
    float voltage = static_cast<float>(value);
    
    // Try direct and scaled
    if (voltage > 1000) voltage /= 10.0f;
    
    if (voltage >= 80.0f && voltage <= 300.0f) {
      if (std::isnan(ups_data.input_voltage)) {
        ups_data.input_voltage = voltage;
        ESP_LOGV(GEN_TAG, "Heuristic: Found possible voltage %.1fV at bytes %zu-%zu", 
                 voltage, i, i+1);
        found_data = true;
      }
    }
  }
  
  return found_data;
}

void GenericHidProtocol::parse_input_sensitivity(uint8_t* data, size_t len, UpsData& ups_data, const char* style) {
  if (len < 2) {
    ESP_LOGV(GEN_TAG, "Input sensitivity report too short: %zu bytes", len);
    return;
  }
  
  uint8_t sensitivity_raw = data[1];
  ESP_LOGD(GEN_TAG, "Raw input sensitivity (%s): 0x%02X (%d)", style, sensitivity_raw, sensitivity_raw);
  
  // DYNAMIC GENERIC SENSITIVITY MAPPING
  // Handle both APC-style (0x35) and CyberPower-style (0x1a) with intelligent fallbacks
  switch (sensitivity_raw) {
    case 0:
      ups_data.input_sensitivity = "high";
      ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s): high (raw: %d)", style, sensitivity_raw);
      break;
    case 1:
      ups_data.input_sensitivity = "normal";
      ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s): normal (raw: %d)", style, sensitivity_raw);
      break;
    case 2:
      ups_data.input_sensitivity = "low";
      ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s): low (raw: %d)", style, sensitivity_raw);
      break;
    case 3:
      ups_data.input_sensitivity = "auto";
      ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s): auto (raw: %d)", style, sensitivity_raw);
      break;
    default:
      // GENERIC DYNAMIC HANDLING: For unknown values, be more permissive
      if (sensitivity_raw >= 100) {
        // Large values likely indicate wrong report format or encoding
        ESP_LOGW(GEN_TAG, "Unexpected large sensitivity value (%s): %d (0x%02X)", 
                 style, sensitivity_raw, sensitivity_raw);
        
        // Try alternative byte positions for generic devices
        for (size_t i = 2; i < len && i < 5; i++) {
          uint8_t alt_value = data[i];
          if (alt_value <= 3) {
            switch (alt_value) {
              case 0: ups_data.input_sensitivity = "high"; break;
              case 1: ups_data.input_sensitivity = "normal"; break;
              case 2: ups_data.input_sensitivity = "low"; break;
              case 3: ups_data.input_sensitivity = "auto"; break;
            }
            ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s, alt byte[%zu]): %s (raw: %d)", 
                     style, i, ups_data.input_sensitivity.c_str(), alt_value);
            return;
          }
        }
        
        // Fallback to reasonable default
        ups_data.input_sensitivity = "normal";
        ESP_LOGW(GEN_TAG, "Using default 'normal' sensitivity (%s) due to unexpected value: %d", 
                 style, sensitivity_raw);
      } else {
        // Values 4-99 - provide extended mapping for unknown devices
        if (sensitivity_raw <= 10) {
          // Map to nearest known value
          if (sensitivity_raw <= 3) {
            ups_data.input_sensitivity = "high";
          } else if (sensitivity_raw <= 6) {
            ups_data.input_sensitivity = "normal";
          } else {
            ups_data.input_sensitivity = "low";
          }
          ESP_LOGI(GEN_TAG, "Generic input sensitivity (%s, mapped): %s (raw: %d)", 
                   style, ups_data.input_sensitivity.c_str(), sensitivity_raw);
        } else {
          ups_data.input_sensitivity = "unknown";
          ESP_LOGW(GEN_TAG, "Unknown generic sensitivity value (%s): %d", style, sensitivity_raw);
        }
      }
      break;
  }
}

// Generic HID test implementations (basic functionality for unknown devices)
bool GenericHidProtocol::start_battery_test_quick() {
  ESP_LOGI(GEN_TAG, "Starting Generic HID quick battery test");
  
  // Try common test report IDs used by various UPS vendors
  // Based on NUT analysis: report IDs 0x14 (CyberPower), 0x52 (APC), and common alternatives
  uint8_t test_report_ids[] = {0x14, 0x52, 0x0f, 0x1a};
  
  for (size_t i = 0; i < sizeof(test_report_ids); i++) {
    uint8_t report_id = test_report_ids[i];
    uint8_t test_data[2] = {report_id, 1}; // Command value 1 = Quick test
    
    ESP_LOGD(GEN_TAG, "Trying quick battery test with report ID 0x%02X", report_id);
    esp_err_t ret = parent_->hid_set_report(0x03, report_id, test_data, sizeof(test_data));
    
    if (ret == ESP_OK) {
      ESP_LOGI(GEN_TAG, "Generic quick battery test command sent with report ID 0x%02X", report_id);
      return true;
    } else {
      ESP_LOGD(GEN_TAG, "Failed with report ID 0x%02X: %s", report_id, esp_err_to_name(ret));
    }
  }
  
  ESP_LOGW(GEN_TAG, "Failed to send generic quick battery test with all tried report IDs");
  return false;
}

bool GenericHidProtocol::start_battery_test_deep() {
  ESP_LOGI(GEN_TAG, "Starting Generic HID deep battery test");
  
  // Try common test report IDs used by various UPS vendors
  uint8_t test_report_ids[] = {0x14, 0x52, 0x0f, 0x1a};
  
  for (size_t i = 0; i < sizeof(test_report_ids); i++) {
    uint8_t report_id = test_report_ids[i];
    uint8_t test_data[2] = {report_id, 2}; // Command value 2 = Deep test
    
    ESP_LOGD(GEN_TAG, "Trying deep battery test with report ID 0x%02X", report_id);
    esp_err_t ret = parent_->hid_set_report(0x03, report_id, test_data, sizeof(test_data));
    
    if (ret == ESP_OK) {
      ESP_LOGI(GEN_TAG, "Generic deep battery test command sent with report ID 0x%02X", report_id);
      return true;
    } else {
      ESP_LOGD(GEN_TAG, "Failed with report ID 0x%02X: %s", report_id, esp_err_to_name(ret));
    }
  }
  
  ESP_LOGW(GEN_TAG, "Failed to send generic deep battery test with all tried report IDs");
  return false;
}

bool GenericHidProtocol::stop_battery_test() {
  ESP_LOGI(GEN_TAG, "Stopping Generic HID battery test");
  
  // Try common test report IDs used by various UPS vendors
  uint8_t test_report_ids[] = {0x14, 0x52, 0x0f, 0x1a};
  
  for (size_t i = 0; i < sizeof(test_report_ids); i++) {
    uint8_t report_id = test_report_ids[i];
    uint8_t test_data[2] = {report_id, 3}; // Command value 3 = Abort test
    
    ESP_LOGD(GEN_TAG, "Trying battery test stop with report ID 0x%02X", report_id);
    esp_err_t ret = parent_->hid_set_report(0x03, report_id, test_data, sizeof(test_data));
    
    if (ret == ESP_OK) {
      ESP_LOGI(GEN_TAG, "Generic battery test stop command sent with report ID 0x%02X", report_id);
      return true;
    } else {
      ESP_LOGD(GEN_TAG, "Failed with report ID 0x%02X: %s", report_id, esp_err_to_name(ret));
    }
  }
  
  ESP_LOGW(GEN_TAG, "Failed to send generic battery test stop with all tried report IDs");
  return false;
}

bool GenericHidProtocol::start_ups_test() {
  ESP_LOGI(GEN_TAG, "Starting Generic HID UPS test");
  
  // Try common panel test report IDs (less standardized than battery test)
  uint8_t test_report_ids[] = {0x79, 0x0c, 0x1f, 0x15};
  
  for (size_t i = 0; i < sizeof(test_report_ids); i++) {
    uint8_t report_id = test_report_ids[i];
    uint8_t test_data[2] = {report_id, 1}; // Command value 1 = Start test
    
    ESP_LOGD(GEN_TAG, "Trying UPS test with report ID 0x%02X", report_id);
    esp_err_t ret = parent_->hid_set_report(0x03, report_id, test_data, sizeof(test_data));
    
    if (ret == ESP_OK) {
      ESP_LOGI(GEN_TAG, "Generic UPS test command sent with report ID 0x%02X", report_id);
      return true;
    } else {
      ESP_LOGD(GEN_TAG, "Failed with report ID 0x%02X: %s", report_id, esp_err_to_name(ret));
    }
  }
  
  ESP_LOGW(GEN_TAG, "Failed to send generic UPS test with all tried report IDs");
  return false;
}

bool GenericHidProtocol::stop_ups_test() {
  ESP_LOGI(GEN_TAG, "Stopping Generic HID UPS test");
  
  // Try common panel test report IDs
  uint8_t test_report_ids[] = {0x79, 0x0c, 0x1f, 0x15};
  
  for (size_t i = 0; i < sizeof(test_report_ids); i++) {
    uint8_t report_id = test_report_ids[i];
    uint8_t test_data[2] = {report_id, 0}; // Command value 0 = Stop test
    
    ESP_LOGD(GEN_TAG, "Trying UPS test stop with report ID 0x%02X", report_id);
    esp_err_t ret = parent_->hid_set_report(0x03, report_id, test_data, sizeof(test_data));
    
    if (ret == ESP_OK) {
      ESP_LOGI(GEN_TAG, "Generic UPS test stop command sent with report ID 0x%02X", report_id);
      return true;
    } else {
      ESP_LOGD(GEN_TAG, "Failed with report ID 0x%02X: %s", report_id, esp_err_to_name(ret));
    }
  }
  
  ESP_LOGW(GEN_TAG, "Failed to send generic UPS test stop with all tried report IDs");
  return false;
}

}  // namespace ups_hid
}  // namespace esphome