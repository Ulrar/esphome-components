#include "nut_ups.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include <cstring>

// USB HID Class defines
#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03
#endif

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
  
  esp_err_t ret = usb_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "USB initialization failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Try to enumerate and connect to UPS device
  ret = usb_device_enumerate();
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "No UPS devices found during initial enumeration");
    // Don't fail completely - device might be connected later
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
  if (!device_connected_ || data.empty()) {
    return false;
  }

  // Take USB mutex for thread safety
  if (xSemaphoreTake(usb_mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to acquire USB mutex for write");
    return false;
  }

  std::vector<uint8_t> dummy_response;
  esp_err_t ret = usb_transfer_sync(data, dummy_response, 1000);
  
  xSemaphoreGive(usb_mutex_);
  
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "USB write failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  ESP_LOGV(TAG, "USB Write: %d bytes", data.size());
  return true;
#else
  return false;
#endif
}

bool NutUpsComponent::usb_read(std::vector<uint8_t> &data, uint32_t timeout_ms) {
#ifdef USE_ESP32
  if (!device_connected_) {
    data.clear();
    return false;
  }

  // Take USB mutex for thread safety  
  if (xSemaphoreTake(usb_mutex_, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to acquire USB mutex for read");
    data.clear();
    return false;
  }

  std::vector<uint8_t> dummy_out;
  esp_err_t ret = usb_transfer_sync(dummy_out, data, timeout_ms);
  
  xSemaphoreGive(usb_mutex_);
  
  if (ret != ESP_OK) {
    ESP_LOGV(TAG, "USB read failed: %s", esp_err_to_name(ret));
    data.clear();
    return false;
  }
  
  ESP_LOGV(TAG, "USB Read: %d bytes", data.size());
  return true;
#else
  data.clear();
  return false;
#endif
}

#ifdef USE_ESP32
esp_err_t NutUpsComponent::usb_init() {
  ESP_LOGD(TAG, "Initializing USB Host...");
  
  if (usb_host_initialized_) {
    ESP_LOGW(TAG, "USB Host already initialized");
    return ESP_OK;
  }
  
  // Create mutex for USB operations
  usb_mutex_ = xSemaphoreCreateMutex();
  if (!usb_mutex_) {
    ESP_LOGE(TAG, "Failed to create USB mutex");
    return ESP_ERR_NO_MEM;
  }
  
  // Initialize USB Host Library
  esp_err_t ret = usb_host_lib_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "USB Host lib init failed: %s", esp_err_to_name(ret));
    return ret;
  }
  
  // Register USB client
  ret = usb_client_register();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "USB client register failed: %s", esp_err_to_name(ret));
    return ret;
  }
  
  usb_host_initialized_ = true;
  ESP_LOGI(TAG, "USB Host initialized successfully");
  return ESP_OK;
}

void NutUpsComponent::usb_deinit() {
  ESP_LOGD(TAG, "Deinitializing USB Host...");
  
  if (!usb_host_initialized_) {
    return;
  }
  
  // Stop USB Host task
  if (usb_task_handle_) {
    vTaskDelete(usb_task_handle_);
    usb_task_handle_ = nullptr;
  }
  
  // Close USB device if connected
  if (usb_device_.dev_hdl) {
    usb_host_device_close(usb_device_.client_hdl, usb_device_.dev_hdl);
    usb_device_.dev_hdl = nullptr;
  }
  
  // Deregister USB client
  if (usb_device_.client_hdl) {
    usb_host_client_deregister(usb_device_.client_hdl);
    usb_device_.client_hdl = nullptr;
  }
  
  // Uninstall USB Host Library
  usb_host_uninstall();
  
  // Delete mutex
  if (usb_mutex_) {
    vSemaphoreDelete(usb_mutex_);
    usb_mutex_ = nullptr;
  }
  
  usb_host_initialized_ = false;
  device_connected_ = false;
  
  ESP_LOGI(TAG, "USB Host deinitialized");
}

esp_err_t NutUpsComponent::usb_host_lib_init() {
  usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  
  esp_err_t ret = usb_host_install(&host_config);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE means already initialized
    return ret;
  }
  
  // Create USB Host Library task
  if (xTaskCreate(usb_host_lib_task, "usb_host", 4096, this, 5, &usb_task_handle_) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to create USB Host task");
    return ESP_ERR_NO_MEM;
  }
  
  return ESP_OK;
}

esp_err_t NutUpsComponent::usb_client_register() {
  usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 5,
    .async = {
      .client_event_callback = usb_client_event_callback,
      .callback_arg = this,
    }
  };
  
  esp_err_t ret = usb_host_client_register(&client_config, &usb_device_.client_hdl);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "USB client register failed: %s", esp_err_to_name(ret));
    return ret;
  }
  
  return ESP_OK;
}

