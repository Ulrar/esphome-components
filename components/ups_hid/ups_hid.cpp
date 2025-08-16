#include "ups_hid.h"
#include "ups_vendors.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

// Include protocol implementations
#include "apc_hid_protocol.h"

#include <functional>
#include <cmath>

#ifdef USE_ESP32
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include <cstring>

// USB HID Class defines
#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03
#endif

#endif

namespace esphome
{
  namespace ups_hid
  {

    void UpsHidComponent::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up UPS HID Component...");
      ESP_LOGI(TAG, "Setup method called - checking simulation mode: %s", simulation_mode_ ? "YES" : "NO");

      if (simulation_mode_)
      {
        ESP_LOGW(TAG, "Running in simulation mode - no actual UPS communication");

        // Initialize simulation data consistently
        {
          std::lock_guard<std::mutex> lock(data_mutex_);
          ups_data_.detected_protocol = PROTOCOL_APC_SMART;
          ups_data_.manufacturer = "Simulated";
          ups_data_.model = "Virtual UPS Pro";
          ups_data_.serial_number = "SIM123456789";
          ups_data_.firmware_version = "1.0.0-SIM";

          // Initialize with reasonable default values
          ups_data_.battery_level = 85.0f;
          ups_data_.input_voltage = 120.0f;
          ups_data_.output_voltage = 118.0f;
          ups_data_.load_percent = 45.0f;
          ups_data_.runtime_minutes = 35.0f;
          ups_data_.frequency = 60.0f;
          ups_data_.status_flags = UPS_STATUS_ONLINE | UPS_STATUS_CHARGING;
        }

        connected_ = true;
        last_successful_read_ = millis();
        ESP_LOGI(TAG, "Simulation mode initialized successfully");
        return;
      }

      ESP_LOGI(TAG, "About to check USE_ESP32 define...");
#ifdef USE_ESP32
      ESP_LOGI(TAG, "USE_ESP32 is defined - initializing USB...");
      if (!initialize_usb())
      {
        ESP_LOGE(TAG, "Failed to initialize USB");
        mark_failed();
        return;
      }
#else
      ESP_LOGE(TAG, "USE_ESP32 is NOT defined - cannot initialize USB!");
      mark_failed();
      return;
#endif

      if (!detect_ups_protocol())
      {
        ESP_LOGE(TAG, "Failed to detect UPS protocol");
        connected_ = false;
        return;
      }

      connected_ = true;
      ESP_LOGI(TAG, "Successfully connected to UPS using %s", get_protocol_name().c_str());
    }

    void UpsHidComponent::update()
    {
      if (simulation_mode_)
      {
        simulate_ups_data();
        update_sensors();
        return;
      }

      if (!connected_)
      {
        ESP_LOGD(TAG, "UPS not connected, attempting to reconnect...");

        // Try USB re-enumeration first
#ifdef USE_ESP32
        esp_err_t ret = usb_device_enumerate();
        if (ret == ESP_OK)
        {
          ESP_LOGI(TAG, "USB device enumeration successful, attempting protocol detection");
        }
#endif

        if (detect_ups_protocol())
        {
          connected_ = true;
          ESP_LOGI(TAG, "Reconnected to UPS");
        }
        else
        {
          return;
        }
      }

      if (read_ups_data())
      {
        last_successful_read_ = millis();
        consecutive_failures_ = 0;
        update_sensors();
      }
      else
      {
        consecutive_failures_++;
        if (should_log_error(protocol_error_limiter_))
        {
          ESP_LOGW(TAG, "Failed to read UPS data (failure #%u)", consecutive_failures_);
        }

        // Implement limited retry logic with exponential backoff
        if (consecutive_failures_ >= 3 && consecutive_failures_ <= max_consecutive_failures_)
        {
          ESP_LOGE(TAG, "Multiple consecutive failures, attempting protocol re-detection");
          if (!detect_ups_protocol())
          {
            ESP_LOGE(TAG, "Protocol re-detection failed");
          }
        }
        else if (consecutive_failures_ > max_consecutive_failures_)
        {
          ESP_LOGE(TAG, "Maximum re-detection attempts reached, giving up");
        }

        if (millis() - last_successful_read_ > protocol_timeout_ms_)
        {
          ESP_LOGE(TAG, "UPS communication timeout, marking as disconnected");
          connected_ = false;
          consecutive_failures_ = 0;
        }
      }
    }

