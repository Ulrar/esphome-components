#include "cyberpower_protocol.h"
#include "ups_hid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cctype>

namespace esphome {
namespace ups_hid {

static const char *const CP_TAG = "ups_hid.cyberpower_hid";

bool CyberPowerProtocol::detect() {
  ESP_LOGD(CP_TAG, "Detecting CyberPower HID protocol");
  
  // Give device time to initialize after connection (same as APC)
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Test multiple report IDs that are known to work with CyberPower devices
  // Based on NUT debug logs
  const uint8_t test_report_ids[] = {
    0x08, // Battery % + Runtime (primary data)
    0x0b, // Status bitmap (PresentStatus)
    0x0f, // Input voltage
    0x13, // Load percentage
    0x0a  // Battery voltage
  };
  
  HidReport test_report;
  
  for (uint8_t report_id : test_report_ids) {
    ESP_LOGD(CP_TAG, "Testing report ID 0x%02X...", report_id);
    
    if (read_hid_report(report_id, test_report)) {
      ESP_LOGI(CP_TAG, "CyberPower HID protocol detected via report 0x%02X (%zu bytes)", 
               report_id, test_report.data.size());
      return true;
    }
    
    // Small delay between attempts
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  ESP_LOGD(CP_TAG, "CyberPower HID protocol not detected");
  return false;
}

bool CyberPowerProtocol::initialize() {
  ESP_LOGI(CP_TAG, "Initializing CyberPower HID protocol");
  
  // Reset scaling factors
  battery_voltage_scale_ = 1.0f;
  battery_scale_checked_ = false;
  
  return true;
}

bool CyberPowerProtocol::read_data(UpsData &data) {
  ESP_LOGD(CP_TAG, "Reading CyberPower HID data");
  
  bool success = false;

  // Core sensors (essential for operation)
  // Read battery capacity limits (Report 0x07) - includes FullChargeCapacity for battery.status
  HidReport battery_capacity_report;
  if (read_hid_report(BATTERY_CAPACITY_REPORT_ID, battery_capacity_report)) {
    parse_battery_capacity_report(battery_capacity_report, data);
    success = true;
  }
  
  // Read battery level and runtime (Report 0x08)
  HidReport battery_runtime_report;
  if (read_hid_report(BATTERY_RUNTIME_REPORT_ID, battery_runtime_report)) {
    parse_battery_runtime_report(battery_runtime_report, data);
    success = true;
  }

  // Read status flags (Report 0x0b)
  HidReport status_report;
  if (read_hid_report(PRESENT_STATUS_REPORT_ID, status_report)) {
    parse_present_status_report(status_report, data);
    success = true;
  }

  // Read input voltage (Report 0x0f)
  HidReport input_voltage_report;
  if (read_hid_report(INPUT_VOLTAGE_REPORT_ID, input_voltage_report)) {
    parse_input_voltage_report(input_voltage_report, data);
    success = true;
  }

  // Read output voltage (Report 0x12)
  HidReport output_voltage_report;
  if (read_hid_report(OUTPUT_VOLTAGE_REPORT_ID, output_voltage_report)) {
    parse_output_voltage_report(output_voltage_report, data);
    success = true;
  }

  // Read load percentage (Report 0x13)
  HidReport load_report;
  if (read_hid_report(LOAD_PERCENT_REPORT_ID, load_report)) {
    parse_load_percent_report(load_report, data);
    success = true;
  }

  // Additional sensors (enhance functionality)
  // Read battery voltage (Report 0x0a) 
  HidReport battery_voltage_report;
  if (read_hid_report(BATTERY_VOLTAGE_REPORT_ID, battery_voltage_report)) {
    parse_battery_voltage_report(battery_voltage_report, data);
  }

  // Read battery voltage nominal (Report 0x09)
  HidReport battery_voltage_nominal_report;
  if (read_hid_report(BATTERY_VOLTAGE_NOMINAL_REPORT_ID, battery_voltage_nominal_report)) {
    parse_battery_voltage_nominal_report(battery_voltage_nominal_report, data);
  }

  // Read input voltage nominal (Report 0x0e)
  HidReport input_voltage_nominal_report;
  if (read_hid_report(INPUT_VOLTAGE_NOMINAL_REPORT_ID, input_voltage_nominal_report)) {
    parse_input_voltage_nominal_report(input_voltage_nominal_report, data);
  }

  // Read input transfer limits (Report 0x10)
  HidReport input_transfer_report;
  if (read_hid_report(INPUT_TRANSFER_REPORT_ID, input_transfer_report)) {
    parse_input_transfer_report(input_transfer_report, data);
  }

  // Read delay settings (Reports 0x15, 0x16)
  HidReport delay_shutdown_report;
  if (read_hid_report(DELAY_SHUTDOWN_REPORT_ID, delay_shutdown_report)) {
    parse_delay_shutdown_report(delay_shutdown_report, data);
  }

  HidReport delay_start_report;
  if (read_hid_report(DELAY_START_REPORT_ID, delay_start_report)) {
    parse_delay_start_report(delay_start_report, data);
  }

  // Read nominal power (Report 0x18)
  HidReport realpower_nominal_report;
  if (read_hid_report(REALPOWER_NOMINAL_REPORT_ID, realpower_nominal_report)) {
    parse_realpower_nominal_report(realpower_nominal_report, data);
  }

  // Read input sensitivity (Report 0x1a)
  HidReport input_sensitivity_report;
  if (read_hid_report(INPUT_SENSITIVITY_REPORT_ID, input_sensitivity_report)) {
    parse_input_sensitivity_report(input_sensitivity_report, data);
  }

  // Read overload status (Report 0x17)
  HidReport overload_report;
  if (read_hid_report(OVERLOAD_REPORT_ID, overload_report)) {
    parse_overload_report(overload_report, data);
  }

  // Read beeper status (Report 0x0c)
  HidReport beeper_status_report;
  if (read_hid_report(BEEPER_STATUS_REPORT_ID, beeper_status_report)) {
    parse_beeper_status_report(beeper_status_report, data);
  }

  // Read device info (Reports 0x02, 0x1b) - these are string descriptors
  HidReport serial_number_report;
  if (read_hid_report(SERIAL_NUMBER_REPORT_ID, serial_number_report)) {
    parse_serial_number_report(serial_number_report, data);
  }

  HidReport firmware_version_report;
  if (read_hid_report(FIRMWARE_VERSION_REPORT_ID, firmware_version_report)) {
    parse_firmware_version_report(firmware_version_report, data);
  }

  // Set frequency to NaN - not available for CyberPower CP1500 model
  data.frequency = NAN;
  
  // TIMING FIX: Only read USB string descriptors after successful HID communication
  // This ensures the device is ready and responsive before attempting descriptor access
  if (success) {
    ESP_LOGD(CP_TAG, "CyberPower HID data read successful, now reading device info...");
    
    // Read manufacturer from USB Manufacturer string descriptor (index 3)
    // NUT shows: UPS.PowerSummary.iManufacturer, Value: 3 → Manufacturer: "CPS"
    std::string manufacturer_string;
    esp_err_t mfr_ret = parent_->usb_get_string_descriptor(3, manufacturer_string);
    
    if (mfr_ret == ESP_OK && !manufacturer_string.empty()) {
      data.manufacturer = manufacturer_string;
      ESP_LOGI(CP_TAG, "Successfully read manufacturer from USB descriptor: \"%s\"", data.manufacturer.c_str());
    } else {
      data.manufacturer.clear();  // Set to unset state instead of hardcoded fallback
      ESP_LOGW(CP_TAG, "Failed to read USB Manufacturer descriptor: %s, leaving unset", esp_err_to_name(mfr_ret));
    }
    
    // Read model from USB Product string descriptor (index 1)
    // NUT shows: Product: "CP1500EPFCLCD"
    std::string product_string;
    esp_err_t prod_ret = parent_->usb_get_string_descriptor(1, product_string);
    
    if (prod_ret == ESP_OK && !product_string.empty()) {
      data.model = product_string;
      ESP_LOGI(CP_TAG, "Successfully read CyberPower model from USB Product descriptor: \"%s\"", data.model.c_str());
    } else {
      data.model.clear();  // Set to unset state instead of hardcoded fallback
      ESP_LOGW(CP_TAG, "Failed to read USB Product descriptor: %s, leaving model unset", esp_err_to_name(prod_ret));
    }
    
    // Read missing dynamic values identified from NUT analysis
    read_missing_dynamic_values(data);
    
    ESP_LOGD(CP_TAG, "CyberPower data read completed successfully");
  } else {
    ESP_LOGW(CP_TAG, "Failed to read any CyberPower HID reports");
    // Leave manufacturer and model unset when HID communication fails
    data.manufacturer.clear();
    data.model.clear();
  }

  return success;
}

bool CyberPowerProtocol::read_hid_report(uint8_t report_id, HidReport &report) {
  uint8_t buffer[64]; // Maximum HID report size
  size_t buffer_len = sizeof(buffer);
  esp_err_t ret;
  
  // Add debug info about parent device state
  ESP_LOGD(CP_TAG, "Attempting to read report 0x%02X from parent device", report_id);
  
  // CyberPower devices primarily use Feature Reports (0x03) - based on NUT debug logs
  ret = parent_->hid_get_report(0x03, report_id, buffer, &buffer_len);
  if (ret == ESP_OK && buffer_len > 0) {
    report.report_id = report_id;
    report.data.assign(buffer, buffer + buffer_len);
    ESP_LOGD(CP_TAG, "READ SUCCESS: Report 0x%02X (%zu bytes)", report_id, buffer_len);
    return true;
  }
  
  // Log the specific error for Feature Report
  ESP_LOGD(CP_TAG, "Feature Report 0x%02X failed: %s", report_id, esp_err_to_name(ret));
  
  // Fallback: try Input Report (0x01) for real-time data
  buffer_len = sizeof(buffer);
  ret = parent_->hid_get_report(0x01, report_id, buffer, &buffer_len);
  if (ret == ESP_OK && buffer_len > 0) {
    report.report_id = report_id;
    report.data.assign(buffer, buffer + buffer_len);
    ESP_LOGD(CP_TAG, "READ SUCCESS (Input): Report 0x%02X (%zu bytes)", report_id, buffer_len);
    return true;
  }
  
  // Log the specific error for Input Report
  ESP_LOGD(CP_TAG, "Input Report 0x%02X failed: %s", report_id, esp_err_to_name(ret));
  ESP_LOGV(CP_TAG, "Failed to read report 0x%02X: %s", report_id, esp_err_to_name(ret));
  return false;
}

void CyberPowerProtocol::parse_battery_runtime_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 4) {
    ESP_LOGW(CP_TAG, "Battery runtime report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT mapping: 
  // Offset 0 (byte 1): RemainingCapacity (battery %) - Size: 8
  // Offset 8 (bytes 2-3): RunTimeToEmpty - Size: 16, little-endian
  // Offset 24 (bytes 4-5): RemainingTimeLimit - Size: 16, little-endian
  uint8_t battery_percentage = report.data[1];
  uint16_t runtime_minutes = report.data[2] | (report.data[3] << 8);
  
  // Clamp battery to 100% like NUT does
  data.battery_level = static_cast<float>(battery_percentage > 100 ? 100 : battery_percentage);
  data.runtime_minutes = static_cast<float>(runtime_minutes);
  
  // Extract runtime low threshold if available (offset 24 = bytes 4-5)
  if (report.data.size() >= 6) {
    uint16_t runtime_low_minutes = report.data[4] | (report.data[5] << 8);
    data.battery_runtime_low = static_cast<float>(runtime_low_minutes);
    ESP_LOGD(CP_TAG, "Battery: %.0f%%, Runtime: %.0f min, Runtime Low: %.0f min", 
             data.battery_level, data.runtime_minutes, data.battery_runtime_low);
  } else {
    ESP_LOGD(CP_TAG, "Battery: %.0f%%, Runtime: %.0f min (raw: %02X %02X%02X)", 
             data.battery_level, data.runtime_minutes, battery_percentage, report.data[3], report.data[2]);
  }
}

void CyberPowerProtocol::parse_battery_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Battery voltage report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x0a, Offset 0, Size 8, Value: 24
  // Current raw value: 0xF0 (240) should become 24V
  // So scaling factor is 24/240 = 0.1 (divide by 10)
  uint8_t voltage_raw = report.data[1];
  data.battery_voltage = static_cast<float>(voltage_raw) / 10.0f; // Scale by 0.1
  
  ESP_LOGD(CP_TAG, "Battery voltage: %.1fV (raw: 0x%02X = %d)", 
           data.battery_voltage, voltage_raw, voltage_raw);
}

void CyberPowerProtocol::parse_present_status_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Present status report too short: %zu bytes", report.data.size());
    return;
  }

