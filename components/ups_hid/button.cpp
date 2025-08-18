#include "button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ups_hid {

static const char *const BUTTON_TAG = "ups_hid.button";

void UpsHidButton::dump_config() {
  ESP_LOGCONFIG(BUTTON_TAG, "UPS HID Button:");
  ESP_LOGCONFIG(BUTTON_TAG, "  Beeper action: %s", beeper_action_.c_str());
}

void UpsHidButton::press_action() {
  if (!parent_) {
    ESP_LOGE(BUTTON_TAG, "No UPS HID parent component set");
    return;
  }

  if (!parent_->is_connected()) {
    ESP_LOGW(BUTTON_TAG, "UPS not connected, cannot execute beeper action: %s", beeper_action_.c_str());
    return;
  }

  ESP_LOGI(BUTTON_TAG, "Executing beeper action: %s", beeper_action_.c_str());

  // Get the current protocol from the parent component
  UpsData ups_data = parent_->get_ups_data();
  if (ups_data.detected_protocol == PROTOCOL_UNKNOWN) {
    ESP_LOGW(BUTTON_TAG, "UPS protocol not detected, cannot execute beeper action");
    return;
  }

  // Get the active protocol and call the appropriate beeper method
  bool success = false;
  
  // Access the active protocol through the parent's internal methods
  // Note: This requires adding a public method to get the active protocol
  auto active_protocol = parent_->get_active_protocol();
  if (!active_protocol) {
    ESP_LOGE(BUTTON_TAG, "No active protocol available");
    return;
  }

  if (beeper_action_ == "enable") {
    success = active_protocol->beeper_enable();
  } else if (beeper_action_ == "disable") {
    success = active_protocol->beeper_disable();
  } else if (beeper_action_ == "mute") {
    success = active_protocol->beeper_mute();
  } else if (beeper_action_ == "test") {
    success = active_protocol->beeper_test();
  } else {
    ESP_LOGE(BUTTON_TAG, "Unknown beeper action: %s", beeper_action_.c_str());
    return;
  }

  if (success) {
    ESP_LOGI(BUTTON_TAG, "Beeper action '%s' executed successfully", beeper_action_.c_str());
  } else {
    ESP_LOGW(BUTTON_TAG, "Failed to execute beeper action: %s", beeper_action_.c_str());
  }
}

}  // namespace ups_hid
}  // namespace esphome