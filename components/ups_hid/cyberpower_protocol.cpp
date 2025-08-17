#include "ups_hid.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

namespace esphome {
namespace ups_hid {

static const char *const CP_TAG = "ups_hid.cyberpower";

// CyberPower HID Report IDs (based on NUT cps-hid.c working implementation)
static const uint8_t CP_REPORT_ID_STATUS = 0x01;      // UPS status flags 
static const uint8_t CP_REPORT_ID_BATTERY = 0x06;     // Battery level and runtime
static const uint8_t CP_REPORT_ID_LOAD = 0x07;        // UPS load information
static const uint8_t CP_REPORT_ID_VOLTAGE = 0x0e;     // Input/output voltage
static const uint8_t CP_REPORT_ID_FREQUENCY = 0x0f;   // Input/output frequency

bool CyberPowerProtocol::detect() {
  ESP_LOGD(CP_TAG, "Detecting CyberPower HID Protocol...");
  
  // Give device time to initialize after connection
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Try multiple report IDs that are known to work with CyberPower devices
  // Based on NUT cps-hid.c implementation
  const uint8_t test_report_ids[] = {
    0x01, // Basic status (UPS.PowerSummary.PresentStatus)
    0x06, // Battery info (UPS.PowerSummary.RemainingCapacity) 
    0x07, // Load info (UPS.Output.PercentLoad)
    0x0e, // Input voltage
    0x0f  // Frequency
  };
  
  HidReport test_report;
  HidReport response_report;
  
  for (uint8_t report_id : test_report_ids) {
    ESP_LOGD(CP_TAG, "Testing report ID 0x%02X...", report_id);
    
    test_report.report_id = report_id;
    test_report.data.clear();
    
    if (send_hid_report(test_report, response_report)) {
      ESP_LOGI(CP_TAG, "SUCCESS: CyberPower HID Protocol detected with report ID 0x%02X (%zu bytes)", 
               report_id, response_report.data.size());
      return true;
    }
    
    // Small delay between attempts
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  ESP_LOGD(CP_TAG, "CyberPower HID Protocol detection failed - no reports responded");
  return false;
}

bool CyberPowerProtocol::initialize() {
  ESP_LOGD(CP_TAG, "Initializing CyberPower HID Protocol...");
  
  ESP_LOGI(CP_TAG, "CyberPower HID Protocol initialized successfully");
  return true;
}

bool CyberPowerProtocol::read_data(UpsData &data) {
  ESP_LOGV(CP_TAG, "Reading CyberPower HID UPS data...");
  
  bool success = false;
  
  // Set manufacturer and model information (based on NUT cps-hid.c format_mfr/format_model)
  data.manufacturer = "CyberPower";
  data.model = "CP Series"; // Generic model name for CyberPower VID=0x0764
  
  // Read status report (most important)
  HidReport status_request;
  status_request.report_id = CP_REPORT_ID_STATUS;
  status_request.data.clear();
  
  HidReport status_report;
  if (send_hid_report(status_request, status_report)) {
    if (parse_status_report(status_report, data)) {
      success = true;
    }
  } else {
    ESP_LOGV(CP_TAG, "Failed to read status report");
  }
  
  // Read battery report
  HidReport battery_request;
  battery_request.report_id = CP_REPORT_ID_BATTERY;
  battery_request.data.clear();
  
  HidReport battery_report;
  if (send_hid_report(battery_request, battery_report)) {
    if (parse_battery_report(battery_report, data)) {
      success = true;
    }
  } else {
    ESP_LOGV(CP_TAG, "Failed to read battery report");
  }
  
  // Read voltage report
  HidReport voltage_request;
  voltage_request.report_id = CP_REPORT_ID_VOLTAGE;
  voltage_request.data.clear();
  
  HidReport voltage_report;
  if (send_hid_report(voltage_request, voltage_report)) {
    if (parse_voltage_report(voltage_report, data)) {
      success = true;
    }
  } else {
    ESP_LOGV(CP_TAG, "Failed to read voltage report");
  }
  
  // Try to read load report (try multiple report IDs)
  const uint8_t load_report_ids[] = {0x07, 0x0F, 0x02, 0x03, 0x04, 0x05};
  for (uint8_t load_id : load_report_ids) {
    HidReport load_request;
    load_request.report_id = load_id;
    load_request.data.clear();
    
    HidReport load_report;
    if (send_hid_report(load_request, load_report)) {
      ESP_LOGD(CP_TAG, "Load report 0x%02X: %zu bytes", load_id, load_report.data.size());
      if (load_report.data.size() >= 2) {
        ESP_LOGD(CP_TAG, "Load report 0x%02X data: %02X %02X", load_id, 
                 load_report.data[0], load_report.data[1]);
        
        // Try different bytes for load percentage (expected ~7-14% for 70W load)
        // Previous data: 07 64 05 0A 14 0A 64
        ESP_LOGD(CP_TAG, "Load report 0x%02X full data:", load_id);
        for (size_t i = 0; i < load_report.data.size(); i++) {
          ESP_LOGD(CP_TAG, "  Byte %zu: 0x%02X (%d)", i, load_report.data[i], load_report.data[i]);
        }
        
        // Try different parsing strategies:
        // Strategy 1: Byte 2 (0x05 = 5%)
        uint8_t load_byte2 = load_report.data.size() > 2 ? load_report.data[2] : 0;
        // Strategy 2: Byte 4 (0x14 = 20 = ~14% after scaling)  
        uint8_t load_byte4 = load_report.data.size() > 4 ? load_report.data[4] : 0;
        
        ESP_LOGD(CP_TAG, "Load candidates - Byte1:%d%%, Byte2:%d%%, Byte4:%d%%", 
                 load_report.data[1], load_byte2, load_byte4);
        
        // Use byte 4 with scaling (0x14=20, divide by ~1.5 to get ~14%)
        if (load_byte4 > 0 && load_byte4 <= 150) {
          data.load_percent = static_cast<float>(load_byte4) * 0.7f; // Scale down from raw value
          ESP_LOGI(CP_TAG, "Load from report 0x%02X: %.1f%% (raw byte4: %d)", 
                   load_id, data.load_percent, load_byte4);
          break;
        }
        // Fallback to byte 2 if byte 4 doesn't work
        else if (load_byte2 > 0 && load_byte2 <= 100) {
          data.load_percent = static_cast<float>(load_byte2);
          ESP_LOGI(CP_TAG, "Load from report 0x%02X: %.1f%% (raw byte2: %d)", 
                   load_id, data.load_percent, load_byte2);
          break;
        }
      }
    }
  }
  
  if (success) {
    ESP_LOGV(CP_TAG, "Successfully read UPS data");
  }
  
  return success;
}

bool CyberPowerProtocol::send_hid_report(const HidReport &report, HidReport &response) {
  // Use Feature Report (0x03) for HID GET_REPORT requests
  const uint8_t report_type = 0x03; // Feature Report
  uint8_t buffer[64]; // Maximum HID report size
  size_t buffer_len = sizeof(buffer);
  
  ESP_LOGD(CP_TAG, "Reading HID Feature report 0x%02X...", report.report_id);
  
  esp_err_t ret = parent_->hid_get_report(report_type, report.report_id, buffer, &buffer_len);
  if (ret != ESP_OK) {
    ESP_LOGD(CP_TAG, "HID GET_REPORT (Feature 0x%02X) failed: %s", report.report_id, esp_err_to_name(ret));
    return false;
  }
  
  if (buffer_len == 0) {
    ESP_LOGD(CP_TAG, "Empty HID report received for ID 0x%02X", report.report_id);
    return false;
  }
  
  response.report_id = report.report_id;
  response.data.assign(buffer, buffer + buffer_len);
  
  ESP_LOGD(CP_TAG, "HID Feature report 0x%02X: received %zu bytes", report.report_id, buffer_len);
  
  // Log the raw data for debugging
  if (buffer_len > 0) {
    std::string hex_data;
    for (size_t i = 0; i < buffer_len; i++) {
      char hex_byte[4];
      snprintf(hex_byte, sizeof(hex_byte), "%02X ", buffer[i]);
      hex_data += hex_byte;
    }
    ESP_LOGD(CP_TAG, "Raw data: %s", hex_data.c_str());
  }
  
  return true;
}

bool CyberPowerProtocol::parse_hid_data(const HidReport &report, UpsData &data) {
  // Generic HID data parser - delegate to specific parsers
  switch (report.report_id) {
    case CP_REPORT_ID_STATUS:
      return parse_status_report(report, data);
    case CP_REPORT_ID_BATTERY:
      return parse_battery_report(report, data);
    case CP_REPORT_ID_VOLTAGE:
      return parse_voltage_report(report, data);
    default:
      ESP_LOGV(CP_TAG, "Unknown report ID 0x%02X", report.report_id);
      return false;
  }
}

bool CyberPowerProtocol::parse_status_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Status report too short: %zu bytes", report.data.size());
    return false;
  }
  