  // Based on NUT debug logs - bit flags in first byte
  uint8_t status_byte = report.data[1];
  
  // Parse status bits (based on HID paths from debug)
  bool ac_present = (status_byte & 0x01) != 0;           // Offset 0
  bool charging = (status_byte & 0x02) != 0;             // Offset 1
  bool discharging = (status_byte & 0x04) != 0;          // Offset 2
  bool low_battery = (status_byte & 0x08) != 0;          // Offset 3
  bool fully_charged = (status_byte & 0x10) != 0;        // Offset 4
  bool time_limit_expired = (status_byte & 0x20) != 0;   // Offset 5
  
  // Build status flags
  uint32_t status_flags = 0;
  
  if (ac_present) {
    status_flags |= UPS_STATUS_ONLINE;
  }
  
  if (!ac_present || discharging) {
    status_flags |= UPS_STATUS_ON_BATTERY;
  }
  
  if (charging) {
    status_flags |= UPS_STATUS_CHARGING;
  }
  
  if (low_battery || time_limit_expired) {
    status_flags |= UPS_STATUS_LOW_BATTERY;
  }
  
  data.status_flags = status_flags;
  
  // Set battery status based on charging/discharging state (not FullChargeCapacity)
  if (charging) {
    data.battery_status = "charging";
  } else if (discharging || !ac_present) {
    data.battery_status = "discharging";
  } else if (fully_charged) {
    data.battery_status = "fully charged";
  } else {
    data.battery_status = "normal";
  }
  