    void UpsHidComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "UPS HID Component:");
      ESP_LOGCONFIG(TAG, "  Simulation Mode: %s", simulation_mode_ ? "YES" : "NO");
      ESP_LOGCONFIG(TAG, "  USB Vendor ID: 0x%04X", usb_vendor_id_);
      ESP_LOGCONFIG(TAG, "  USB Product ID: 0x%04X", usb_product_id_);
      ESP_LOGCONFIG(TAG, "  Protocol Timeout: %u ms", protocol_timeout_ms_);
      ESP_LOGCONFIG(TAG, "  Auto Detect Protocol: %s", auto_detect_protocol_ ? "YES" : "NO");
      ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", get_update_interval());

      if (connected_)
      {
        ESP_LOGCONFIG(TAG, "  Status: Connected");
        ESP_LOGCONFIG(TAG, "  Protocol: %s", get_protocol_name().c_str());
        ESP_LOGCONFIG(TAG, "  Manufacturer: %s", ups_data_.manufacturer.c_str());
        ESP_LOGCONFIG(TAG, "  Model: %s", ups_data_.model.c_str());
      }
      else
      {
        ESP_LOGCONFIG(TAG, "  Status: Disconnected");
      }
    }

    std::string UpsHidComponent::get_protocol_name() const
    {
      if (active_protocol_)
      {
        return active_protocol_->get_protocol_name();
      }
      return "Unknown";
    }

    bool UpsHidComponent::initialize_usb()
    {
#ifdef USE_ESP32
      ESP_LOGI(TAG, "Initializing USB communication...");
      ESP_LOGI(TAG, "ESP32-S3 USB OTG Host Mode Configuration:");
      ESP_LOGI(TAG, "  Board: ESP32-S3-DevKitC-1");
      ESP_LOGI(TAG, "  USB OTG Pins: D+ (GPIO20), D- (GPIO19)");
      ESP_LOGI(TAG, "  USB Host Mode: Enabled");

      // Add delay to ensure USB hardware is ready
      vTaskDelay(pdMS_TO_TICKS(100));

      esp_err_t ret = usb_init();
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "USB initialization failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "USB Troubleshooting:");
        ESP_LOGE(TAG, "  1. Verify USB OTG pads are soldered correctly");
        ESP_LOGE(TAG, "  2. Check UPS is powered ON and connected");
        ESP_LOGE(TAG, "  3. Try different USB cable");
        ESP_LOGE(TAG, "  4. Verify ESP32-S3 has USB host capability");
        return false;
      }

      ESP_LOGI(TAG, "USB Host initialized, waiting for device enumeration...");
      // Wait longer for devices to be detected
      vTaskDelay(pdMS_TO_TICKS(2000));

      // Try to enumerate and connect to UPS device
      ret = usb_device_enumerate();
      if (ret != ESP_OK)
      {
        ESP_LOGW(TAG, "No UPS devices found during initial enumeration");
        ESP_LOGW(TAG, "Device enumeration troubleshooting:");
        ESP_LOGW(TAG, "  1. Check if UPS is USB HID compatible");
        ESP_LOGW(TAG, "  2. Try connecting UPS to a computer first");
        ESP_LOGW(TAG, "  3. Some UPS devices need to be 'awake' (press a button)");
        ESP_LOGW(TAG, "  4. Verify USB cable supports data (not just power)");
        // Don't fail completely - device might be connected later
      }

      ESP_LOGI(TAG, "USB initialized successfully");
      return true;
#else
      return false;
