#include "nut_ups.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace esphome {
namespace nut_ups {

void NutUpsComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NUT UPS Component...");
  
  if (simulation_mode_) {
    ESP_LOGW(TAG, "Running in simulation mode - no actual UPS communication");
    connected_ = true;
    ups_data_.detected_protocol = PROTOCOL_APC_SMART;
    ups_data_.manufacturer = "Simulated";
    ups_data_.model = "Virtual UPS";
    return;
  }
  
#ifdef USE_ESP32
  if (!initialize_usb()) {
    ESP_LOGE(TAG, "Failed to initialize USB");
    mark_failed();
    return;
  }
  
  if (!detect_ups_protocol()) {
    ESP_LOGE(TAG, "Failed to detect UPS protocol");
    connected_ = false;
    return;
  }
  
  connected_ = true;
  ESP_LOGI(TAG, "Successfully connected to UPS using %s", get_protocol_name().c_str());
#else
  ESP_LOGE(TAG, "NUT UPS component requires ESP32 platform");
  mark_failed();
#endif
}

void NutUpsComponent::update() {
  if (simulation_mode_) {
    simulate_ups_data();
    update_sensors();
    return;
  }
  
  if (!connected_) {
    ESP_LOGD(TAG, "UPS not connected, attempting to reconnect...");
    if (detect_ups_protocol()) {
      connected_ = true;
      ESP_LOGI(TAG, "Reconnected to UPS");
    } else {
      return;
    }
  }
  
  if (read_ups_data()) {
    last_successful_read_ = millis();
    update_sensors();
  } else {
    ESP_LOGW(TAG, "Failed to read UPS data");
    if (millis() - last_successful_read_ > protocol_timeout_ms_) {
      ESP_LOGE(TAG, "UPS communication timeout, marking as disconnected");
      connected_ = false;
    }
  }
}

void NutUpsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "NUT UPS Component:");
  ESP_LOGCONFIG(TAG, "  Simulation Mode: %s", simulation_mode_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  USB Vendor ID: 0x%04X", usb_vendor_id_);
  ESP_LOGCONFIG(TAG, "  USB Product ID: 0x%04X", usb_product_id_);
  ESP_LOGCONFIG(TAG, "  Protocol Timeout: %u ms", protocol_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Auto Detect Protocol: %s", auto_detect_protocol_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", get_update_interval());
  
  if (connected_) {
    ESP_LOGCONFIG(TAG, "  Status: Connected");
    ESP_LOGCONFIG(TAG, "  Protocol: %s", get_protocol_name().c_str());
    ESP_LOGCONFIG(TAG, "  Manufacturer: %s", ups_data_.manufacturer.c_str());
    ESP_LOGCONFIG(TAG, "  Model: %s", ups_data_.model.c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  Status: Disconnected");
  }
}

std::string NutUpsComponent::get_protocol_name() const {
  if (active_protocol_) {
    return active_protocol_->get_protocol_name();
  }
  return "Unknown";
}

bool NutUpsComponent::initialize_usb() {
#ifdef USE_ESP32
  ESP_LOGD(TAG, "Initializing USB communication...");
  
  // Initialize USB host if not already done
  // Note: This is a simplified approach - full USB Host implementation
  // would require more extensive ESP-IDF USB Host library integration
  
  // For now, we'll use a basic approach that can be extended
  esp_err_t ret = usb_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "USB initialization failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  ESP_LOGI(TAG, "USB initialized successfully");
  return true;
#else
  return false;
#endif
}

bool NutUpsComponent::detect_ups_protocol() {
  ESP_LOGD(TAG, "Detecting UPS protocol...");
  
  if (!auto_detect_protocol_) {
    // Use APC Smart Protocol as default when auto-detection is disabled
    active_protocol_ = std::make_unique<ApcSmartProtocol>(this);
    if (active_protocol_->initialize()) {
      ESP_LOGI(TAG, "Using pre-configured protocol: %s", active_protocol_->get_protocol_name().c_str());
      ups_data_.detected_protocol = active_protocol_->get_protocol_type();
      return true;
    }
    return false;
  }
  
  // Try APC Smart Protocol first
  auto apc_protocol = std::make_unique<ApcSmartProtocol>(this);
  if (apc_protocol->detect() && apc_protocol->initialize()) {
    active_protocol_ = std::move(apc_protocol);
    ups_data_.detected_protocol = PROTOCOL_APC_SMART;
    ESP_LOGI(TAG, "Detected APC Smart Protocol");
    return true;
  }
  
  // Try CyberPower HID Protocol
  auto cyberpower_protocol = std::make_unique<CyberPowerProtocol>(this);
  if (cyberpower_protocol->detect() && cyberpower_protocol->initialize()) {
    active_protocol_ = std::move(cyberpower_protocol);
    ups_data_.detected_protocol = PROTOCOL_CYBERPOWER_HID;
    ESP_LOGI(TAG, "Detected CyberPower HID Protocol");
    return true;
  }
  
  // Try Generic HID Protocol as fallback
  auto generic_protocol = std::make_unique<GenericHidProtocol>(this);
  if (generic_protocol->detect() && generic_protocol->initialize()) {
    active_protocol_ = std::move(generic_protocol);
    ups_data_.detected_protocol = PROTOCOL_GENERIC_HID;
    ESP_LOGI(TAG, "Detected Generic HID Protocol");
    return true;
  }
  
  ESP_LOGE(TAG, "No compatible UPS protocol detected");
  return false;
}

bool NutUpsComponent::read_ups_data() {
  if (!active_protocol_) {
    return false;
  }
  
  return active_protocol_->read_data(ups_data_);
}

void NutUpsComponent::update_sensors() {
  // Update numeric sensors
  for (auto &pair : sensors_) {
    const std::string &type = pair.first;
    auto *sensor = pair.second;
    
    if (type == "battery_level" && !std::isnan(ups_data_.battery_level)) {
      sensor->publish_state(ups_data_.battery_level);
    } else if (type == "input_voltage" && !std::isnan(ups_data_.input_voltage)) {
      sensor->publish_state(ups_data_.input_voltage);
    } else if (type == "output_voltage" && !std::isnan(ups_data_.output_voltage)) {
      sensor->publish_state(ups_data_.output_voltage);
    } else if (type == "load_percent" && !std::isnan(ups_data_.load_percent)) {
      sensor->publish_state(ups_data_.load_percent);
    } else if (type == "runtime" && !std::isnan(ups_data_.runtime_minutes)) {
      sensor->publish_state(ups_data_.runtime_minutes);
    } else if (type == "frequency" && !std::isnan(ups_data_.frequency)) {
      sensor->publish_state(ups_data_.frequency);
    }
  }
  
  // Update binary sensors
  for (auto &pair : binary_sensors_) {
    const std::string &type = pair.first;
    auto *sensor = pair.second;
    
    if (type == "online") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_ONLINE);
    } else if (type == "on_battery") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_ON_BATTERY);
    } else if (type == "low_battery") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_LOW_BATTERY);
    } else if (type == "fault") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_FAULT);
    } else if (type == "overload") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_OVERLOAD);
    } else if (type == "charging") {
      sensor->publish_state(ups_data_.status_flags & UPS_STATUS_CHARGING);
    }
  }
  
  // Update text sensors
  for (auto &pair : text_sensors_) {
    const std::string &type = pair.first;
    auto *sensor = pair.second;
    
    if (type == "model" && !ups_data_.model.empty()) {
      sensor->publish_state(ups_data_.model);
    } else if (type == "manufacturer" && !ups_data_.manufacturer.empty()) {
      sensor->publish_state(ups_data_.manufacturer);
    } else if (type == "protocol") {
      sensor->publish_state(get_protocol_name());
    } else if (type == "status") {
      std::string status_str = "";
      if (ups_data_.status_flags & UPS_STATUS_ONLINE) status_str += "Online ";
      if (ups_data_.status_flags & UPS_STATUS_ON_BATTERY) status_str += "OnBattery ";
      if (ups_data_.status_flags & UPS_STATUS_LOW_BATTERY) status_str += "LowBattery ";
      if (ups_data_.status_flags & UPS_STATUS_CHARGING) status_str += "Charging ";
      if (ups_data_.status_flags & UPS_STATUS_FAULT) status_str += "Fault ";
      if (status_str.empty()) status_str = "Unknown";
      sensor->publish_state(status_str);
    }
  }
}

