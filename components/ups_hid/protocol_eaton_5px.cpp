#include "protocol_eaton_5px.h"
#include "constants_hid.h"
#include "constants_ups.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <string>

namespace esphome {
namespace ups_hid {

static const char *const EATON_TAG = "ups_hid.eaton_5px";

static std::string hex_dump(const std::vector<uint8_t> &v) {
  std::string s;
  char buf[6];
  for (size_t i = 0; i < v.size(); ++i) {
    snprintf(buf, sizeof(buf), "%02X%s", v[i], (i + 1 < v.size()) ? " " : "");
    s += buf;
  }
  return s;
}

// Heuristic: scan a buffer for 16-bit LE values and try several scale factors
// Return NAN if none found
static float find_best_voltage_candidate(const std::vector<uint8_t> &buf, float nominal) {
  if (buf.size() < 2) return NAN;
  const float scales[] = {1.0f, 10.0f, 100.0f, 2.0f, 5.0f};
  bool found = false;
  float best = NAN;
  float best_score = 1e9f;

  for (size_t i = 1; i + 1 < buf.size(); ++i) {
    uint16_t raw = static_cast<uint16_t>(buf[i]) | (static_cast<uint16_t>(buf[i+1]) << 8);
    if (raw == 0xFFFF || raw == 0x0000) continue;
    for (float s : scales) {
      float v = static_cast<float>(raw) / s;
      if (v < 50.0f || v > 300.0f) continue; // plausible mains range
      float score = std::fabs(v - nominal);
      if (score < best_score) {
        best_score = score;
        best = v;
        found = true;
      }
    }
  }

  return found ? best : NAN;
}

// Heuristic: find a load percent in the buffer (1..100) prefer values >5
static int find_load_percent_in_buf(const std::vector<uint8_t> &buf) {
  for (size_t i = 1; i < buf.size(); ++i) {
    uint8_t v = buf[i];
    if (v > 5 && v <= 100) return static_cast<int>(v);
  }
  // fallback to any non-zero small value
  for (size_t i = 1; i < buf.size(); ++i) {
    uint8_t v = buf[i];
    if (v > 0 && v <= 100) return static_cast<int>(v);
  }
  return -1;
}

// Candidate report IDs to probe for Eaton 5PX (in practice seen in NUT mge-hid mappings)
static const uint8_t EATON_TEST_REPORT_IDS[] = { 0x0C, 0x16, 0x06, 0x30, 0x31 };

bool Eaton5PxProtocol::detect() {
  ESP_LOGD(EATON_TAG, "Detecting Eaton 5PX protocol...");

  if (!parent_->is_connected()) {
    ESP_LOGD(EATON_TAG, "Device not connected, skipping Eaton detection");
    return false;
  }

  // Try output voltage (report 0x31)
  

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
  ESP_LOGD(EATON_TAG, "Raw 0x0C: %s", hex_dump(buf).c_str());
  parse_power_summary(buf, data);
    success = true;
  }

  // Try present/status
  if (read_hid_report(0x16, buf)) {
  ESP_LOGD(EATON_TAG, "Raw 0x16: %s", hex_dump(buf).c_str());
  parse_present_status(buf, data);
    success = true;
  }

  // Try battery alternative report
  if (read_hid_report(0x06, buf)) {
  ESP_LOGD(EATON_TAG, "Raw 0x06: %s", hex_dump(buf).c_str());
  parse_power_summary(buf, data);
    success = true;
  }

  // Read input (0x30) and output (0x31) reports and use heuristics to pick best candidate
  std::vector<uint8_t> buf30, buf31, buf35, buf06, buf0C;
  if (read_hid_report(0x30, buf30) && buf30.size() > 0) {
    ESP_LOGD(EATON_TAG, "Raw 0x30: %s", hex_dump(buf30).c_str());
    success = true;
  }
  if (read_hid_report(0x31, buf31) && buf31.size() > 0) {
    ESP_LOGD(EATON_TAG, "Raw 0x31: %s", hex_dump(buf31).c_str());
    success = true;
  }