#endif
    }

    bool UpsHidComponent::should_log_error(ErrorRateLimit &limiter)
    {
      uint32_t now = millis();

      // Check if we're past the rate limit window
      if (now - limiter.last_error_time > ErrorRateLimit::RATE_LIMIT_MS)
      {
        // Reset the limiter for new time window
        if (limiter.suppressed_count > 0)
        {
          // Log how many errors we suppressed in the previous window
          ESP_LOGW(TAG, "Suppressed %u similar error messages in the last %u ms",
                   limiter.suppressed_count, ErrorRateLimit::RATE_LIMIT_MS);
          limiter.suppressed_count = 0;
        }
        limiter.error_count = 0;
        limiter.last_error_time = now;
      }

      limiter.error_count++;

      // Allow first few errors in burst, then rate limit
      if (limiter.error_count <= ErrorRateLimit::MAX_BURST)
      {
        return true;
      }

      // Rate limit subsequent errors
      limiter.suppressed_count++;
      return false;
    }

    void UpsHidComponent::log_suppressed_errors(ErrorRateLimit &limiter)
    {
      if (limiter.suppressed_count > 0)
      {
        ESP_LOGW(TAG, "Suppressed %u similar error messages", limiter.suppressed_count);
        limiter.suppressed_count = 0;
      }
    }

    bool UpsHidComponent::detect_ups_protocol()
    {
      ESP_LOGD(TAG, "Detecting UPS protocol...");

      if (!auto_detect_protocol_)
      {
        // Use APC Smart Protocol as default when auto-detection is disabled
        active_protocol_ = std::make_unique<ApcSmartProtocol>(this);
        if (active_protocol_->initialize())
        {
          ESP_LOGI(TAG, "Using pre-configured protocol: %s", active_protocol_->get_protocol_name().c_str());
          ups_data_.detected_protocol = active_protocol_->get_protocol_type();
          return true;
        }
        return false;
      }

      // Get the actual USB device vendor/product IDs (may differ from configured defaults)
      uint16_t detected_vid = usb_vendor_id_;
      uint16_t detected_pid = usb_product_id_;

#ifdef USE_ESP32
      // Try to get actual device IDs if USB device is connected
      if (device_connected_ && usb_device_.dev_hdl)
      {
        detected_vid = usb_device_.vid;
        detected_pid = usb_device_.pid;
      }
#endif

      // Log vendor information for debugging
      if (is_known_ups_vendor(detected_vid))
      {
        const char *vendor_name = get_ups_vendor_name(detected_vid);
        ESP_LOGD(TAG, "Detected known UPS vendor: %s (0x%04X)", vendor_name, detected_vid);
      }
      else
      {
        ESP_LOGD(TAG, "Unknown vendor ID: 0x%04X (trying generic protocols)", detected_vid);
      }

      // Vendor-specific protocol detection with retry logic
      std::vector<std::pair<std::string, std::function<bool()>>> protocol_attempts;

      // Build protocol detection list based on vendor ID
      switch (detected_vid)
      {
      case 0x051D: // APC
        ESP_LOGD(TAG, "APC device detected, trying HID first, then Smart protocol");
        protocol_attempts.push_back({"APC HID", [this]()
                                     {
                                       auto protocol = std::make_unique<ApcHidProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_APC_HID;
                                         return true;
                                       }
                                       return false;
                                     }});
        protocol_attempts.push_back({"APC Smart", [this]()
                                     {
                                       auto protocol = std::make_unique<ApcSmartProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_APC_SMART;
                                         return true;
                                       }
                                       return false;
                                     }});
        break;

      case 0x0764: // CyberPower
        ESP_LOGD(TAG, "CyberPower device detected, trying CyberPower HID protocol");
        protocol_attempts.push_back({"CyberPower HID", [this]()
                                     {
                                       auto protocol = std::make_unique<CyberPowerProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_CYBERPOWER_HID;
                                         return true;
                                       }
                                       return false;
                                     }});
        break;

      case 0x09AE: // Tripp Lite
      case 0x06DA: // MGE UPS Systems (now Eaton)
      case 0x0463: // MGE Office Protection Systems
      case 0x050D: // Belkin
      case 0x0665: // Cypress/Belkin
        ESP_LOGD(TAG, "Known UPS vendor detected (0x%04X), trying Generic HID", detected_vid);
        protocol_attempts.push_back({"Generic HID", [this]()
                                     {
                                       auto protocol = std::make_unique<GenericHidProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_GENERIC_HID;
                                         return true;
                                       }
                                       return false;
                                     }});
        break;

      default:
        // Unknown vendor - try all protocols with enhanced detection
        ESP_LOGD(TAG, "Unknown vendor (0x%04X), trying all protocols", detected_vid);
        protocol_attempts.push_back({"APC HID", [this]()
                                     {
                                       auto protocol = std::make_unique<ApcHidProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_APC_HID;
                                         return true;
                                       }
                                       return false;
                                     }});
        protocol_attempts.push_back({"APC Smart", [this]()
                                     {
                                       auto protocol = std::make_unique<ApcSmartProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_APC_SMART;
                                         return true;
                                       }
                                       return false;
                                     }});
        protocol_attempts.push_back({"CyberPower HID", [this]()
                                     {
                                       auto protocol = std::make_unique<CyberPowerProtocol>(this);
                                       if (protocol->detect() && protocol->initialize())
                                       {
                                         active_protocol_ = std::move(protocol);
                                         ups_data_.detected_protocol = PROTOCOL_CYBERPOWER_HID;
                                         return true;
                                       }
                                       return false;
                                     }});
        break;
      }

      // Always add Generic HID as final fallback
      protocol_attempts.push_back({"Generic HID", [this]()
                                   {
                                     auto protocol = std::make_unique<GenericHidProtocol>(this);
                                     if (protocol->detect() && protocol->initialize())
                                     {
                                       active_protocol_ = std::move(protocol);
                                       ups_data_.detected_protocol = PROTOCOL_GENERIC_HID;
                                       return true;
                                     }
                                     return false;
                                   }});

      // Try each protocol with timeout and retry logic
      for (const auto &attempt : protocol_attempts)
      {
        ESP_LOGD(TAG, "Trying %s protocol...", attempt.first.c_str());

        // Attempt detection with timeout
        uint32_t start_time = millis();
        bool success = false;

        // Try detection with rate limiting
        if (!should_log_error(protocol_error_limiter_))
        {
          ESP_LOGV(TAG, "Protocol detection rate limited, skipping %s", attempt.first.c_str());
          continue;
        }

        success = attempt.second();

        uint32_t detection_time = millis() - start_time;

        if (success)
        {
          ESP_LOGI(TAG, "Successfully detected %s protocol (took %ums)",
                   attempt.first.c_str(), detection_time);
          return true;
        }
        else
        {
          ESP_LOGD(TAG, "%s protocol detection failed (took %ums)",
                   attempt.first.c_str(), detection_time);

          // Small delay between attempts to prevent overwhelming the device
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      }

      ESP_LOGE(TAG, "No compatible UPS protocol detected for vendor 0x%04X", detected_vid);
      return false;
    }

    bool UpsHidComponent::read_ups_data()
    {
      if (!active_protocol_)
      {
        ESP_LOGW(TAG, "No active protocol available for reading data");
        return false;
      }

      // Create temporary data structure to avoid corrupting existing data on failure
      UpsData temp_data;
      {
        std::lock_guard<std::mutex> lock(data_mutex_);
        temp_data = ups_data_;
      }

      if (active_protocol_->read_data(temp_data))
      {
        // Only update if we got valid data
        if (temp_data.is_valid())
        {
          std::lock_guard<std::mutex> lock(data_mutex_);
          ups_data_ = temp_data;
          return true;
        }
        else
        {
          ESP_LOGW(TAG, "Protocol returned invalid data");
          return false;
        }
      }

      ESP_LOGV(TAG, "Failed to read data from protocol: %s", active_protocol_->get_protocol_name().c_str());
      return false;
    }

    void UpsHidComponent::update_sensors()
    {
      // Create local copy of data under lock to minimize lock time
      UpsData local_data;
      {
        std::lock_guard<std::mutex> lock(data_mutex_);
        local_data = ups_data_;
      }

      // Update numeric sensors with validation
      for (const auto &[type, sensor] : sensors_)
      {
        if (!sensor)
          continue; // Safety check

        float value = NAN;

        if (type == "battery_level")
        {
          value = local_data.battery_level;
        }
        else if (type == "input_voltage")
        {
          value = local_data.input_voltage;
        }
        else if (type == "output_voltage")
        {
          value = local_data.output_voltage;
        }
        else if (type == "load_percent")
        {
          value = local_data.load_percent;
        }
        else if (type == "runtime")
        {
          value = local_data.runtime_minutes;
        }
        else if (type == "frequency")
        {
          value = local_data.frequency;
        }

        // Only publish if value is valid and within reasonable bounds
        if (!std::isnan(value))
        {
          // Add bounds checking for safety
          if (type == "battery_level" && (value < 0.0f || value > 100.0f))
          {
            ESP_LOGW(TAG, "Battery level out of bounds: %.1f%%", value);
            continue;
          }
          if (type == "load_percent" && (value < 0.0f || value > 100.0f))
          {
            ESP_LOGW(TAG, "Load percentage out of bounds: %.1f%%", value);
            continue;
          }

          sensor->publish_state(value);
        }
      }

      // Update binary sensors
      for (auto &pair : binary_sensors_)
      {
        const std::string &type = pair.first;
        auto *sensor = pair.second;

        if (type == "online")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_ONLINE);
        }
        else if (type == "on_battery")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_ON_BATTERY);
        }
        else if (type == "low_battery")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_LOW_BATTERY);
        }
        else if (type == "fault")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_FAULT);
        }
        else if (type == "overload")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_OVERLOAD);
        }
        else if (type == "charging")
        {
          sensor->publish_state(local_data.status_flags & UPS_STATUS_CHARGING);
        }
      }

      // Update text sensors
      for (auto &pair : text_sensors_)
      {
        const std::string &type = pair.first;
        auto *sensor = pair.second;

        if (type == "model" && !local_data.model.empty())
        {
          sensor->publish_state(local_data.model);
        }
        else if (type == "manufacturer" && !local_data.manufacturer.empty())
        {
          sensor->publish_state(local_data.manufacturer);
        }
        else if (type == "protocol")
        {
          sensor->publish_state(get_protocol_name());
        }
        else if (type == "status")
        {
          std::string status_str = "";
          if (local_data.status_flags & UPS_STATUS_ONLINE)
            status_str += "Online ";
          if (local_data.status_flags & UPS_STATUS_ON_BATTERY)
            status_str += "OnBattery ";
          if (local_data.status_flags & UPS_STATUS_LOW_BATTERY)
            status_str += "LowBattery ";
          if (local_data.status_flags & UPS_STATUS_CHARGING)
            status_str += "Charging ";
          if (local_data.status_flags & UPS_STATUS_FAULT)
            status_str += "Fault ";
          if (status_str.empty())
            status_str = "Unknown";
          sensor->publish_state(status_str);
        }
      }
    }

    void UpsHidComponent::simulate_ups_data()
    {
      uint32_t now_ms = millis();
      static uint32_t last_log_time = 0;

      // Use time-based simulation for consistent behavior
      float time_sec = now_ms / 1000.0f;

      // Add some randomness for realistic variation
      static uint32_t random_seed = now_ms;
      random_seed = random_seed * 1103515245 + 12345;        // Simple LCG
      float random_factor = (random_seed % 1000) / 10000.0f; // 0.0 to 0.1

      // Create simulation data with thread safety
      {
        std::lock_guard<std::mutex> lock(data_mutex_);

        // Store previous values for change detection
        float prev_battery = ups_data_.battery_level;
        float prev_input = ups_data_.input_voltage;
        float prev_load = ups_data_.load_percent;

        // Simulate realistic UPS data with faster, more visible changes
        float battery_calc = 85.0f + sin(time_sec * 0.05f) * 12.0f + random_factor * 5.0f;
        ups_data_.battery_level = battery_calc < 0.0f ? 0.0f : (battery_calc > 100.0f ? 100.0f : battery_calc);

        float input_calc = 120.0f + sin(time_sec * 0.08f) * 8.0f + random_factor * 3.0f;
        ups_data_.input_voltage = input_calc < 100.0f ? 100.0f : (input_calc > 130.0f ? 130.0f : input_calc);

        float output_calc = 118.0f + sin(time_sec * 0.06f) * 5.0f + random_factor * 2.0f;
        ups_data_.output_voltage = output_calc < 100.0f ? 100.0f : (output_calc > 125.0f ? 125.0f : output_calc);

        float load_calc = 45.0f + sin(time_sec * 0.03f) * 20.0f + random_factor * 8.0f;
        ups_data_.load_percent = load_calc < 0.0f ? 0.0f : (load_calc > 100.0f ? 100.0f : load_calc);

        float runtime_calc = 35.0f + sin(time_sec * 0.02f) * 15.0f + random_factor * 5.0f;
        ups_data_.runtime_minutes = runtime_calc < 0.0f ? 0.0f : (runtime_calc > 120.0f ? 120.0f : runtime_calc);

        float freq_calc = 60.0f + sin(time_sec * 0.4f) * 0.5f + random_factor * 0.3f;
        ups_data_.frequency = freq_calc < 59.5f ? 59.5f : (freq_calc > 60.5f ? 60.5f : freq_calc);

        // Simulate realistic status changes with shorter, more dynamic cycles
        uint32_t cycle_pos = (now_ms / 100) % 200; // 20 second total cycle
        if (cycle_pos < 140)
        {
          ups_data_.status_flags = UPS_STATUS_ONLINE | UPS_STATUS_CHARGING;
        }
        else if (cycle_pos < 170)
        {
          ups_data_.status_flags = UPS_STATUS_ON_BATTERY;
        }
        else if (cycle_pos < 190)
        {
          ups_data_.status_flags = UPS_STATUS_ON_BATTERY | UPS_STATUS_LOW_BATTERY;
        }
        else
        {
          // Brief fault simulation
          ups_data_.status_flags = UPS_STATUS_FAULT;
        }

        // Keep device info consistent
        ups_data_.manufacturer = "Simulated";
        ups_data_.model = "Virtual UPS Pro";
        ups_data_.serial_number = "SIM123456789";
        ups_data_.firmware_version = "1.0.0-SIM";

        // Debug logging every 10 seconds to verify changes
        if (now_ms - last_log_time > 10000)
        {
          ESP_LOGD(TAG, "Simulation values: Battery=%.1f%%, Input=%.1fV, Load=%.1f%%, Status=0x%X",
                   ups_data_.battery_level, ups_data_.input_voltage, ups_data_.load_percent, ups_data_.status_flags);
          ESP_LOGD(TAG, "Value changes: Battery=%+.1f, Input=%+.1f, Load=%+.1f",
                   ups_data_.battery_level - prev_battery,
                   ups_data_.input_voltage - prev_input,
                   ups_data_.load_percent - prev_load);
          last_log_time = now_ms;
        }
      }

      // Simulate connection issues for testing (less frequent)
      uint32_t connection_cycle = (now_ms / 1000) % 300; // 5 minute cycle
      if (connection_cycle == 299)
      {
        ESP_LOGD(TAG, "Simulating temporary connection loss");
        connected_ = false;
      }
      else if (connection_cycle == 5 && !connected_)
      {
        ESP_LOGD(TAG, "Simulating connection restoration");
        connected_ = true;
        last_successful_read_ = millis();
        consecutive_failures_ = 0;
      }
    }

    void UpsHidComponent::register_sensor(sensor::Sensor *sens, const std::string &type)
    {
      sensors_[type] = sens;
    }

    void UpsHidComponent::register_binary_sensor(binary_sensor::BinarySensor *sens, const std::string &type)
    {
      binary_sensors_[type] = sens;
    }

    void UpsHidComponent::register_text_sensor(text_sensor::TextSensor *sens, const std::string &type)
    {
      text_sensors_[type] = sens;
    }

    bool UpsHidComponent::usb_write(const std::vector<uint8_t> &data)
    {
#ifdef USE_ESP32
      if (data.empty())
      {
        return false;
      }

      // Take USB mutex for thread safety
      if (xSemaphoreTake(usb_mutex_, pdMS_TO_TICKS(1000)) != pdTRUE)
      {
        if (should_log_error(usb_error_limiter_))
        {
          ESP_LOGW(TAG, "Failed to acquire USB mutex for write");
        }
        return false;
      }

      // Check connection status inside mutex to prevent race conditions
      if (!device_connected_)
      {
        xSemaphoreGive(usb_mutex_);
        return false;
      }

      std::vector<uint8_t> dummy_response;
      esp_err_t ret = usb_transfer_sync(data, dummy_response, 1000);

      xSemaphoreGive(usb_mutex_);

      if (ret != ESP_OK)
      {
        if (should_log_error(usb_error_limiter_))
        {
          ESP_LOGW(TAG, "USB write failed: %s", esp_err_to_name(ret));
        }
        return false;
      }

      ESP_LOGV(TAG, "USB Write: %zu bytes", data.size());
      return true;
#else
      return false;
#endif
    }

    bool UpsHidComponent::usb_read(std::vector<uint8_t> &data, uint32_t timeout_ms)
    {
#ifdef USE_ESP32
      data.clear();

      // Take USB mutex for thread safety
      if (xSemaphoreTake(usb_mutex_, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
      {
        if (should_log_error(usb_error_limiter_))
        {
          ESP_LOGW(TAG, "Failed to acquire USB mutex for read");
        }
        return false;
      }

      // Check connection status inside mutex to prevent race conditions
      if (!device_connected_)
      {
        xSemaphoreGive(usb_mutex_);
        return false;
      }

      std::vector<uint8_t> dummy_out;
      esp_err_t ret = usb_transfer_sync(dummy_out, data, timeout_ms);

      xSemaphoreGive(usb_mutex_);

      if (ret != ESP_OK)
      {
        if (should_log_error(usb_error_limiter_))
        {
          ESP_LOGW(TAG, "USB read failed: %s", esp_err_to_name(ret));
        }
        data.clear();
        return false;
      }

      ESP_LOGV(TAG, "USB Read: %zu bytes", data.size());
      return true;
#else
      data.clear();
      return false;
#endif
    }

#ifdef USE_ESP32
    esp_err_t UpsHidComponent::usb_init()
    {
      ESP_LOGD(TAG, "Initializing USB Host...");

      if (usb_host_initialized_)
      {
        ESP_LOGW(TAG, "USB Host already initialized");
        return ESP_OK;
      }

      // Create mutex for USB operations
      usb_mutex_ = xSemaphoreCreateMutex();
      if (!usb_mutex_)
      {
        ESP_LOGE(TAG, "Failed to create USB mutex");
        return ESP_ERR_NO_MEM;
      }

      // Initialize USB Host Library
      esp_err_t ret = usb_host_lib_init();
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "USB Host lib init failed: %s", esp_err_to_name(ret));
        return ret;
      }

      // Register USB client
      ret = usb_client_register();
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "USB client register failed: %s", esp_err_to_name(ret));
        return ret;
      }

      usb_host_initialized_ = true;
      ESP_LOGI(TAG, "USB Host initialized successfully");
      return ESP_OK;
    }

    void UpsHidComponent::usb_deinit()
    {
      ESP_LOGD(TAG, "Deinitializing USB Host...");

      if (!usb_host_initialized_)
      {
        return;
      }

      // Stop USB Host task
      if (usb_task_handle_)
      {
        vTaskDelete(usb_task_handle_);
        usb_task_handle_ = nullptr;
      }

      // Close USB device if connected
      if (usb_device_.dev_hdl)
      {
        usb_host_device_close(usb_device_.client_hdl, usb_device_.dev_hdl);
        usb_device_.dev_hdl = nullptr;
      }

      // Deregister USB client
      if (usb_device_.client_hdl)
      {
        usb_host_client_deregister(usb_device_.client_hdl);
        usb_device_.client_hdl = nullptr;
      }

      // Uninstall USB Host Library
      usb_host_uninstall();

      // Delete mutex
      if (usb_mutex_)
      {
        vSemaphoreDelete(usb_mutex_);
        usb_mutex_ = nullptr;
      }

      usb_host_initialized_ = false;
      device_connected_ = false;

      ESP_LOGI(TAG, "USB Host deinitialized");
    }

    esp_err_t UpsHidComponent::usb_host_lib_init()
    {
      // Enhanced USB host configuration with debugging
      usb_host_config_t host_config = {
          .skip_phy_setup = false,
          .intr_flags = ESP_INTR_FLAG_LEVEL1,
      };

      ESP_LOGI(TAG, "Installing USB Host library...");
      ESP_LOGD(TAG, "USB Host config: skip_phy_setup=%s, intr_flags=0x%08X",
               host_config.skip_phy_setup ? "true" : "false", host_config.intr_flags);

      esp_err_t ret = usb_host_install(&host_config);
      ESP_LOGD(TAG, "usb_host_install returned: %s", esp_err_to_name(ret));

      if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
      { // ESP_ERR_INVALID_STATE means already initialized
        ESP_LOGE(TAG, "USB Host install failed: %s", esp_err_to_name(ret));
        return ret;
      }
      ESP_LOGI(TAG, "USB Host library installed successfully");

      // Create USB Host Library task with larger stack size for safety
      if (xTaskCreate(usb_host_lib_task, "usb_host", 8192, this, 5, &usb_task_handle_) != pdTRUE)
      {
        ESP_LOGE(TAG, "Failed to create USB Host task");
        return ESP_ERR_NO_MEM;
      }

      return ESP_OK;
    }

    esp_err_t UpsHidComponent::usb_client_register()
    {
      usb_host_client_config_t client_config = {
          .is_synchronous = false,
          .max_num_event_msg = 5,
          .async = {
              .client_event_callback = usb_client_event_callback,
              .callback_arg = this,
          }};

      esp_err_t ret = usb_host_client_register(&client_config, &usb_device_.client_hdl);
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "USB client register failed: %s", esp_err_to_name(ret));
        return ret;
      }

      return ESP_OK;
    }

    esp_err_t UpsHidComponent::usb_device_enumerate()
    {
      ESP_LOGD(TAG, "Enumerating USB devices...");

      // Check USB host status first
      ESP_LOGD(TAG, "Checking USB host library status...");

      uint8_t dev_addr_list[10];
      int num_dev = 10;

      ESP_LOGD(TAG, "Calling usb_host_device_addr_list_fill with max_dev=%d", num_dev);
      esp_err_t ret = usb_host_device_addr_list_fill(num_dev, dev_addr_list, &num_dev);
      ESP_LOGD(TAG, "usb_host_device_addr_list_fill returned: %s, num_dev=%d", esp_err_to_name(ret), num_dev);

      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to get device address list: %s", esp_err_to_name(ret));
        return ret;
      }

      ESP_LOGI(TAG, "USB enumeration found %d devices", num_dev);

      // Add detailed logging for each device address
      for (int i = 0; i < num_dev; i++)
      {
        ESP_LOGI(TAG, "Device %d: Address %d", i, dev_addr_list[i]);
      }

      for (int i = 0; i < num_dev; i++)
      {
        usb_device_handle_t dev_hdl;
        ret = usb_host_device_open(usb_device_.client_hdl, dev_addr_list[i], &dev_hdl);
        if (ret != ESP_OK)
        {
          continue;
        }

        const usb_device_desc_t *dev_desc;
        ret = usb_host_get_device_descriptor(dev_hdl, &dev_desc);
        if (ret == ESP_OK && usb_is_ups_device(dev_desc))
        {
          // Found UPS device
          usb_device_.dev_hdl = dev_hdl;
          usb_device_.dev_addr = dev_addr_list[i];
          usb_device_.vid = dev_desc->idVendor;
          usb_device_.pid = dev_desc->idProduct;

          ESP_LOGI(TAG, "Found UPS device: VID=0x%04X, PID=0x%04X", usb_device_.vid, usb_device_.pid);

          // Claim interface and get endpoints
          esp_err_t claim_ret = usb_claim_interface();
          if (claim_ret == ESP_OK)
          {
            esp_err_t endpoint_ret = usb_get_endpoints();
            if (endpoint_ret == ESP_OK)
            {
              device_connected_ = true;
              return ESP_OK;
            }
            if (should_log_error(usb_error_limiter_))
            {
              ESP_LOGW(TAG, "Failed to get USB endpoints: %s", esp_err_to_name(endpoint_ret));
            }
          }
          else
          {
            if (should_log_error(usb_error_limiter_))
            {
              ESP_LOGW(TAG, "Failed to claim USB interface: %s", esp_err_to_name(claim_ret));
            }
          }

          // Clean up on failure
          usb_device_.dev_hdl = nullptr;
          usb_device_.dev_addr = 0;
        }

        // Close device handle if we opened it but didn't use it
        usb_host_device_close(usb_device_.client_hdl, dev_hdl);
      }

      return ESP_ERR_NOT_FOUND;
    }

    bool UpsHidComponent::usb_is_ups_device(const usb_device_desc_t *desc)
    {
      uint16_t vid = desc->idVendor;
      uint16_t pid = desc->idProduct;

      ESP_LOGV(TAG, "Checking device: VID=0x%04X, PID=0x%04X", vid, pid);

      // Check configured VID/PID first (highest priority)
      if (vid == usb_vendor_id_ && pid == usb_product_id_)
      {
        ESP_LOGD(TAG, "Device matches configured VID/PID");
        return true;
      }

      // Check against known UPS vendor list
      if (is_known_ups_vendor(vid))
      {
        const char *vendor_name = get_ups_vendor_name(vid);
        ESP_LOGD(TAG, "Recognized UPS vendor: %s (0x%04X)", vendor_name, vid);
        return true;
      }

      // Check device class (some UPS devices use HID class)
      if (desc->bDeviceClass == USB_CLASS_HID ||
          (desc->bDeviceClass == 0 && desc->bDeviceSubClass == 0))
      {
        ESP_LOGV(TAG, "Device might be HID-compatible UPS (unknown vendor)");
        return true; // Could be a HID UPS from unknown vendor
      }

      return false;
    }

    esp_err_t UpsHidComponent::usb_claim_interface()
    {
      // Get active configuration
      const usb_config_desc_t *config_desc;
      esp_err_t ret = usb_host_get_active_config_descriptor(usb_device_.dev_hdl, &config_desc);
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to get config descriptor: %s", esp_err_to_name(ret));
        return ret;
      }

      // Find HID interface
      const usb_intf_desc_t *intf_desc = nullptr;
      int offset = 0;

      for (int i = 0; i < config_desc->bNumInterfaces; i++)
      {
        intf_desc = usb_parse_interface_descriptor(config_desc, i, 0, &offset);
        if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_HID)
        {
          usb_device_.interface_num = intf_desc->bInterfaceNumber;
          usb_device_.is_hid_device = true;
          break;
        }
      }

      if (!intf_desc)
      {
        ESP_LOGE(TAG, "No HID interface found");
        return ESP_ERR_NOT_FOUND;
      }

      // Claim the interface
      ret = usb_host_interface_claim(usb_device_.client_hdl, usb_device_.dev_hdl,
                                     usb_device_.interface_num, 0);
      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to claim interface: %s", esp_err_to_name(ret));
        return ret;
      }

      ESP_LOGD(TAG, "Claimed interface %d", usb_device_.interface_num);
      return ESP_OK;
    }

    esp_err_t UpsHidComponent::usb_get_endpoints()
    {
      // Get active configuration
      const usb_config_desc_t *config_desc;
      esp_err_t ret = usb_host_get_active_config_descriptor(usb_device_.dev_hdl, &config_desc);
      if (ret != ESP_OK)
      {
        return ret;
      }

      // Find HID interface first
      const usb_intf_desc_t *intf_desc = nullptr;
      int intf_offset = 0;

      for (int i = 0; i < config_desc->bNumInterfaces; i++)
      {
        intf_desc = usb_parse_interface_descriptor(config_desc, i, 0, &intf_offset);
        if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_HID)
        {
          break;
        }
      }

      if (!intf_desc)
      {
        ESP_LOGE(TAG, "No HID interface found for endpoints");
        return ESP_ERR_NOT_FOUND;
      }

      // Find endpoints in the HID interface
      int ep_index = 0;
      const usb_ep_desc_t *ep_desc;

      while ((ep_desc = usb_parse_endpoint_descriptor_by_index(intf_desc, ep_index, 0, nullptr)) != nullptr)
      {
        if (USB_EP_DESC_GET_EP_DIR(ep_desc))
        { // IN endpoint
          usb_device_.ep_in = ep_desc->bEndpointAddress;
          usb_device_.max_packet_size = ep_desc->wMaxPacketSize;
          ESP_LOGD(TAG, "Found IN endpoint: 0x%02X, max packet: %d",
                   usb_device_.ep_in, usb_device_.max_packet_size);
        }
        else
        { // OUT endpoint
          usb_device_.ep_out = ep_desc->bEndpointAddress;
          ESP_LOGD(TAG, "Found OUT endpoint: 0x%02X", usb_device_.ep_out);
        }
        ep_index++;
      }

      if (usb_device_.ep_in == 0)
      {
        ESP_LOGE(TAG, "No IN endpoint found");
        return ESP_ERR_NOT_FOUND;
      }

      return ESP_OK;
    }

    esp_err_t UpsHidComponent::usb_transfer_sync(const std::vector<uint8_t> &data_out,
                                                 std::vector<uint8_t> &data_in,
                                                 uint32_t timeout_ms)
    {
      if (!device_connected_ || !usb_device_.dev_hdl)
      {
        ESP_LOGW(TAG, "USB device not connected for transfer");
        return ESP_ERR_INVALID_STATE;
      }

      // Validate timeout
      timeout_ms = std::clamp(timeout_ms, static_cast<uint32_t>(100), static_cast<uint32_t>(30000)); // 100ms to 30s

      esp_err_t ret = ESP_OK;

      // Send data if provided
      if (!data_out.empty() && usb_device_.ep_out != 0)
      {
        if (data_out.size() > usb_device_.max_packet_size * 4)
        { // Reasonable size check
          ESP_LOGW(TAG, "Output data too large: %zu bytes", data_out.size());
          return ESP_ERR_INVALID_SIZE;
        }

        usb_transfer_t *transfer_out = nullptr;
        ret = usb_host_transfer_alloc(data_out.size(), 0, &transfer_out);
        if (ret != ESP_OK || !transfer_out)
        {
          ESP_LOGE(TAG, "Failed to allocate OUT transfer: %s", esp_err_to_name(ret));
          return ret;
        }

        transfer_out->device_handle = usb_device_.dev_hdl;
        transfer_out->bEndpointAddress = usb_device_.ep_out;
        transfer_out->callback = nullptr;
        transfer_out->context = this;
        transfer_out->num_bytes = data_out.size();
        std::memcpy(transfer_out->data_buffer, data_out.data(), data_out.size());

        ret = usb_host_transfer_submit(transfer_out);
        if (ret == ESP_OK)
        {
          vTaskDelay(pdMS_TO_TICKS(std::min(timeout_ms, static_cast<uint32_t>(1000)))); // Max 1s for OUT
        }
        else
        {
          ESP_LOGW(TAG, "Failed to submit OUT transfer: %s", esp_err_to_name(ret));
        }

        usb_host_transfer_free(transfer_out);
        if (ret != ESP_OK)
        {
          return ret;
        }
      }

      // Read data if IN endpoint exists
      if (usb_device_.ep_in != 0)
      {
        size_t buffer_size = std::max(static_cast<size_t>(64), static_cast<size_t>(usb_device_.max_packet_size));
        buffer_size = std::min(buffer_size, static_cast<size_t>(1024)); // Cap at 1KB

        usb_transfer_t *transfer_in = nullptr;
        ret = usb_host_transfer_alloc(buffer_size, 0, &transfer_in);
        if (ret != ESP_OK || !transfer_in)
        {
          ESP_LOGE(TAG, "Failed to allocate IN transfer: %s", esp_err_to_name(ret));
          return ret;
        }

        transfer_in->device_handle = usb_device_.dev_hdl;
        transfer_in->bEndpointAddress = usb_device_.ep_in;
        transfer_in->callback = nullptr;
        transfer_in->context = this;
        transfer_in->num_bytes = buffer_size;

        ret = usb_host_transfer_submit(transfer_in);
        if (ret == ESP_OK)
        {
          vTaskDelay(pdMS_TO_TICKS(timeout_ms));

          // Copy received data with bounds checking
          if (transfer_in->actual_num_bytes > 0 &&
              transfer_in->actual_num_bytes <= buffer_size)
          {
            data_in.resize(transfer_in->actual_num_bytes);
            std::memcpy(data_in.data(), transfer_in->data_buffer, transfer_in->actual_num_bytes);
            ESP_LOGV(TAG, "USB IN transfer received %d bytes", transfer_in->actual_num_bytes);
          }
          else if (transfer_in->actual_num_bytes > buffer_size)
          {
            ESP_LOGW(TAG, "USB IN transfer size mismatch: %d > %zu",
                     transfer_in->actual_num_bytes, buffer_size);
            ret = ESP_ERR_INVALID_SIZE;
          }
        }
        else
        {
          ESP_LOGW(TAG, "Failed to submit IN transfer: %s", esp_err_to_name(ret));
        }

        usb_host_transfer_free(transfer_in);
      }

      return ret;
    }

    void UpsHidComponent::usb_host_lib_task(void *arg)
    {
      auto *component = static_cast<UpsHidComponent *>(arg);

      ESP_LOGD(TAG, "USB Host task started");

      while (component->usb_host_initialized_)
      {
        uint32_t event_flags;
        esp_err_t ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (ret != ESP_OK)
        {
          ESP_LOGW(TAG, "USB Host lib handle events error: %s", esp_err_to_name(ret));
          continue;
        }

        // Log all event flags for debugging
        if (event_flags != 0)
        {
          ESP_LOGD(TAG, "USB Host event flags: 0x%08X", event_flags);
        }

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
          ESP_LOGD(TAG, "USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS");
          usb_host_device_free_all();
        }

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
          ESP_LOGD(TAG, "USB_HOST_LIB_EVENT_FLAGS_ALL_FREE");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
      }

      ESP_LOGD(TAG, "USB Host task ended");
      vTaskDelete(nullptr);
    }

    void UpsHidComponent::usb_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
    {
      auto *component = static_cast<UpsHidComponent *>(arg);

      ESP_LOGD(TAG, "USB client event received: type=%d", event_msg->event);

      switch (event_msg->event)
      {
      case USB_HOST_CLIENT_EVENT_NEW_DEV:
        ESP_LOGI(TAG, "USB_HOST_CLIENT_EVENT_NEW_DEV: device address %d", event_msg->new_dev.address);
        // Trigger device enumeration
        component->usb_device_enumerate();
        break;

      case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGI(TAG, "USB_HOST_CLIENT_EVENT_DEV_GONE: device handle %p", event_msg->dev_gone.dev_hdl);
        if (component->usb_device_.dev_hdl == event_msg->dev_gone.dev_hdl)
        {
          component->device_connected_ = false;
          component->usb_device_.dev_hdl = nullptr;
        }
        break;

      default:
        ESP_LOGD(TAG, "Unknown USB client event: %d", event_msg->event);
        break;
      }
    }
#endif

    // Base Protocol Implementation
    bool UpsProtocolBase::send_command(const std::vector<uint8_t> &cmd, std::vector<uint8_t> &response, uint32_t timeout_ms)
    {
      if (!parent_->usb_write(cmd))
      {
        return false;
      }

      return parent_->usb_read(response, timeout_ms);
    }

    std::string UpsProtocolBase::bytes_to_string(const std::vector<uint8_t> &data)
    {
      std::string result;
      result.reserve(data.size());
      for (uint8_t byte : data)
      {
        if (byte >= 32 && byte <= 126)
        { // Printable ASCII
          result += static_cast<char>(byte);
        }
      }
      return result;
    }

  } // namespace ups_hid
} // namespace esphome