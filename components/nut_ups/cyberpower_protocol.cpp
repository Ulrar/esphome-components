#include "nut_ups.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nut_ups {

static const char *const CP_TAG = "nut_ups.cyberpower";

// CyberPower HID Report IDs and data structures
static const uint8_t CP_REPORT_ID_UPS_STATUS = 0x01;
static const uint8_t CP_REPORT_ID_BATTERY_INFO = 0x02;
static const uint8_t CP_REPORT_ID_VOLTAGE_INFO = 0x03;
static const uint8_t CP_REPORT_ID_DEVICE_INFO = 0x04;

// HID Usage Page definitions for UPS (Power Device) - Based on NUT reference
static const uint16_t HID_USAGE_PAGE_POWER_DEVICE = 0x84;
static const uint16_t HID_USAGE_PAGE_BATTERY = 0x85;
static const uint16_t HID_USAGE_PAGE_UPS = 0x84;

// Power Device Class HID Usages (from NUT cps-hid.c)
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
static const uint16_t HID_USAGE_UPS_INPUT_FREQUENCY = 0x32;
static const uint16_t HID_USAGE_UPS_PERCENT_LOAD = 0x35;

bool CyberPowerProtocol::detect() {
  ESP_LOGD(CP_TAG, "Detecting CyberPower HID Protocol...");
  
  // First try vendor/product ID detection (like NUT does)
  // CyberPower vendor ID is 0x0764
  if (parent_->get_usb_vendor_id() == 0x0764) {
    ESP_LOGD(CP_TAG, "CyberPower vendor ID detected: 0x%04X", parent_->get_usb_vendor_id());
    
    // Try to get basic HID report to confirm communication
    HidReport request;
    request.report_id = CP_REPORT_ID_UPS_STATUS;
    request.data = {0x00};
    
    HidReport response;
    if (send_hid_report(request, response)) {
      ESP_LOGD(CP_TAG, "CyberPower HID communication confirmed");
      return true;
    }
  }
  
  // Fallback: try device info request for unknown devices
  HidReport request;
  request.report_id = CP_REPORT_ID_DEVICE_INFO;
  request.data = {0x00};
  
  HidReport response;
  if (send_hid_report(request, response)) {
    // Check if response contains CyberPower identifiers
    std::string info = bytes_to_string(response.data);
    if (info.find("CP") != std::string::npos || 
        info.find("CYBER") != std::string::npos ||
        info.find("CyberPower") != std::string::npos) {
      ESP_LOGD(CP_TAG, "CyberPower device identified from response");
      return true;
    }
  }
  
  ESP_LOGD(CP_TAG, "No CyberPower HID Protocol detected");
  return false;
}

bool CyberPowerProtocol::initialize() {
  ESP_LOGD(CP_TAG, "Initializing CyberPower HID Protocol...");
  
  // Get device information
  HidReport device_info_request;
  device_info_request.report_id = CP_REPORT_ID_DEVICE_INFO;
  device_info_request.data = {0x00};
  
  HidReport device_info_response;
  if (send_hid_report(device_info_request, device_info_response)) {
    ESP_LOGD(CP_TAG, "Device info received, %d bytes", device_info_response.data.size());
    
    // Parse manufacturer and model from device info
    // CyberPower HID devices typically include this in the device descriptor
    if (device_info_response.data.size() >= 8) {
      ESP_LOGD(CP_TAG, "CyberPower device detected");
    }
  }
  
  ESP_LOGI(CP_TAG, "CyberPower HID Protocol initialized successfully");
  return true;
}

bool CyberPowerProtocol::read_data(UpsData &data) {
  ESP_LOGV(CP_TAG, "Reading CyberPower UPS data...");
  
  bool success = true;
  
  // Read UPS status
  HidReport status_request;
  status_request.report_id = CP_REPORT_ID_UPS_STATUS;
  status_request.data = {0x00};
  
  HidReport status_response;
  if (send_hid_report(status_request, status_response)) {
    if (parse_hid_data(status_response, data)) {
      ESP_LOGV(CP_TAG, "Status data parsed successfully");
    } else {
      ESP_LOGW(CP_TAG, "Failed to parse status data");
      success = false;
    }
  } else {
    ESP_LOGW(CP_TAG, "Failed to read UPS status");
    success = false;
  }
  
  // Read battery information
  HidReport battery_request;
  battery_request.report_id = CP_REPORT_ID_BATTERY_INFO;
  battery_request.data = {0x00};
  
  HidReport battery_response;
  if (send_hid_report(battery_request, battery_response)) {
    if (parse_hid_data(battery_response, data)) {
      ESP_LOGV(CP_TAG, "Battery data parsed successfully");
    } else {
      ESP_LOGW(CP_TAG, "Failed to parse battery data");
    }
  } else {
    ESP_LOGW(CP_TAG, "Failed to read battery info");
  }
  
  // Read voltage information
  HidReport voltage_request;
  voltage_request.report_id = CP_REPORT_ID_VOLTAGE_INFO;
  voltage_request.data = {0x00};
  
  HidReport voltage_response;
  if (send_hid_report(voltage_request, voltage_response)) {
    if (parse_hid_data(voltage_response, data)) {
      ESP_LOGV(CP_TAG, "Voltage data parsed successfully");
    } else {
      ESP_LOGW(CP_TAG, "Failed to parse voltage data");
    }
  } else {
    ESP_LOGW(CP_TAG, "Failed to read voltage info");
  }
  
  // Set manufacturer and model for CyberPower
  if (data.manufacturer.empty()) {
    data.manufacturer = "CyberPower";
  }
  
  return success;
}