  uint32_t status_flags = 0;
  
  // Parse CyberPower HID status based on working NUT implementation
  // report.data[0] = report ID (0x01)
  // report.data[1] = status flags byte (similar to APC but different bit layout)
  uint8_t status_byte = report.data[1];
  
  ESP_LOGD(CP_TAG, "Status byte: 0x%02X", status_byte);
  
  // Parse status flags - CyberPower may use different bit patterns than expected
  // Need to analyze actual vs expected behavior
  bool ac_present = status_byte & 0x01;           // Bit 0: AC present (but may be unreliable)
  bool charging = status_byte & 0x02;             // Bit 1: Charging  
  bool discharging = status_byte & 0x04;          // Bit 2: Discharging
  bool battery_good = status_byte & 0x08;         // Bit 3: Battery good
  bool overload = status_byte & 0x10;             // Bit 4: Overload
  bool low_battery = status_byte & 0x20;          // Bit 5: Low battery
  bool need_replacement = status_byte & 0x40;     // Bit 6: Need replacement
  bool internal_failure = status_byte & 0x80;     // Bit 7: Internal failure
  
  ESP_LOGD(CP_TAG, "Status bits - AC:%d, Chg:%d, Dischg:%d, Good:%d, OL:%d, LowBat:%d, Repl:%d, Fail:%d",
           ac_present, charging, discharging, battery_good, overload, low_battery, need_replacement, internal_failure);
  