  ESP_LOGD(CP_TAG, "Status: AC:%s Charging:%s OnBatt:%s LowBatt:%s BattStatus:\"%s\"", 
           ac_present ? "Yes" : "No",
           charging ? "Yes" : "No", 
           (status_flags & UPS_STATUS_ON_BATTERY) ? "Yes" : "No",
           (status_flags & UPS_STATUS_LOW_BATTERY) ? "Yes" : "No",
           data.battery_status.c_str());
}

void CyberPowerProtocol::parse_input_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(CP_TAG, "Input voltage report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug: Report 0x0f, Value: 231 (matches our 0x00E6 = 230)
  // Data format: [ID, volt_low, volt_high] - 16-bit little endian
  uint16_t voltage_raw = report.data[1] | (report.data[2] << 8);
  data.input_voltage = static_cast<float>(voltage_raw);
  
  ESP_LOGD(CP_TAG, "Input voltage: %.1fV (raw: 0x%02X%02X = %d)", 
           data.input_voltage, report.data[2], report.data[1], voltage_raw);
}

void CyberPowerProtocol::parse_output_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(CP_TAG, "Output voltage report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug: Report 0x12, Value: 231 (matches our 0x00E6 = 230)
  // Data format: [ID, volt_low, volt_high] - 16-bit little endian  
  uint16_t voltage_raw = report.data[1] | (report.data[2] << 8);
  data.output_voltage = static_cast<float>(voltage_raw);
  
  ESP_LOGD(CP_TAG, "Output voltage: %.1fV (raw: 0x%02X%02X = %d)", 
           data.output_voltage, report.data[2], report.data[1], voltage_raw);
}

void CyberPowerProtocol::parse_load_percent_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Load percentage report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug: Report 0x13, Value: 8 (our raw: 0x07 = 7%)
  // Data format: [ID, load%] - single byte
  uint8_t load_percent = report.data[1];
  data.load_percent = static_cast<float>(load_percent);
  
  ESP_LOGD(CP_TAG, "Load: %.0f%% (raw: 0x%02X = %d)", 
           data.load_percent, load_percent, load_percent);
}