bool CyberPowerProtocol::send_hid_report(const HidReport &report, HidReport &response) {
  ESP_LOGVV(CP_TAG, "Sending HID report ID: 0x%02X, %d bytes", report.report_id, report.data.size());
  
  // Construct HID report packet
  std::vector<uint8_t> hid_packet;
  hid_packet.push_back(report.report_id);
  hid_packet.insert(hid_packet.end(), report.data.begin(), report.data.end());
  
  std::vector<uint8_t> raw_response;
  if (!send_command(hid_packet, raw_response, 2000)) {
    ESP_LOGW(CP_TAG, "Failed to send HID report");
    return false;
  }
  
  if (raw_response.empty()) {
    ESP_LOGW(CP_TAG, "Empty HID response");
    return false;
  }
  
  // Parse response
  response.report_id = raw_response[0];
  response.data.clear();
  if (raw_response.size() > 1) {
    response.data.assign(raw_response.begin() + 1, raw_response.end());
  }
  
  ESP_LOGVV(CP_TAG, "HID response ID: 0x%02X, %d bytes", response.report_id, response.data.size());
  return true;
}

bool CyberPowerProtocol::parse_hid_data(const HidReport &report, UpsData &data) {
  if (report.data.empty()) {
    return false;
  }
  
  ESP_LOGVV(CP_TAG, "Parsing HID report ID: 0x%02X", report.report_id);
  
  switch (report.report_id) {
    case CP_REPORT_ID_UPS_STATUS:
      return parse_status_report(report, data);
      
    case CP_REPORT_ID_BATTERY_INFO:
      return parse_battery_report(report, data);
      
    case CP_REPORT_ID_VOLTAGE_INFO:
      return parse_voltage_report(report, data);
      
    case CP_REPORT_ID_DEVICE_INFO:
      return parse_device_info_report(report, data);
      
    default:
      ESP_LOGW(CP_TAG, "Unknown HID report ID: 0x%02X", report.report_id);
      return false;
  }
}

bool CyberPowerProtocol::parse_status_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 4) {
    ESP_LOGW(CP_TAG, "Status report too short: %d bytes", report.data.size());
    return false;
  }
  
  // CyberPower status format (varies by model, this is a common interpretation)
  uint8_t status_byte = report.data[0];
  uint8_t flags_byte = report.data[1];
  
  data.status_flags = 0;
  
  // Parse status bits (CyberPower specific)
  if (status_byte & 0x01) data.status_flags |= UPS_STATUS_ONLINE;
  if (status_byte & 0x02) data.status_flags |= UPS_STATUS_ON_BATTERY;
  if (status_byte & 0x04) data.status_flags |= UPS_STATUS_LOW_BATTERY;
  if (status_byte & 0x08) data.status_flags |= UPS_STATUS_REPLACE_BATTERY;
  if (status_byte & 0x10) data.status_flags |= UPS_STATUS_CHARGING;
  if (status_byte & 0x20) data.status_flags |= UPS_STATUS_FAULT;
  if (status_byte & 0x40) data.status_flags |= UPS_STATUS_OVERLOAD;
  
  ESP_LOGV(CP_TAG, "Status: 0x%02X, Flags: 0x%08X", status_byte, data.status_flags);
  return true;
}