esp_err_t NutUpsComponent::usb_device_enumerate() {
  ESP_LOGD(TAG, "Enumerating USB devices...");
  
  uint8_t dev_addr_list[10];
  int num_dev = 10;
  
  esp_err_t ret = usb_host_device_addr_list_fill(num_dev, dev_addr_list, &num_dev);
  if (ret != ESP_OK) {
    return ret;
  }
  
  ESP_LOGD(TAG, "Found %d USB devices", num_dev);
  
  for (int i = 0; i < num_dev; i++) {
    usb_device_handle_t dev_hdl;
    ret = usb_host_device_open(usb_device_.client_hdl, dev_addr_list[i], &dev_hdl);
    if (ret != ESP_OK) {
      continue;
    }
    
    const usb_device_desc_t *dev_desc;
    ret = usb_host_get_device_descriptor(dev_hdl, &dev_desc);
    if (ret == ESP_OK && usb_is_ups_device(dev_desc)) {
      // Found UPS device
      usb_device_.dev_hdl = dev_hdl;
      usb_device_.dev_addr = dev_addr_list[i];
      usb_device_.vid = dev_desc->idVendor;
      usb_device_.pid = dev_desc->idProduct;
      
      ESP_LOGI(TAG, "Found UPS device: VID=0x%04X, PID=0x%04X", usb_device_.vid, usb_device_.pid);
      
      // Claim interface and get endpoints
      if (usb_claim_interface() == ESP_OK && usb_get_endpoints() == ESP_OK) {
        device_connected_ = true;
        return ESP_OK;
      }
    }
    
    usb_host_device_close(usb_device_.client_hdl, dev_hdl);
  }
  
  return ESP_ERR_NOT_FOUND;
}

bool NutUpsComponent::usb_is_ups_device(const usb_device_desc_t *desc) {
  // Check if this is a known UPS device
  // Common UPS vendors: APC (0x051D), CyberPower (0x0764), Tripp Lite (0x09AE)
  uint16_t vid = desc->idVendor;
  uint16_t pid = desc->idProduct;
  
  ESP_LOGV(TAG, "Checking device: VID=0x%04X, PID=0x%04X", vid, pid);
  
  // Check configured VID/PID first
  if (vid == usb_vendor_id_ && pid == usb_product_id_) {
    ESP_LOGD(TAG, "Device matches configured VID/PID");
    return true;
  }
  
  // Check known UPS vendor IDs
  switch (vid) {
    case 0x051D: // APC
    case 0x0764: // CyberPower  
    case 0x09AE: // Tripp Lite
    case 0x06DA: // MGE UPS Systems
    case 0x0665: // Cypress (some UPS devices)
      ESP_LOGD(TAG, "Recognized UPS vendor: 0x%04X", vid);
      return true;
    default:
      break;
  }
  
  // Check device class (some UPS devices use HID class)
  if (desc->bDeviceClass == USB_CLASS_HID ||
      (desc->bDeviceClass == 0 && desc->bDeviceSubClass == 0)) {
    ESP_LOGV(TAG, "Device might be HID-compatible UPS");
    return true; // Could be a HID UPS
  }
  
  return false;
}

esp_err_t NutUpsComponent::usb_claim_interface() {
  // Get active configuration
  const usb_config_desc_t *config_desc;
  esp_err_t ret = usb_host_get_active_config_descriptor(usb_device_.dev_hdl, &config_desc);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get config descriptor: %s", esp_err_to_name(ret));
    return ret;
  }
  
  // Find HID interface
  const usb_intf_desc_t *intf_desc = nullptr;
  int offset = 0;
  
  for (int i = 0; i < config_desc->bNumInterfaces; i++) {
    intf_desc = usb_parse_interface_descriptor(config_desc, i, 0, &offset);
    if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_HID) {
      usb_device_.interface_num = intf_desc->bInterfaceNumber;
      usb_device_.is_hid_device = true;
      break;
    }
  }
  
  if (!intf_desc) {
    ESP_LOGE(TAG, "No HID interface found");
    return ESP_ERR_NOT_FOUND;
  }
  
  // Claim the interface
  ret = usb_host_interface_claim(usb_device_.client_hdl, usb_device_.dev_hdl, 
                                 usb_device_.interface_num, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to claim interface: %s", esp_err_to_name(ret));
    return ret;
  }
  
  ESP_LOGD(TAG, "Claimed interface %d", usb_device_.interface_num);
  return ESP_OK;
}