void CyberPowerProtocol::check_battery_voltage_scaling(float battery_voltage, float nominal_voltage) {
  if (battery_scale_checked_) {
    return;
  }
  
  // NUT implements scaling check: if voltage > 1.4 * nominal, apply 2/3 scaling
  const float sanity_ratio = 1.4f;
  
  if (battery_voltage > (nominal_voltage * sanity_ratio)) {
    ESP_LOGI(CP_TAG, "Battery voltage %.1fV exceeds %.1fV * %.1f, applying 2/3 scaling",
             battery_voltage, nominal_voltage, sanity_ratio);
    battery_voltage_scale_ = 2.0f / 3.0f;
  } else {
    ESP_LOGD(CP_TAG, "Battery voltage %.1fV is within normal range, no scaling needed",
             battery_voltage);
    battery_voltage_scale_ = 1.0f;
  }
  
  battery_scale_checked_ = true;
}

void CyberPowerProtocol::parse_battery_voltage_nominal_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Battery voltage nominal report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x09, Value: 24 (ConfigVoltage)
  // Some CyberPower models report in decivolts (240 = 24.0V)
  uint8_t voltage_raw = report.data[1];
  data.battery_voltage_nominal = static_cast<float>(voltage_raw) / 10.0f;
  
  ESP_LOGD(CP_TAG, "Battery voltage nominal: %.0fV (raw: 0x%02X = %d)", 
           data.battery_voltage_nominal, voltage_raw, voltage_raw);
}

void CyberPowerProtocol::parse_beeper_status_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Beeper status report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x0c, Value: 2 (AudibleAlarmControl)
  uint8_t beeper_raw = report.data[1];
  
  // Map NUT values: 1=disabled, 2=enabled, 3=muted
  switch (beeper_raw) {
    case 1:
      data.ups_beeper_status = "disabled";
      break;
    case 2:
      data.ups_beeper_status = "enabled";
      break;
    case 3:
      data.ups_beeper_status = "muted";
      break;
    default:
      data.ups_beeper_status = "unknown";
      break;
  }
  
  ESP_LOGD(CP_TAG, "Beeper status: %s (raw: 0x%02X = %d)", 
           data.ups_beeper_status.c_str(), beeper_raw, beeper_raw);
}

void CyberPowerProtocol::parse_input_voltage_nominal_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Input voltage nominal report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x0e, Value: 230 (ConfigVoltage)
  uint8_t voltage_raw = report.data[1];
  data.input_voltage_nominal = static_cast<float>(voltage_raw);
  
  ESP_LOGD(CP_TAG, "Input voltage nominal: %.0fV (raw: 0x%02X = %d)", 
           data.input_voltage_nominal, voltage_raw, voltage_raw);
}

