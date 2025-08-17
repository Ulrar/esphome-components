#pragma once

#include "ups_hid.h"

namespace esphome {
namespace ups_hid {

// APC HID Protocol implementation for modern APC UPS devices
class ApcHidProtocol : public UpsProtocolBase {
public:
  explicit ApcHidProtocol(UpsHidComponent *parent);
  
  bool detect() override;
  bool initialize() override;
  bool read_data(UpsData &data) override;
  UpsProtocol get_protocol_type() const override { return PROTOCOL_APC_HID; }
  std::string get_protocol_name() const override { return "APC HID Protocol"; }

private:
  struct HidReport {
    uint8_t report_id;
    std::vector<uint8_t> data;
    
    HidReport() : report_id(0) {}
  };

  bool init_hid_communication();
  bool read_hid_report(uint8_t report_id, HidReport &report);
  bool write_hid_report(const HidReport &report);
  
  void parse_status_report(const HidReport &report, UpsData &data);
  void parse_battery_report(const HidReport &report, UpsData &data);
  void parse_voltage_report(const HidReport &report, UpsData &data);
  void parse_power_report(const HidReport &report, UpsData &data);
  
  // NUT-compatible parsers
  void parse_power_summary_report(const HidReport &report, UpsData &data);
  void parse_present_status_report(const HidReport &report, UpsData &data);
  void parse_apc_status_report(const HidReport &report, UpsData &data);
  void parse_input_voltage_report(const HidReport &report, UpsData &data);
  void parse_load_report(const HidReport &report, UpsData &data);
  void read_device_info();
  void parse_device_info_report(const HidReport &report);
  void log_raw_data(const uint8_t* buffer, size_t buffer_len);
};

} // namespace ups_hid
} // namespace esphome