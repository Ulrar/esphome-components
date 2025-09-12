#include "protocol_eaton_5px.h"
#include "constants_hid.h"
#include "constants_ups.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace ups_hid {

static const char *const EATON_TAG = "ups_hid.eaton_5px";

// Candidate report IDs to probe for Eaton 5PX (in practice seen in NUT mge-hid mappings)
static const uint8_t EATON_TEST_REPORT_IDS[] = { 0x0C, 0x16, 0x06, 0x30, 0x31 };

bool Eaton5PxProtocol::detect() {
  ESP_LOGD(EATON_TAG, "Detecting Eaton 5PX protocol...");

  if (!parent_->is_connected()) {
    ESP_LOGD(EATON_TAG, "Device not connected, skipping Eaton detection");
    return false;
  }

  // Allow device a short time to enumerate
  vTaskDelay(pdMS_TO_TICKS(timing::USB_INITIALIZATION_DELAY_MS));

  std::vector<uint8_t> buf;
  for (uint8_t id : EATON_TEST_REPORT_IDS) {
    if (!parent_->is_connected()) return false;
    ESP_LOGD(EATON_TAG, "Testing report 0x%02X", id);
    if (read_hid_report(id, buf)) {
      ESP_LOGI(EATON_TAG, "Eaton 5PX detected via report 0x%02X (%zu bytes)", id, buf.size());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(timing::REPORT_RETRY_DELAY_MS));
  }

  ESP_LOGD(EATON_TAG, "Eaton 5PX not detected");
  return false;
}

bool Eaton5PxProtocol::initialize() {
  ESP_LOGD(EATON_TAG, "Initializing Eaton 5PX protocol");
  // Nothing special needed for init in this minimal implementation
  return true;
}

bool Eaton5PxProtocol::read_hid_report(uint8_t report_id, std::vector<uint8_t> &out) {
  if (!parent_->is_connected()) return false;

  uint8_t buffer[limits::MAX_HID_REPORT_SIZE];
  size_t buffer_len = sizeof(buffer);

  // Prefer Input report for dynamic data
  esp_err_t ret = parent_->hid_get_report(HID_REPORT_TYPE_INPUT, report_id, buffer, &buffer_len, parent_->get_protocol_timeout());
  if (ret == ESP_OK && buffer_len > 0) {
    out.assign(buffer, buffer + buffer_len);
    ESP_LOGV(EATON_TAG, "Read Input report 0x%02X (%zu bytes)", report_id, buffer_len);
    return true;
  }

  // Fallback to Feature report
  buffer_len = sizeof(buffer);
  ret = parent_->hid_get_report(HID_REPORT_TYPE_FEATURE, report_id, buffer, &buffer_len, parent_->get_protocol_timeout());
  if (ret == ESP_OK && buffer_len > 0) {
    out.assign(buffer, buffer + buffer_len);
    ESP_LOGV(EATON_TAG, "Read Feature report 0x%02X (%zu bytes)", report_id, buffer_len);
    return true;
  }

  return false;
}

void Eaton5PxProtocol::parse_power_summary(const std::vector<uint8_t> &buf, UpsData &data) {
  // Minimal heuristic parsing based on NUT mapping: RemainingCapacity (percent) and RunTimeToEmpty (seconds)
  if (buf.size() < 4) return;

  // Common placements: battery % often at offset 1, runtime at offsets 2-3 (LE seconds)
  uint8_t batt_percent = buf.size() > 1 ? buf[1] : 0;
  uint16_t runtime_seconds = 0;
  if (buf.size() > 3) runtime_seconds = static_cast<uint16_t>(buf[2]) | (static_cast<uint16_t>(buf[3]) << 8);

  // Clamp and convert
  data.battery.level = static_cast<float>(std::min<uint8_t>(batt_percent, static_cast<uint8_t>(battery::MAX_LEVEL_PERCENT)));
  if (runtime_seconds > 0) data.battery.runtime_minutes = static_cast<float>(runtime_seconds) / 60.0f;

  ESP_LOGD(EATON_TAG, "Parsed power summary: battery=%.0f%% runtime=%.1fmin", data.battery.level, data.battery.runtime_minutes);
}

void Eaton5PxProtocol::parse_present_status(const std::vector<uint8_t> &buf, UpsData &data) {
  if (buf.size() < 2) return;
  uint8_t status = buf[1];

  // Use same semantics as other protocols: bit0 AC present, bit1 charging, bit2 discharging etc (heuristic)
  bool ac_present = (status & 0x01) != 0;
  bool charging = (status & 0x02) != 0;
  bool discharging = (status & 0x04) != 0;

  if (ac_present && !discharging) {
    data.power.status = status::ONLINE;
    data.power.input_voltage = parent_->get_fallback_nominal_voltage();
  } else {
    data.power.status = status::ON_BATTERY;
    data.power.input_voltage = NAN;
  }

  if (charging) data.battery.status = battery_status::CHARGING;
  else if (discharging) data.battery.status = battery_status::DISCHARGING;

  ESP_LOGD(EATON_TAG, "Parsed present status: AC=%s CHRG=%s DISCH=%s", ac_present?"Y":"N", charging?"Y":"N", discharging?"Y":"N");
}

bool Eaton5PxProtocol::read_data(UpsData &data) {
  ESP_LOGV(EATON_TAG, "Reading Eaton 5PX data (minimal)");

  bool success = false;
  std::vector<uint8_t> buf;

  // Try power summary
  if (read_hid_report(0x0C, buf)) {
    parse_power_summary(buf, data);
    success = true;
  }

  // Try present/status
  if (read_hid_report(0x16, buf)) {
    parse_present_status(buf, data);
    success = true;
  }

  // Try battery alternative report
  if (read_hid_report(0x06, buf)) {
    parse_power_summary(buf, data);
    success = true;
  }

  // Try input voltage
  if (read_hid_report(0x30, buf) && buf.size() >= 3) {
    // heuristic: voltage at offset 1 as raw value scaled by 0.1
    uint8_t vraw = buf[1];
    data.power.input_voltage = static_cast<float>(vraw) / battery::VOLTAGE_SCALE_FACTOR;
    success = true;
    ESP_LOGD(EATON_TAG, "Parsed input voltage: %.1fV (raw=0x%02X)", data.power.input_voltage, vraw);
  }

  // Read USB descriptors for manufacturer/model if we got data
  if (success) {
    std::string mfr;
    if (parent_->get_string_descriptor(3, mfr) == ESP_OK && !mfr.empty()) {
      data.device.manufacturer = mfr;
    }
    std::string prod;
    if (parent_->get_string_descriptor(1, prod) == ESP_OK && !prod.empty()) {
      // Trim trailing firmware tokens if present
      size_t pos = prod.find(" FW:");
      if (pos != std::string::npos) prod.resize(pos);
      data.device.model = prod;
    }
  }

  // Default test result
  data.test.ups_test_result = test::RESULT_NO_TEST;

  return success;
}

}  // namespace ups_hid
}  // namespace esphome