void CyberPowerProtocol::parse_input_transfer_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 5) {
    ESP_LOGW(CP_TAG, "Input transfer report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x10
  // Offset 0, Size 16: LowVoltageTransfer = 170
  // Offset 16, Size 16: HighVoltageTransfer = 260
  uint16_t low_transfer = report.data[1] | (report.data[2] << 8);
  uint16_t high_transfer = report.data[3] | (report.data[4] << 8);
  
  data.input_transfer_low = static_cast<float>(low_transfer);
  data.input_transfer_high = static_cast<float>(high_transfer);
  
  ESP_LOGD(CP_TAG, "Input transfer limits: Low=%.0fV, High=%.0fV", 
           data.input_transfer_low, data.input_transfer_high);
}

void CyberPowerProtocol::parse_delay_shutdown_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(CP_TAG, "Delay shutdown report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x15, Value: -60 (DelayBeforeShutdown)
  // Handle 0xFFFF as "disabled" (not -1)
  uint16_t delay_raw_unsigned = report.data[1] | (report.data[2] << 8);
  if (delay_raw_unsigned == 0xFFFF) {
    // When disabled, use NUT default for CyberPower (DEFAULT_OFFDELAY_CPS = 60)
    data.ups_delay_shutdown = 60;  
    ESP_LOGD(CP_TAG, "UPS delay shutdown: 60 seconds (default, raw: 0xFFFF)");
  } else {
    int16_t delay_raw = static_cast<int16_t>(delay_raw_unsigned);
    data.ups_delay_shutdown = delay_raw;
    ESP_LOGD(CP_TAG, "UPS delay shutdown: %d seconds", data.ups_delay_shutdown);
  }
}

void CyberPowerProtocol::parse_delay_start_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(CP_TAG, "Delay start report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x16, Value: -60 (DelayBeforeStartup)
  // Handle 0xFFFF as "disabled" (not -1)
  uint16_t delay_raw_unsigned = report.data[1] | (report.data[2] << 8);
  if (delay_raw_unsigned == 0xFFFF) {
    // When disabled, use NUT default for CyberPower (DEFAULT_ONDELAY_CPS = 120)
    data.ups_delay_start = 120;
    ESP_LOGD(CP_TAG, "UPS delay start: 120 seconds (default, raw: 0xFFFF)");
  } else {
    int16_t delay_raw = static_cast<int16_t>(delay_raw_unsigned);
    data.ups_delay_start = delay_raw;
    ESP_LOGD(CP_TAG, "UPS delay start: %d seconds", data.ups_delay_start);
  }
}

void CyberPowerProtocol::parse_realpower_nominal_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 3) {
    ESP_LOGW(CP_TAG, "Real power nominal report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x18, Value: 900 (ConfigActivePower)
  uint16_t power_raw = report.data[1] | (report.data[2] << 8);
  data.ups_realpower_nominal = static_cast<float>(power_raw);
  
  ESP_LOGD(CP_TAG, "UPS nominal real power: %.0fW", data.ups_realpower_nominal);
}

void CyberPowerProtocol::parse_input_sensitivity_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Input sensitivity report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x1a, Value: 1 (CPSInputSensitivity)
  uint8_t sensitivity_raw = report.data[1];
  ESP_LOGD(CP_TAG, "Raw CyberPower sensitivity from report 0x1a: 0x%02X (%d)", sensitivity_raw, sensitivity_raw);
  
  // DYNAMIC SENSITIVITY MAPPING: Handle known CyberPower values with intelligent fallbacks
  switch (sensitivity_raw) {
    case 0:
      data.input_sensitivity = "high";
      ESP_LOGI(CP_TAG, "CyberPower input sensitivity: high (raw: %d)", sensitivity_raw);
      break;
    case 1:
      data.input_sensitivity = "normal";
      ESP_LOGI(CP_TAG, "CyberPower input sensitivity: normal (raw: %d)", sensitivity_raw);
      break;
    case 2:
      data.input_sensitivity = "low";
      ESP_LOGI(CP_TAG, "CyberPower input sensitivity: low (raw: %d)", sensitivity_raw);
      break;
    default:
      // DYNAMIC HANDLING: For unknown values, provide better context
      if (sensitivity_raw >= 100) {
        // Large values might indicate report format issue
        ESP_LOGW(CP_TAG, "Unexpected large CyberPower sensitivity value: %d (0x%02X) - possible report format issue", 
                 sensitivity_raw, sensitivity_raw);
        
        // Try alternative parsing - some models might use different byte
        if (report.data.size() >= 3) {
          uint8_t alt_value = report.data[2];
          ESP_LOGD(CP_TAG, "Trying alternative sensitivity parsing from byte[2]: %d", alt_value);
          
          if (alt_value <= 2) {
            sensitivity_raw = alt_value;
            switch (sensitivity_raw) {
              case 0: data.input_sensitivity = "high"; break;
              case 1: data.input_sensitivity = "normal"; break;
              case 2: data.input_sensitivity = "low"; break;
            }
            ESP_LOGI(CP_TAG, "CyberPower input sensitivity (alt parsing): %s (raw: %d)", 
                     data.input_sensitivity.c_str(), sensitivity_raw);
            return;
          }
        }
        
        // Fallback for problematic values
        data.input_sensitivity = "normal";
        ESP_LOGW(CP_TAG, "Using default 'normal' sensitivity due to unexpected value: %d", sensitivity_raw);
      } else {
        // Values 3-99 - extend mapping for future CyberPower models
        if (sensitivity_raw == 3) {
          data.input_sensitivity = "auto";
          ESP_LOGI(CP_TAG, "CyberPower input sensitivity: auto (raw: %d)", sensitivity_raw);
        } else {
          data.input_sensitivity = "unknown";
          ESP_LOGW(CP_TAG, "Unknown CyberPower sensitivity value: %d - please report this for future support", 
                   sensitivity_raw);
        }
      }
      break;
  }
}