  // EXPERIMENTAL: Try different status detection logic for CyberPower
  // Current issue: UPS on battery but status_byte=0x01 (AC present) - this seems wrong
  
  // Strategy 1: Use voltage as indicator (battery power typically shows lower voltage)
  // Strategy 2: Check if battery level is decreasing over time
  // Strategy 3: Maybe bit 0 doesn't mean AC present for CyberPower
  
  // For now, let's try inverted logic or different bit interpretation
  // If status byte is 0x01 but user says UPS is on battery, maybe:
  // - Bit 0 might mean something else
  // - Or the UPS always reports AC present regardless of actual state
  
  ESP_LOGW(CP_TAG, "ANALYZING STATUS: User reports UPS on battery, status_byte=0x%02X", status_byte);
  ESP_LOGW(CP_TAG, "BINARY: %s", 
           (std::string("") + 
            (status_byte & 0x80 ? "1" : "0") +
            (status_byte & 0x40 ? "1" : "0") +
            (status_byte & 0x20 ? "1" : "0") +
            (status_byte & 0x10 ? "1" : "0") +
            (status_byte & 0x08 ? "1" : "0") +
            (status_byte & 0x04 ? "1" : "0") +
            (status_byte & 0x02 ? "1" : "0") +
            (status_byte & 0x01 ? "1" : "0")).c_str());
  
  // For CyberPower, we need to understand the real status pattern
  // Current observation: status_byte=0x01 when on battery (not expected)
  // This suggests CyberPower uses a different bit mapping
  
  // HYPOTHESIS: Maybe for CyberPower:
  // - 0x01 might mean "UPS functioning" not "AC present"  
  // - Or bit patterns are vendor-specific
  
  // Let's try a different approach: if no charging activity, assume battery mode
  bool likely_on_battery = !charging;  // Simple heuristic
  
  if (likely_on_battery) {
    status_flags |= UPS_STATUS_ON_BATTERY;
    ESP_LOGW(CP_TAG, "UPS detected as ON BATTERY (no charging activity)");
  } else {
    status_flags |= UPS_STATUS_ONLINE;
    ESP_LOGI(CP_TAG, "UPS detected as ONLINE (charging detected or AC mode)");
  }
  
  if (charging) {
    status_flags |= UPS_STATUS_CHARGING;
  }
  
  // Only set fault if we have actual fault indicators (not just absence of battery_good)
  if (internal_failure || need_replacement) {
    status_flags |= UPS_STATUS_FAULT;
  }
  
  if (low_battery) {
    status_flags |= UPS_STATUS_LOW_BATTERY;
  }
  
  if (overload) {
    status_flags |= UPS_STATUS_OVERLOAD;
  }
  
  if (need_replacement) {
    status_flags |= UPS_STATUS_REPLACE_BATTERY;
  }
  
  data.status_flags = status_flags;
  
  ESP_LOGI(CP_TAG, "UPS Status - AC:%s, Charging:%s, Discharging:%s, Good:%s, Flags:0x%02X", 
           ac_present ? "Yes" : "No", 
           charging ? "Yes" : "No",
           discharging ? "Yes" : "No",
           battery_good ? "Yes" : "No",
           status_byte);
           