esp_err_t NutUpsComponent::usb_get_endpoints() {
  // Get active configuration
  const usb_config_desc_t *config_desc;
  esp_err_t ret = usb_host_get_active_config_descriptor(usb_device_.dev_hdl, &config_desc);
  if (ret != ESP_OK) {
    return ret;
  }
  
  // Find HID interface first
  const usb_intf_desc_t *intf_desc = nullptr;
  int intf_offset = 0;
  
  for (int i = 0; i < config_desc->bNumInterfaces; i++) {
    intf_desc = usb_parse_interface_descriptor(config_desc, i, 0, &intf_offset);
    if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_HID) {
      break;
    }
  }
  
  if (!intf_desc) {
    ESP_LOGE(TAG, "No HID interface found for endpoints");
    return ESP_ERR_NOT_FOUND;
  }
  
  // Find endpoints in the HID interface
  int ep_index = 0;
  const usb_ep_desc_t *ep_desc;
  
  while ((ep_desc = usb_parse_endpoint_descriptor_by_index(intf_desc, ep_index, 0, nullptr)) != nullptr) {
    if (USB_EP_DESC_GET_EP_DIR(ep_desc)) { // IN endpoint
      usb_device_.ep_in = ep_desc->bEndpointAddress;
      usb_device_.max_packet_size = ep_desc->wMaxPacketSize;
      ESP_LOGD(TAG, "Found IN endpoint: 0x%02X, max packet: %d", 
               usb_device_.ep_in, usb_device_.max_packet_size);
    } else { // OUT endpoint
      usb_device_.ep_out = ep_desc->bEndpointAddress;
      ESP_LOGD(TAG, "Found OUT endpoint: 0x%02X", usb_device_.ep_out);
    }
    ep_index++;
  }
  
  if (usb_device_.ep_in == 0) {
    ESP_LOGE(TAG, "No IN endpoint found");
    return ESP_ERR_NOT_FOUND;
  }
  
  return ESP_OK;
}

esp_err_t NutUpsComponent::usb_transfer_sync(const std::vector<uint8_t> &data_out, 
                                             std::vector<uint8_t> &data_in, 
                                             uint32_t timeout_ms) {
  if (!device_connected_ || !usb_device_.dev_hdl) {
    return ESP_ERR_INVALID_STATE;
  }
  
  esp_err_t ret = ESP_OK;
  
  // Send data if provided
  if (!data_out.empty() && usb_device_.ep_out != 0) {
    usb_transfer_t *transfer_out;
    ret = usb_host_transfer_alloc(data_out.size(), 0, &transfer_out);
    if (ret != ESP_OK) {
      return ret;
    }
    
    transfer_out->device_handle = usb_device_.dev_hdl;
    transfer_out->bEndpointAddress = usb_device_.ep_out;
    transfer_out->callback = nullptr;
    transfer_out->context = this;
    transfer_out->num_bytes = data_out.size();
    std::memcpy(transfer_out->data_buffer, data_out.data(), data_out.size());
    
    ret = usb_host_transfer_submit(transfer_out);
    if (ret == ESP_OK) {
      // Wait for completion
      vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    }
    
    usb_host_transfer_free(transfer_out);
    if (ret != ESP_OK) {
      return ret;
    }
  }
  
  // Read data if IN endpoint exists
  if (usb_device_.ep_in != 0) {
    usb_transfer_t *transfer_in;
    size_t buffer_size = std::max(static_cast<size_t>(64), static_cast<size_t>(usb_device_.max_packet_size));
    ret = usb_host_transfer_alloc(buffer_size, 0, &transfer_in);
    if (ret != ESP_OK) {
      return ret;
    }
    
    transfer_in->device_handle = usb_device_.dev_hdl;
    transfer_in->bEndpointAddress = usb_device_.ep_in;
    transfer_in->callback = nullptr;
    transfer_in->context = this;
    transfer_in->num_bytes = buffer_size;
    
    ret = usb_host_transfer_submit(transfer_in);
    if (ret == ESP_OK) {
      // Wait for completion
      vTaskDelay(pdMS_TO_TICKS(timeout_ms));
      
      // Copy received data
      if (transfer_in->actual_num_bytes > 0) {
        data_in.resize(transfer_in->actual_num_bytes);
        std::memcpy(data_in.data(), transfer_in->data_buffer, transfer_in->actual_num_bytes);
      }
    }
    
    usb_host_transfer_free(transfer_in);
  }
  
  return ret;
}

void NutUpsComponent::usb_host_lib_task(void *arg) {
  auto *component = static_cast<NutUpsComponent *>(arg);
  
  ESP_LOGD(TAG, "USB Host task started");
  
  while (component->usb_host_initialized_) {
    uint32_t event_flags;
    esp_err_t ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "USB Host lib handle events error: %s", esp_err_to_name(ret));
      continue;
    }
    
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_LOGD(TAG, "No USB clients");
      usb_host_device_free_all();
    }
    
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      ESP_LOGD(TAG, "All USB devices freed");
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  ESP_LOGD(TAG, "USB Host task ended");
  vTaskDelete(nullptr);
}

void NutUpsComponent::usb_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
  auto *component = static_cast<NutUpsComponent *>(arg);
  
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGD(TAG, "New USB device connected: address %d", event_msg->new_dev.address);
      // Trigger device enumeration
      component->usb_device_enumerate();
      break;
      
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      ESP_LOGD(TAG, "USB device disconnected: handle %p", event_msg->dev_gone.dev_hdl);
      if (component->usb_device_.dev_hdl == event_msg->dev_gone.dev_hdl) {
        component->device_connected_ = false;
        component->usb_device_.dev_hdl = nullptr;
      }
      break;
      
    default:
      ESP_LOGV(TAG, "USB client event: %d", event_msg->event);
      break;
  }
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