void CyberPowerProtocol::parse_firmware_version_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Firmware version report too short: %zu bytes", report.data.size());
    return;
  }

  // DYNAMIC FIRMWARE VERSION: Primary method - USB string descriptor
  // NUT debug shows: Report 0x1b, Value: 5 (CPSFirmwareVersion - string descriptor index)
  uint8_t string_index = report.data[1];
  
  // Validate string index is reasonable (CyberPower typically uses indices 1-10)
  if (string_index > 0 && string_index <= 15) {
    ESP_LOGD(CP_TAG, "Reading CyberPower firmware from USB string descriptor index: %d", string_index);
    
    std::string actual_firmware;
    esp_err_t fw_ret = parent_->usb_get_string_descriptor(string_index, actual_firmware);
    
    if (fw_ret == ESP_OK && !actual_firmware.empty()) {
      // Clean up firmware string - remove invalid characters and trim
      std::string cleaned_firmware = clean_firmware_string(actual_firmware);
      data.firmware_version = cleaned_firmware;
      ESP_LOGI(CP_TAG, "Successfully read CyberPower firmware from USB string descriptor %d: \"%s\"", 
               string_index, data.firmware_version.c_str());
      if (cleaned_firmware != actual_firmware) {
        ESP_LOGD(CP_TAG, "Cleaned firmware string from \"%s\" to \"%s\"", 
                 actual_firmware.c_str(), cleaned_firmware.c_str());
      }
      return;
    } else {
      ESP_LOGW(CP_TAG, "Failed to read USB string descriptor %d for firmware: %s, trying fallbacks", 
               string_index, esp_err_to_name(fw_ret));
    }
  } else {
    ESP_LOGD(CP_TAG, "Invalid string index %d for firmware, trying direct HID parsing", string_index);
  }
  
  // FALLBACK 1: Try to parse firmware from raw HID report data
  // Some CyberPower models might store firmware directly in HID reports
  std::string firmware_from_hid;
  for (size_t i = 1; i < report.data.size() && report.data[i] != 0; i++) {
    if (report.data[i] >= 32 && report.data[i] <= 126) { // Printable ASCII
      firmware_from_hid += static_cast<char>(report.data[i]);
    }
  }
  
  if (!firmware_from_hid.empty() && firmware_from_hid.length() >= 3) {
    data.firmware_version = firmware_from_hid;
    ESP_LOGI(CP_TAG, "Firmware version from HID report data: %s", data.firmware_version.c_str());
    return;
  }
  
  // FALLBACK 2: Try other common CyberPower string descriptor indices
  // Some models may use different indices for firmware
  const uint8_t common_fw_indices[] = {4, 5, 6}; // Common firmware string indices
  for (uint8_t idx : common_fw_indices) {
    if (idx == string_index) continue; // Already tried this one
    
    ESP_LOGD(CP_TAG, "Trying alternative firmware string descriptor index: %d", idx);
    std::string fw_attempt;
    esp_err_t ret = parent_->usb_get_string_descriptor(idx, fw_attempt);
    
    if (ret == ESP_OK && !fw_attempt.empty()) {
      std::string cleaned_fw = clean_firmware_string(fw_attempt);
      if ((cleaned_fw.find("CR") == 0 || cleaned_fw.find("CP") == 0 || 
           cleaned_fw.find("FW") != std::string::npos)) {
        // Looks like a valid CyberPower firmware string
        data.firmware_version = cleaned_fw;
        ESP_LOGI(CP_TAG, "Found CyberPower firmware at alternative string descriptor %d: \"%s\"", 
                 idx, data.firmware_version.c_str());
        if (cleaned_fw != fw_attempt) {
          ESP_LOGD(CP_TAG, "Cleaned alternative firmware string from \"%s\" to \"%s\"", 
                   fw_attempt.c_str(), cleaned_fw.c_str());
        }
        return;
      }
    }
  }
  
  // FALLBACK 3: Generate version from binary data as last resort
  if (report.data.size() >= 3) {
    char firmware_fallback[32];
    snprintf(firmware_fallback, sizeof(firmware_fallback), "CP-%02X.%02X.%02X", 
             report.data[1], report.data[2], 
             (report.data.size() > 3) ? report.data[3] : 0);
    data.firmware_version = firmware_fallback;
    ESP_LOGD(CP_TAG, "Using binary firmware version fallback: %s", data.firmware_version.c_str());
  } else {
    // No firmware version could be determined
    data.firmware_version.clear();
    ESP_LOGW(CP_TAG, "Unable to determine CyberPower firmware version from any source");
  }
  
  ESP_LOGD(CP_TAG, "Final firmware version: %s (original string index: %d)", 
           data.firmware_version.c_str(), string_index);
}

void CyberPowerProtocol::parse_overload_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Overload report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: Report 0x17, Offset: 1, Value: 0 (UPS.Output.Overload)
  // Offset 1 means it's in the second byte of the report
  uint8_t overload_byte = report.data[1];
  bool overload = (overload_byte & 0x01) != 0;  // Check bit 0 (Offset 1 in NUT = bit 0)
  
  if (overload) {
    data.status_flags |= UPS_STATUS_OVERLOAD;
    ESP_LOGW(CP_TAG, "CyberPower UPS OVERLOAD detected (raw: 0x%02X)", overload_byte);
  } else {
    data.status_flags &= ~UPS_STATUS_OVERLOAD;
    ESP_LOGD(CP_TAG, "CyberPower UPS overload status: normal (raw: 0x%02X)", overload_byte);
  }
}

void CyberPowerProtocol::parse_serial_number_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Serial number report too short: %zu bytes", report.data.size());
    return;
  }

  // NUT debug shows: UPS.PowerSummary.iSerialNumber, ReportID: 0x02, Value: 2
  // This is a USB string descriptor index, not the actual serial number
  uint8_t string_index = report.data[1];
  
  ESP_LOGD(CP_TAG, "Serial number string descriptor index: %d", string_index);
  
  // Use real USB string descriptor reading - this will get the actual CyberPower serial number
  // NUT shows: CyberPower real serial = "CRMLX2000234"
  std::string actual_serial;
  esp_err_t ret = parent_->usb_get_string_descriptor(string_index, actual_serial);
  
  if (ret == ESP_OK && !actual_serial.empty()) {
    data.serial_number = actual_serial;
    ESP_LOGI(CP_TAG, "Successfully read CyberPower serial number from USB string descriptor %d: \"%s\"", 
             string_index, data.serial_number.c_str());
  } else {
    // Fallback if USB string descriptor reading fails
    ESP_LOGW(CP_TAG, "Failed to read USB string descriptor %d: %s", 
             string_index, esp_err_to_name(ret));
    
    // Set to unset state instead of generating fallback ID
    data.serial_number.clear();
    ESP_LOGW(CP_TAG, "Leaving serial number unset due to USB string descriptor failure");
  }
  
  ESP_LOGD(CP_TAG, "Serial number: %s (string index: %d)", 
           data.serial_number.c_str(), string_index);
}