void NutUpsComponent::simulate_ups_data() {
  static uint32_t sim_counter = 0;
  sim_counter++;
  
  // Simulate realistic UPS data that changes over time
  ups_data_.battery_level = 85.0f + sin(sim_counter * 0.01f) * 10.0f;
  ups_data_.input_voltage = 120.0f + sin(sim_counter * 0.02f) * 5.0f;
  ups_data_.output_voltage = 118.0f + sin(sim_counter * 0.015f) * 3.0f;
  ups_data_.load_percent = 45.0f + sin(sim_counter * 0.005f) * 15.0f;
  ups_data_.runtime_minutes = 35.0f + sin(sim_counter * 0.003f) * 10.0f;
  ups_data_.frequency = 60.0f + sin(sim_counter * 0.1f) * 0.2f;
  
  // Simulate status changes
  if (sim_counter % 1000 < 800) {
    ups_data_.status_flags = UPS_STATUS_ONLINE | UPS_STATUS_CHARGING;
  } else if (sim_counter % 1000 < 950) {
    ups_data_.status_flags = UPS_STATUS_ON_BATTERY;
  } else {
    ups_data_.status_flags = UPS_STATUS_ON_BATTERY | UPS_STATUS_LOW_BATTERY;
  }
  
  // Keep simulated device info
  ups_data_.manufacturer = "Simulated";
  ups_data_.model = "Virtual UPS Pro";
}

void NutUpsComponent::register_sensor(sensor::Sensor *sens, const std::string &type) {
  sensors_[type] = sens;
}

void NutUpsComponent::register_binary_sensor(binary_sensor::BinarySensor *sens, const std::string &type) {
  binary_sensors_[type] = sens;
}

void NutUpsComponent::register_text_sensor(text_sensor::TextSensor *sens, const std::string &type) {
  text_sensors_[type] = sens;
}

bool NutUpsComponent::usb_write(const std::vector<uint8_t> &data) {
#ifdef USE_ESP32
  // Placeholder for actual USB write implementation
  // This would use ESP-IDF USB Host APIs in a real implementation
  ESP_LOGV(TAG, "USB Write: %d bytes", data.size());
  return true;
#else
  return false;
#endif
}

bool NutUpsComponent::usb_read(std::vector<uint8_t> &data, uint32_t timeout_ms) {
#ifdef USE_ESP32
  // Placeholder for actual USB read implementation
  // This would use ESP-IDF USB Host APIs in a real implementation
  ESP_LOGV(TAG, "USB Read with timeout: %u ms", timeout_ms);
  data.clear();
  return true;
#else
  return false;
#endif
}

#ifdef USE_ESP32
esp_err_t NutUpsComponent::usb_init() {
  // Placeholder for USB initialization
  // In a real implementation, this would initialize the ESP32 USB Host
  ESP_LOGD(TAG, "Initializing USB Host...");
  return ESP_OK;
}

void NutUpsComponent::usb_deinit() {
  // Placeholder for USB cleanup
  ESP_LOGD(TAG, "Deinitializing USB Host...");
}
#endif

// Base Protocol Implementation
bool UpsProtocolBase::send_command(const std::vector<uint8_t> &cmd, std::vector<uint8_t> &response, uint32_t timeout_ms) {
  if (!parent_->usb_write(cmd)) {
    return false;
  }
  
  return parent_->usb_read(response, timeout_ms);
}

std::string UpsProtocolBase::bytes_to_string(const std::vector<uint8_t> &data) {
  std::string result;
  result.reserve(data.size());
  for (uint8_t byte : data) {
    if (byte >= 32 && byte <= 126) {  // Printable ASCII
      result += static_cast<char>(byte);
    }
  }
  return result;
}

}  // namespace nut_ups
}  // namespace esphome