  // Prefer explicit 16-bit candidates but fall back to heuristic scan across both reports
  float nominal = parent_->get_fallback_nominal_voltage();
  float best_candidate = NAN;
  // Try direct 16-bit little-endian at offsets [1,2] in each report first
  auto try_direct = [&](const std::vector<uint8_t> &b) -> float {
    if (b.size() >= 3) {
      uint16_t vraw = static_cast<uint16_t>(b[1]) | (static_cast<uint16_t>(b[2]) << 8);
      if (vraw != 0xFFFF && vraw != 0x0000) {
        float voltage = static_cast<float>(vraw);
        if (voltage > 1000.0f) voltage /= battery::VOLTAGE_SCALE_FACTOR;
        else if (voltage > 100.0f && voltage < 1000.0f) {
          float cand = voltage / battery::VOLTAGE_SCALE_FACTOR;
          if (cand >= 80.0f && cand <= 300.0f) voltage = cand;
        }
        if (voltage >= 50.0f && voltage <= 300.0f) return voltage;
      }
    }
    return NAN;
  };

  float d30 = try_direct(buf30);
  float d31 = try_direct(buf31);

  // Also compute scanned candidates
  float c30 = find_best_voltage_candidate(buf30, nominal);
  float c31 = find_best_voltage_candidate(buf31, nominal);

  // Determine best candidates per-report
  float chosen30 = NAN;
  std::string src30;
  if (!std::isnan(d30) || !std::isnan(c30)) {
    chosen30 = !std::isnan(d30) ? d30 : c30;
    src30 = !std::isnan(d30) ? "direct_0x30" : "scan_0x30";
    if (!std::isnan(d30) && !std::isnan(c30)) {
      if (std::fabs(c30 - nominal) < std::fabs(d30 - nominal)) { chosen30 = c30; src30 = "scan_0x30"; }
    }
  }

  float chosen31 = NAN;
  std::string src31;
  if (!std::isnan(d31) || !std::isnan(c31)) {
    chosen31 = !std::isnan(d31) ? d31 : c31;
    src31 = !std::isnan(d31) ? "direct_0x31" : "scan_0x31";
    if (!std::isnan(d31) && !std::isnan(c31)) {
      if (std::fabs(c31 - nominal) < std::fabs(d31 - nominal)) { chosen31 = c31; src31 = "scan_0x31"; }
    }
  }

  // Decide input voltage by comparing the two per-report candidates.
  // Bias toward 0x30 unless 0x31 is significantly closer to nominal.
  if (!std::isnan(chosen30) || !std::isnan(chosen31)) {
    float best_val = NAN;
    std::string best_src;
    if (!std::isnan(chosen30) && std::isnan(chosen31)) {
      best_val = chosen30; best_src = src30;
    } else if (std::isnan(chosen30) && !std::isnan(chosen31)) {
      best_val = chosen31; best_src = src31;
    } else {
      // both present: compare distances with bias
      float dist30 = std::fabs(chosen30 - nominal);
      float dist31 = std::fabs(chosen31 - nominal);
      const float THRESHOLD_V = 8.0f;
      if (dist31 + THRESHOLD_V < dist30) { best_val = chosen31; best_src = src31; }
      else { best_val = chosen30; best_src = src30; }
    }
    data.power.input_voltage = best_val;
    ESP_LOGD(EATON_TAG, "Selected input voltage candidate: %.1fV (source=%s)", data.power.input_voltage, best_src.c_str());
    success = true;
  }

  // Output: prefer 0x31 candidates
  if (!std::isnan(chosen31)) {
    data.power.output_voltage = chosen31;
    ESP_LOGD(EATON_TAG, "Parsed output voltage: %.1fV (source=%s)", data.power.output_voltage, src31.c_str());
    success = true;
  }

  // Verbose candidate logging to help map bytes -> voltage on real hardware
  if (buf30.size() > 0 || buf31.size() > 0) {
    const float scales[] = {1.0f, 2.0f, 5.0f, 10.0f, 100.0f};
    for (const auto &bpair : {std::make_pair(std::string("0x30"), buf30), std::make_pair(std::string("0x31"), buf31)}) {
      const auto &label = bpair.first;
      const auto &bufx = bpair.second;
      if (bufx.size() < 2) continue;
      for (size_t i = 1; i + 1 < bufx.size(); ++i) {
        uint16_t raw = static_cast<uint16_t>(bufx[i]) | (static_cast<uint16_t>(bufx[i+1]) << 8);
        if (raw == 0xFFFF || raw == 0x0000) continue;
        for (float s : scales) {
          float v = static_cast<float>(raw) / s;
          if (v >= 50.0f && v <= 300.0f) {
            ESP_LOGD(EATON_TAG, "Candidate %s offset %zu raw=0x%04X scale=%.2f -> %.2fV", label.c_str(), i, raw, s, v);
          }
        }
      }
    }
  }