void CyberPowerProtocol::read_missing_dynamic_values(UpsData &data) {
  ESP_LOGD(CP_TAG, "Reading CyberPower missing dynamic values from NUT analysis...");
  
  // 1. Battery capacity limits (Report 0x07) - contains multiple values
  HidReport battery_capacity_limits_report;
  if (read_hid_report(0x07, battery_capacity_limits_report)) {
    parse_battery_capacity_limits_report(battery_capacity_limits_report, data);
  }
  
  // 2. Battery chemistry/type (Report 0x03) - same as APC
  HidReport battery_chemistry_report;
  if (read_hid_report(0x03, battery_chemistry_report)) {
    parse_battery_chemistry_report(battery_chemistry_report, data);
  }
  
  // 3. Battery runtime low threshold is already available in existing Report 0x08
  // It's at offset 24 and already parsed in parse_battery_runtime_report
  // Just need to extract it properly
  
  // 4. Set static/derived values based on NUT behavior  
  data.ups_test_result = "No test initiated";  // Default test result
  
  // NOTE: battery_status is now properly set based on charging state in parse_present_status_report
  
  // 5. Timer values represent active countdown (negative when no countdown active)
  // NUT shows: ups.timer.shutdown: -60, ups.timer.start: -60
  data.ups_timer_shutdown = -data.ups_delay_shutdown;  // Negative indicates no active countdown
  data.ups_timer_start = -data.ups_delay_start;        // Negative indicates no active countdown  
  data.ups_timer_reboot = -10;  // CyberPower doesn't have separate reboot timer, use default
  
  ESP_LOGD(CP_TAG, "Completed reading CyberPower missing dynamic values");
}

void CyberPowerProtocol::parse_battery_capacity_report(const HidReport &report, UpsData &data) {
  // This is the same as the capacity limits report - just a cleaner interface
  parse_battery_capacity_limits_report(report, data);
}

void CyberPowerProtocol::parse_battery_capacity_limits_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 6) {
    ESP_LOGW(CP_TAG, "Battery capacity limits report too short: %zu bytes", report.data.size());
    return;
  }
  
  // NUT debug shows Report 0x07 contains multiple capacity values at different offsets:
  // Offset 24: WarningCapacityLimit = 20
  // Offset 32: RemainingCapacityLimit = 10
  // Data format: [ID, byte1, byte2, byte3, byte4, byte5, ...]
  
  // Extract warning capacity limit (offset 24 = byte 4)
  if (report.data.size() > 4) {
    uint8_t warning_limit = report.data[4]; // Offset 24 bits = byte 3 + 1
    data.battery_charge_warning = static_cast<float>(warning_limit);
    ESP_LOGI(CP_TAG, "CyberPower Battery charge warning threshold: %.0f%% (raw: %d)", 
             data.battery_charge_warning, warning_limit);
  }
  
  // Extract remaining capacity limit (offset 32 = byte 5)
  if (report.data.size() > 5) {
    uint8_t remaining_limit = report.data[5]; // Offset 32 bits = byte 4 + 1
    data.battery_charge_low = static_cast<float>(remaining_limit);
    ESP_LOGI(CP_TAG, "CyberPower Battery charge low threshold: %.0f%% (raw: %d)", 
             data.battery_charge_low, remaining_limit);
  }
  
  // Extract full charge capacity (offset 40 = byte 6) - this is NOT used for battery.status 
  // FullChargeCapacity represents maximum capacity (100%), not current status
  if (report.data.size() > 6) {
    uint8_t full_charge_capacity = report.data[6]; // Offset 40 bits = byte 5 + 1
    ESP_LOGD(CP_TAG, "CyberPower FullChargeCapacity: %d%% (always 100%% for healthy battery)", full_charge_capacity);
    // Note: battery_status is now set from charging state in parse_present_status_report
  }
}

void CyberPowerProtocol::parse_battery_chemistry_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 2) {
    ESP_LOGW(CP_TAG, "Battery chemistry report too short: %zu bytes", report.data.size());
    return;
  }
  
  // NUT debug: ReportID: 0x03, Value: 4 (iDeviceChemistry) - same as APC
  uint8_t chemistry_raw = report.data[1];
  
  // Map chemistry values based on NUT libhid implementation (same as APC)
  switch (chemistry_raw) {
    case 1:
      data.battery_type = "Alkaline";
      break;
    case 2:
      data.battery_type = "NiCd";
      break;
    case 3:
      data.battery_type = "NiMH";
      break;
    case 4:
      data.battery_type = "PbAcid";  // Lead Acid (matches NUT output)
      break;
    case 5:
      data.battery_type = "LiIon";
      break;
    case 6:
      data.battery_type = "LiPoly";
      break;
    default:
      data.battery_type = "Unknown";
      ESP_LOGW(CP_TAG, "Unknown CyberPower battery chemistry value: %d", chemistry_raw);
      break;
  }
  
  ESP_LOGI(CP_TAG, "CyberPower Battery chemistry: %s (raw: %d)", 
           data.battery_type.c_str(), chemistry_raw);
}

std::string CyberPowerProtocol::clean_firmware_string(const std::string &raw_firmware) {
  if (raw_firmware.empty()) {
    return raw_firmware;
  }
  
  std::string cleaned = raw_firmware;
  
  // Remove non-printable characters and common USB string descriptor artifacts
  cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), [](unsigned char c) {
    // Keep alphanumeric, dots, dashes, and spaces
    return !(std::isalnum(c) || c == '.' || c == '-' || c == ' ');
  }), cleaned.end());
  
  // Trim trailing and leading whitespace
  // Trim leading whitespace
  cleaned.erase(cleaned.begin(), std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) {
    return !std::isspace(ch);
  }));
  
  // Trim trailing whitespace
  cleaned.erase(std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) {
    return !std::isspace(ch);
  }).base(), cleaned.end());
  
  // If the string is now empty, return the original
  if (cleaned.empty()) {
    ESP_LOGW(CP_TAG, "Firmware string cleaning resulted in empty string, keeping original");
    return raw_firmware;
  }
  
  return cleaned;
}

