#pragma once

#include "ups_hid.h"
#include "data_composite.h"
#include "data_device.h"

namespace esphome {
namespace ups_hid {

class Eaton5PxProtocol : public UpsProtocolBase {
 public:
  explicit Eaton5PxProtocol(UpsHidComponent *parent) : UpsProtocolBase(parent) {}
  ~Eaton5PxProtocol() override = default;

  DeviceInfo::DetectedProtocol get_protocol_type() const override { return DeviceInfo::PROTOCOL_GENERIC_HID; }
  std::string get_protocol_name() const override { return "Eaton 5PX"; }

  bool detect() override;
  bool initialize() override;
  bool read_data(UpsData &data) override;

  // Control/test/beeper not implemented for this minimal driver

 private:
  bool read_hid_report(uint8_t report_id, std::vector<uint8_t> &out);
  void parse_power_summary(const std::vector<uint8_t> &buf, UpsData &data);
  void parse_present_status(const std::vector<uint8_t> &buf, UpsData &data);
};

// Register for Eaton/MGE vendor ID
#define EATON_VENDOR_ID 0x0463
REGISTER_UPS_PROTOCOL_FOR_VENDOR(EATON_VENDOR_ID, Eaton5PxProtocol, 
    [](UpsHidComponent *parent) -> std::unique_ptr<UpsProtocolBase> { return std::make_unique<Eaton5PxProtocol>(parent); },
    "Eaton 5PX", "Targeted Eaton 5PX HID protocol", 50)

}  // namespace ups_hid
}  // namespace esphome