  // For output voltage, prefer explicit parse from 0x31 (direct) then heuristic
  if (!std::isnan(d31)) {
    data.power.output_voltage = d31;
    ESP_LOGD(EATON_TAG, "Parsed output voltage (direct): %.1fV", data.power.output_voltage);
    success = true;
  } else {
    float out_c = find_best_voltage_candidate(buf31, nominal);
    if (!std::isnan(out_c)) {
      data.power.output_voltage = out_c;
      ESP_LOGD(EATON_TAG, "Parsed output voltage (heuristic): %.1fV", data.power.output_voltage);
      success = true;
    }
  }

  // Try load percentage (report 0x35) first, then scan other reports (0x31, 0x06, 0x0C)
  if (read_hid_report(0x35, buf35) && buf35.size() >= 2) {
    ESP_LOGD(EATON_TAG, "Raw 0x35: %s", hex_dump(buf35).c_str());
    uint8_t load_raw = buf35[1];
    if (load_raw <= 100 && load_raw > 0) {
      data.power.load_percent = static_cast<float>(load_raw);
      success = true;
      ESP_LOGD(EATON_TAG, "Parsed load percent: %d%% (raw=0x%02X)", load_raw, load_raw);
    }
  }
  // fallback: scan 0x31 and known battery/summary buffers
  if (data.power.load_percent <= 0.0f) {
    // ensure we have these buffers to scan
    if (buf31.empty()) read_hid_report(0x31, buf31);
    if (buf06.empty()) read_hid_report(0x06, buf06);
    if (buf0C.empty()) read_hid_report(0x0C, buf0C);
    int cand = find_load_percent_in_buf(buf31);
    if (cand < 0) cand = find_load_percent_in_buf(buf06);
    if (cand < 0) cand = find_load_percent_in_buf(buf0C);
    if (cand > 0) {
      data.power.load_percent = static_cast<float>(cand);
      success = true;
      ESP_LOGD(EATON_TAG, "Heuristic load percent: %d%%", cand);
    }
  }

  // If still no load percent, try to derive from reported power (W or VA) in 0x31/0x06
  if (data.power.load_percent <= 0.0f) {
    // scan for 16-bit power candidates in 0x31 and 0x06
    auto scan_power = [&](const std::vector<uint8_t> &b) -> float {
      if (b.size() < 3) return NAN;
      for (size_t i = 1; i + 1 < b.size(); ++i) {
        uint16_t raw = static_cast<uint16_t>(b[i]) | (static_cast<uint16_t>(b[i+1]) << 8);
        if (raw == 0x0000 || raw == 0xFFFF) continue;
        // consider raw as watts directly or scaled (divide by 10)
        float cand1 = static_cast<float>(raw);
        float cand10 = static_cast<float>(raw) / 10.0f;
        // plausible power range for a 1500W UPS: 1..3000 W
        if (cand1 >= 1.0f && cand1 <= 3000.0f) return cand1;
        if (cand10 >= 1.0f && cand10 <= 3000.0f) return cand10;
      }
      return NAN;
    };

    float p = scan_power(buf31);
    if (std::isnan(p)) p = scan_power(buf06);
    if (!std::isnan(p)) {
      // Determine nominal power: prefer detected realpower_nominal, else fallback to 1500W for 1500i
      float nominal_w = data.power.realpower_nominal;
      if (std::isnan(nominal_w)) {
        // Try to infer from model string
        if (data.device.model.find("1500") != std::string::npos) nominal_w = 1500.0f;
        else nominal_w = 1500.0f; // default fallback
      }
      float load = (p / nominal_w) * 100.0f;
      if (load > 0.0f && load <= 200.0f) {
        data.power.load_percent = load;
        success = true;
        ESP_LOGD(EATON_TAG, "Derived load from power: %.0fW nominal=%.0fW -> load=%.1f%% (power_raw=%.1f)", p, nominal_w, data.power.load_percent, p);
      }
    }
  }

  // Read USB descriptors for manufacturer/model if we got data
  if (success) {
    // iManufacturer is commonly index 1 and iProduct/index 3 holds the model string
    std::string mfr;
    if (parent_->get_string_descriptor(1, mfr) == ESP_OK && !mfr.empty()) {
      data.device.manufacturer = mfr;
    }
    std::string prod;
    if (parent_->get_string_descriptor(3, prod) == ESP_OK && !prod.empty()) {
      // Trim trailing firmware tokens if present (often in product string)
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