bool CyberPowerProtocol::beeper_enable() {
  ESP_LOGD(CP_TAG, "Sending CyberPower beeper enable command");
  
  // CYBERPOWER DEVICE SPECIFIC: From NUT debug, device uses report ID 0x0c
  // NUT shows: "UPS.PowerSummary.AudibleAlarmControl, Type: Feature, ReportID: 0x0c"
  ESP_LOGD(CP_TAG, "Trying beeper enable with report ID 0x%02X", BEEPER_STATUS_REPORT_ID);
  
  uint8_t beeper_data[2] = {BEEPER_STATUS_REPORT_ID, 0x02};  // Report ID, Value=2 (enabled)
  
  esp_err_t ret = parent_->hid_set_report(0x03, BEEPER_STATUS_REPORT_ID, beeper_data, sizeof(beeper_data));
  if (ret == ESP_OK) {
    ESP_LOGI(CP_TAG, "CyberPower beeper enabled successfully with report ID 0x%02X", BEEPER_STATUS_REPORT_ID);
    return true;
  } else {
    ESP_LOGW(CP_TAG, "Failed to enable CyberPower beeper with report ID 0x%02X: %s", BEEPER_STATUS_REPORT_ID, esp_err_to_name(ret));
    return false;
  }
}

bool CyberPowerProtocol::beeper_disable() {
  ESP_LOGD(CP_TAG, "Sending CyberPower beeper disable command");
  
  // CYBERPOWER DEVICE SPECIFIC: From NUT debug, device uses report ID 0x0c
  ESP_LOGD(CP_TAG, "Trying beeper disable with report ID 0x%02X", BEEPER_STATUS_REPORT_ID);
  
  uint8_t beeper_data[2] = {BEEPER_STATUS_REPORT_ID, 0x01};  // Report ID, Value=1 (disabled)
  
  esp_err_t ret = parent_->hid_set_report(0x03, BEEPER_STATUS_REPORT_ID, beeper_data, sizeof(beeper_data));
  if (ret == ESP_OK) {
    ESP_LOGI(CP_TAG, "CyberPower beeper disabled successfully with report ID 0x%02X", BEEPER_STATUS_REPORT_ID);
    return true;
  } else {
    ESP_LOGW(CP_TAG, "Failed to disable CyberPower beeper with report ID 0x%02X: %s", BEEPER_STATUS_REPORT_ID, esp_err_to_name(ret));
    return false;
  }
}

bool CyberPowerProtocol::beeper_mute() {
  ESP_LOGD(CP_TAG, "Sending CyberPower beeper mute command");
  
  // MUTE FUNCTIONALITY (Value 3):
  // - Acknowledges and silences current active alarms  
  // - Beeper may still sound for new critical events
  // - Different from DISABLE (1) which turns off beeper completely
  uint8_t beeper_data[2] = {0x0c, 0x03};  // Report ID, Value=3 (muted/acknowledged)
  
  esp_err_t ret = parent_->hid_set_report(0x03, BEEPER_STATUS_REPORT_ID, beeper_data, sizeof(beeper_data));
  if (ret == ESP_OK) {
    ESP_LOGI(CP_TAG, "CyberPower beeper muted (current alarms acknowledged) successfully");
    return true;
  } else {
    ESP_LOGW(CP_TAG, "Failed to mute CyberPower beeper: %s", esp_err_to_name(ret));
    return false;
  }
}

bool CyberPowerProtocol::beeper_test() {
  ESP_LOGD(CP_TAG, "Starting CyberPower beeper test sequence");
  
  // First, read current beeper status to restore later
  HidReport current_report;
  if (!read_hid_report(BEEPER_STATUS_REPORT_ID, current_report)) {
    ESP_LOGW(CP_TAG, "Failed to read current beeper status for test");
    return false;
  }
  
  uint8_t original_state = (current_report.data.size() >= 2) ? current_report.data[1] : 0x02;
  ESP_LOGI(CP_TAG, "Original beeper state: %d", original_state);
  
  // For CyberPower beeper test, we need to:
  // 1. Enable beeper first (if disabled)
  // 2. Wait for beeper to sound (longer delay)
  // 3. Disable beeper to stop the test sound
  // 4. Restore original state
  
  // FOCUS ON ENABLE/DISABLE: Since NUT shows beeper currently "enabled" (value=2)
  // Test by disabling first (may be audible), then re-enabling
  ESP_LOGI(CP_TAG, "Step 1: Disabling beeper (from current enabled state)");
  if (!beeper_disable()) {
    ESP_LOGW(CP_TAG, "Failed to disable beeper for test");
    return false;
  }
  
  ESP_LOGI(CP_TAG, "Step 2: Waiting 3 seconds with beeper disabled");
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(CP_TAG, "Step 3: Re-enabling beeper");
  if (!beeper_enable()) {
    ESP_LOGW(CP_TAG, "Failed to re-enable beeper");
    // Don't return false - continue to restore original state
  }
  
  // Brief delay before restoration
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Step 4: Restore original beeper state
  ESP_LOGI(CP_TAG, "Step 4: Restoring original beeper state: %d", original_state);
  uint8_t restore_data[2] = {0x0c, original_state};
  esp_err_t ret = parent_->hid_set_report(0x03, BEEPER_STATUS_REPORT_ID, restore_data, sizeof(restore_data));
  
  if (ret == ESP_OK) {
    ESP_LOGI(CP_TAG, "CyberPower beeper test sequence completed successfully");
    return true;
  } else {
    ESP_LOGW(CP_TAG, "Beeper test completed but failed to restore original state: %s", esp_err_to_name(ret));
    return true; // Test succeeded even if restore failed
  }
}

}  // namespace ups_hid
}  // namespace esphome