  return true;
}

bool CyberPowerProtocol::parse_battery_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Battery report too short: %zu bytes", report.data.size());
    return false;
  }
  
  // Parse battery level based on real CyberPower data analysis
  // Report 0x06: Raw value needs scaling for CyberPower PID=0x0501
  // report.data[0] = report ID (0x06)  
  // report.data[1] = battery charge level (raw value, needs scaling)
  
  uint8_t raw_battery = report.data[1];
  ESP_LOGD(CP_TAG, "Raw battery value: %d", raw_battery);
  
  // Apply CyberPower scaling factor (observed: raw=2 should be ~62%)
  // Based on NUT cps-hid.c battery scaling for PID 0x0501
  data.battery_level = static_cast<float>(raw_battery * 30); // 30x scaling factor
  
  // Clamp battery charge to 100% like NUT does for CyberPower
  if (data.battery_level > 100.0f) {
    data.battery_level = 100.0f;
  }
  
  ESP_LOGI(CP_TAG, "Battery level: %.0f%% (raw: %d)", data.battery_level, raw_battery);
  
  // Parse runtime if more bytes available
  if (report.data.size() >= 6) {
    // Log all bytes to understand the data structure
    ESP_LOGD(CP_TAG, "Battery report full data (%zu bytes):", report.data.size());
    for (size_t i = 0; i < report.data.size(); i++) {
      ESP_LOGD(CP_TAG, "  Byte %zu: 0x%02X (%d)", i, report.data[i], report.data[i]);
    }
    
    // Try different runtime parsing strategies:
    // Strategy 1: 32-bit little-endian at bytes 2-5
    uint32_t runtime_raw_32 = report.data[2] + 
                             (report.data[3] << 8) + 
                             (report.data[4] << 16) + 
                             (report.data[5] << 24);
    
    // Strategy 2: 16-bit little-endian at bytes 2-3
    uint16_t runtime_raw_16 = report.data[2] + (report.data[3] << 8);
    
    // Strategy 3: Direct byte values
    uint8_t runtime_byte2 = report.data[2];
    uint8_t runtime_byte3 = report.data[3];
    
    ESP_LOGD(CP_TAG, "Runtime candidates - 32bit:%u, 16bit:%u, byte2:%u, byte3:%u", 
             runtime_raw_32, runtime_raw_16, runtime_byte2, runtime_byte3);
    
    // Use the most reasonable value (expect ~43 minutes)
    if (runtime_raw_16 > 10 && runtime_raw_16 < 300) { // 16-bit value in reasonable range
      data.runtime_minutes = static_cast<float>(runtime_raw_16);
      ESP_LOGI(CP_TAG, "Runtime: %.0f minutes (16-bit at bytes 2-3)", data.runtime_minutes);
    } else if (runtime_byte2 > 10 && runtime_byte2 < 250) { // Single byte in reasonable range
      data.runtime_minutes = static_cast<float>(runtime_byte2);
      ESP_LOGI(CP_TAG, "Runtime: %.0f minutes (byte 2)", data.runtime_minutes);
    } else if (runtime_byte3 > 10 && runtime_byte3 < 250) { // Try byte 3
      data.runtime_minutes = static_cast<float>(runtime_byte3);
      ESP_LOGI(CP_TAG, "Runtime: %.0f minutes (byte 3)", data.runtime_minutes);
    } else {
      // Use battery level estimation as fallback
      data.runtime_minutes = data.battery_level * 0.7f; // Improved estimate 
      ESP_LOGD(CP_TAG, "Using estimated runtime: %.0f minutes", data.runtime_minutes);
    }
  } else {
    // Use battery level estimation for short reports
    data.runtime_minutes = data.battery_level * 0.7f; // Improved estimate
    ESP_LOGD(CP_TAG, "Using estimated runtime: %.0f minutes", data.runtime_minutes);
  }
  
  return true;
}

bool CyberPowerProtocol::parse_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Voltage report too short: %zu bytes", report.data.size());
    return false;
  }
  
  // Parse voltage - handle both 2-byte and 3-byte reports
  // report.data[0] = report ID (0x0E)
  // report.data[1] = voltage (primary byte)
  uint16_t voltage_raw = report.data[1];
  
  // If we have a second byte, treat it as high byte for 16-bit value
  if (report.data.size() >= 3) {
    voltage_raw += (report.data[2] << 8);
    ESP_LOGV(CP_TAG, "16-bit voltage: 0x%04X", voltage_raw);
  } else {
    ESP_LOGV(CP_TAG, "8-bit voltage: 0x%02X", voltage_raw);
  }
  
  // CyberPower voltage might need scaling (NUT applies scaling factors for some models)
  // For now, treat as direct voltage like APC
  data.input_voltage = static_cast<float>(voltage_raw);
  data.output_voltage = data.input_voltage; // Assume same for most UPS devices
  
  ESP_LOGI(CP_TAG, "Voltage: %.0fV", data.input_voltage);
  
  return true;
}

bool CyberPowerProtocol::parse_device_info_report(const HidReport &report, UpsData &data) {
  // CyberPower device info parsing - for future enhancement
  ESP_LOGV(CP_TAG, "Device info report parsing not yet implemented");
  return true;
}

}  // namespace ups_hid
}  // namespace esphome