bool CyberPowerProtocol::parse_battery_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 6) {
    ESP_LOGW(CP_TAG, "Battery report too short: %d bytes", report.data.size());
    return false;
  }
  
  // CyberPower battery format (little-endian)
  uint16_t battery_level_raw = report.data[0] | (report.data[1] << 8);
  uint16_t runtime_raw = report.data[2] | (report.data[3] << 8);
  
  // Battery level validation with clamping (NUT-style approach)
  float battery_level;
  if (battery_level_raw <= 100) {
    battery_level = static_cast<float>(battery_level_raw);
  } else if (battery_level_raw <= 1000) {
    battery_level = static_cast<float>(battery_level_raw) / 10.0f;
  } else {
    ESP_LOGW(CP_TAG, "Battery level out of range: %u", battery_level_raw);
    battery_level = static_cast<float>(battery_level_raw) / 10.0f; // Try scaling anyway
  }
  
  // Clamp to valid range (0-100%) like NUT does
  data.battery_level = std::max(0.0f, std::min(100.0f, battery_level));
  
  // Runtime is typically in seconds or minutes
  if (runtime_raw < 3600) { // Assume minutes if < 3600
    data.runtime_minutes = static_cast<float>(runtime_raw);
  } else { // Assume seconds if >= 3600
    data.runtime_minutes = static_cast<float>(runtime_raw) / 60.0f;
  }
  
  ESP_LOGV(CP_TAG, "Battery: %.1f%%, Runtime: %.1f min", data.battery_level, data.runtime_minutes);
  return true;
}

bool CyberPowerProtocol::parse_voltage_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 8) {
    ESP_LOGW(CP_TAG, "Voltage report too short: %d bytes", report.data.size());
    return false;
  }
  
  // CyberPower voltage format (little-endian, typically in 0.1V units)
  uint16_t input_voltage_raw = report.data[0] | (report.data[1] << 8);
  uint16_t output_voltage_raw = report.data[2] | (report.data[3] << 8);
  uint16_t load_raw = report.data[4] | (report.data[5] << 8);
  uint16_t frequency_raw = report.data[6] | (report.data[7] << 8);
  
  // Convert raw values to actual units with NUT-style validation
  float input_voltage = static_cast<float>(input_voltage_raw) / 10.0f;
  float output_voltage = static_cast<float>(output_voltage_raw) / 10.0f;
  
  // Voltage range validation (80-300V like NUT does)
  if (input_voltage >= 80.0f && input_voltage <= 300.0f) {
    data.input_voltage = input_voltage;
  } else {
    ESP_LOGW(CP_TAG, "Input voltage out of range: %.1fV", input_voltage);
    data.input_voltage = NAN;
  }
  
  if (output_voltage >= 80.0f && output_voltage <= 300.0f) {
    data.output_voltage = output_voltage;
  } else {
    ESP_LOGW(CP_TAG, "Output voltage out of range: %.1fV", output_voltage);
    data.output_voltage = NAN;
  }
  
  // Load percentage validation with clamping (like NUT does)
  if (load_raw <= 100) {
    data.load_percent = static_cast<float>(load_raw);
  } else if (load_raw <= 1000) {
    // Some models report in 0.1% units
    data.load_percent = static_cast<float>(load_raw) / 10.0f;
  } else {
    ESP_LOGW(CP_TAG, "Load value out of range: %u", load_raw);
    data.load_percent = std::max(0.0f, std::min(100.0f, static_cast<float>(load_raw) / 10.0f));
  }
  
  // Frequency validation (NUT expects 47-53 Hz typically)
  float frequency = static_cast<float>(frequency_raw) / 10.0f;
  if (frequency >= 40.0f && frequency <= 70.0f) {
    data.frequency = frequency;
  } else {
    ESP_LOGW(CP_TAG, "Frequency out of range: %.1f Hz", frequency);
    data.frequency = NAN;
  }
  
  ESP_LOGV(CP_TAG, "Input: %.1fV, Output: %.1fV, Load: %.1f%%, Freq: %.1fHz", 
           data.input_voltage, data.output_voltage, data.load_percent, data.frequency);
  return true;
}

bool CyberPowerProtocol::parse_device_info_report(const HidReport &report, UpsData &data) {
  if (report.data.size() < 16) {
    ESP_LOGW(CP_TAG, "Device info report too short: %d bytes", report.data.size());
    return false;
  }
  
  // Extract model and serial number from device info
  // This is a simplified parser - actual format varies by model
  
  std::string info_string = bytes_to_string(report.data);
  
  // Look for model information
  if (info_string.find("CP") != std::string::npos || 
      info_string.find("CYBER") != std::string::npos) {
    size_t start = info_string.find_first_not_of(" \t\n\r");
    size_t end = info_string.find_first_of(" \t\n\r", start);
    if (start != std::string::npos) {
      data.model = info_string.substr(start, end - start);
    }
  }
  
  data.manufacturer = "CyberPower";
  
  ESP_LOGV(CP_TAG, "Device: %s %s", data.manufacturer.c_str(), data.model.c_str());
  return true;
}

}  // namespace nut_ups
}  // namespace esphome