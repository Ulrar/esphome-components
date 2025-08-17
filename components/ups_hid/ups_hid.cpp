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
#include "driver/gpio.h"

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

      // Use cached data from asynchronous USB operations instead of blocking reads
      // This prevents task watchdog timeouts by avoiding long USB control transfers
      bool data_updated = false;
      
      {
        std::lock_guard<std::mutex> cache_lock(ups_data_cache_.mutex);
        
        if (ups_data_cache_.data_valid) {
          // Update main data with fresh cached data
          {
            std::lock_guard<std::mutex> data_lock(data_mutex_);
            ups_data_ = ups_data_cache_.data;
          }
          
          last_successful_read_ = millis();
          consecutive_failures_ = 0;
          data_updated = true;
          
          // Mark cache as processed
          ups_data_cache_.data_valid = false;
          
          ESP_LOGV(TAG, "Updated from cached UPS data");
        }
      }
      
      if (!data_updated) {
        // No cached data available - try a quick non-blocking read
        // But with reduced timeout to prevent watchdog issues
        if (read_ups_data()) {
          last_successful_read_ = millis();
          consecutive_failures_ = 0;
          data_updated = true;
        } else {
          consecutive_failures_++;
          if (should_log_error(protocol_error_limiter_)) {
            ESP_LOGW(TAG, "Failed to read UPS data (failure #%u)", consecutive_failures_);
          }

          // Implement limited retry logic with exponential backoff
          if (consecutive_failures_ >= 3 && consecutive_failures_ <= max_consecutive_failures_) {
            ESP_LOGE(TAG, "Multiple consecutive failures, attempting protocol re-detection");
            if (!detect_ups_protocol()) {
              ESP_LOGE(TAG, "Protocol re-detection failed");
            }
          }
          else if (consecutive_failures_ > max_consecutive_failures_) {
            ESP_LOGE(TAG, "Maximum re-detection attempts reached, giving up");
          }

          if (millis() - last_successful_read_ > protocol_timeout_ms_) {
            ESP_LOGE(TAG, "UPS communication timeout, marking as disconnected");
            connected_ = false;
            consecutive_failures_ = 0;
          }
        }
      }
      
      // Always update sensors with current data (cached or fresh)
      update_sensors();
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
      ESP_LOGI(TAG, "Initializing USB HID Host communication...");
      ESP_LOGI(TAG, "ESP32-S3 USB OTG Host Mode Configuration:");
      ESP_LOGI(TAG, "  Board: ESP32-S3-DevKitC-1");
      ESP_LOGI(TAG, "  USB OTG Pins: D+ (GPIO20), D- (GPIO19)");
      ESP_LOGI(TAG, "  USB Host Mode: Enabled with HID Host driver");

      // Create mutex for USB synchronization
      usb_mutex_ = xSemaphoreCreateMutex();
      if (usb_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create USB mutex");
        return false;
      }

      // Initialize USB Host Library first
      esp_err_t ret = usb_host_lib_init();
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Host library initialization failed: %s", esp_err_to_name(ret));
        return false;
      }
      usb_host_initialized_ = true;

      // Register USB client
      ret = usb_client_register();
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB client registration failed: %s", esp_err_to_name(ret));
        usb_deinit();
        return false;
      }

      // Wait a moment for USB devices to be detected
      vTaskDelay(pdMS_TO_TICKS(1000));
      
      // Try initial device enumeration
      ret = usb_device_enumerate();
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Initial USB device enumeration completed successfully");
      } else {
        ESP_LOGW(TAG, "Initial USB device enumeration failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "USB Troubleshooting if no devices found:");
        ESP_LOGW(TAG, "  1. Verify USB OTG pads are soldered correctly");
        ESP_LOGW(TAG, "  2. Check UPS is powered ON and connected");
        ESP_LOGW(TAG, "  3. Try different USB cable (must support data)");
        ESP_LOGW(TAG, "  4. Some UPS devices need to be 'awake' (press a button)");
      }
      
      ESP_LOGI(TAG, "USB Host initialized successfully");
      return true;
#else
      ESP_LOGE(TAG, "USB HID Host only supported on ESP32 platform");
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
        // Only try Smart Protocol if device has OUT endpoint (bidirectional communication)
        if (!usb_device_.is_input_only) {
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
        } else {
          ESP_LOGD(TAG, "Skipping APC Smart Protocol - input-only device (no OUT endpoint)");
        }
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
        // Yield to prevent task watchdog timeout during long protocol detection
        vTaskDelay(pdMS_TO_TICKS(1));
        
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
    esp_err_t UpsHidComponent::usb_host_lib_init()
    {
      ESP_LOGD(TAG, "Creating USB Host Library task...");

      // Create USB Host Library task
      BaseType_t task_created = xTaskCreatePinnedToCore(
          usb_host_lib_task,
          "usb_events",
          4096,
          xTaskGetCurrentTaskHandle(), // Pass current task handle for notification
          2,
          &usb_lib_task_handle_,
          0);

      if (task_created != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create USB Host Library task");
        return ESP_FAIL;
      }

      // Wait for notification from USB Host Library task that it's ready
      uint32_t notification_value = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(5000));
      if (notification_value == 0) {
        ESP_LOGE(TAG, "USB Host Library task startup timeout");
        return ESP_ERR_TIMEOUT;
      }

      ESP_LOGI(TAG, "USB Host Library task started successfully");
      
      // Create USB client task for asynchronous operations
      usb_tasks_running_ = true;
      task_created = xTaskCreatePinnedToCore(
          usb_client_task,
          "usb_client",
          4096,
          this,
          3, // Higher priority than USB Host Library task
          &usb_client_task_handle_,
          1);
      
      if (task_created != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create USB client task");
        usb_tasks_running_ = false;
        return ESP_FAIL;
      }
      
      ESP_LOGI(TAG, "USB client task started successfully");
      return ESP_OK;
    }

    esp_err_t UpsHidComponent::usb_client_register()
    {
      ESP_LOGD(TAG, "Registering USB client...");

      usb_host_client_config_t client_config = {
          .is_synchronous = false,
          .max_num_event_msg = 5,
          .async = {
              .client_event_callback = usb_client_event_callback,
              .callback_arg = this,
          }};

      esp_err_t ret = usb_host_client_register(&client_config, &usb_device_.client_hdl);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB client register failed: %s", esp_err_to_name(ret));
        return ret;
      }

      ESP_LOGI(TAG, "USB client registered successfully");
      return ESP_OK;
    }


    void UpsHidComponent::usb_deinit()
    {
      ESP_LOGD(TAG, "Deinitializing USB Host...");

      // Stop USB tasks
      usb_tasks_running_ = false;
      
      if (usb_client_task_handle_) {
        vTaskDelete(usb_client_task_handle_);
        usb_client_task_handle_ = nullptr;
      }

      if (usb_host_initialized_) {
        // Stop USB Host Library task
        if (usb_lib_task_handle_) {
          vTaskDelete(usb_lib_task_handle_);
          usb_lib_task_handle_ = nullptr;
        }

        // Uninstall USB Host Library
        usb_host_uninstall();
        usb_host_initialized_ = false;
      }

      // Delete mutex
      if (usb_mutex_) {
        vSemaphoreDelete(usb_mutex_);
        usb_mutex_ = nullptr;
      }

      device_connected_ = false;
      ESP_LOGI(TAG, "USB Host deinitialized");
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

      // Parse endpoints correctly using ESP-IDF approach
      const usb_ep_desc_t *ep_desc = nullptr;
      int ep_offset = intf_offset;
      
      ESP_LOGD(TAG, "Interface has %d endpoints", intf_desc->bNumEndpoints);
      
      for (int i = 0; i < intf_desc->bNumEndpoints; i++) {
        ep_desc = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, &ep_offset);
        if (ep_desc) {
          if (USB_EP_DESC_GET_EP_DIR(ep_desc)) {
            usb_device_.ep_in = ep_desc->bEndpointAddress;
            usb_device_.max_packet_size = ep_desc->wMaxPacketSize;
            ESP_LOGD(TAG, "Found IN endpoint: 0x%02X (max packet size: %d)",
                     usb_device_.ep_in, usb_device_.max_packet_size);
          } else {
            usb_device_.ep_out = ep_desc->bEndpointAddress;
            ESP_LOGD(TAG, "Found OUT endpoint: 0x%02X", usb_device_.ep_out);
          }
        } else {
          ESP_LOGW(TAG, "Failed to parse endpoint %d", i);
        }
      }

      if (usb_device_.ep_in == 0)
      {
        ESP_LOGE(TAG, "No IN endpoint found");
        return ESP_ERR_NOT_FOUND;
      }

      // Detect input-only devices (no OUT endpoint)
      if (usb_device_.ep_out == 0) {
        usb_device_.is_input_only = true;
        ESP_LOGW(TAG, "INPUT-ONLY HID device detected - no OUT endpoint available");
        ESP_LOGD(TAG, "Device can only send data to host, cannot receive commands");
      } else {
        usb_device_.is_input_only = false;
        ESP_LOGD(TAG, "Bidirectional device detected - has both IN and OUT endpoints");
      }

      return ESP_OK;
    }

    void UpsHidComponent::usb_transfer_callback(usb_transfer_t *transfer)
    {
      TransferContext *ctx = static_cast<TransferContext*>(transfer->context);
      if (!ctx) {
        ESP_LOGE(TAG, "Transfer callback: NULL context");
        return;
      }

      // Store transfer results
      ctx->result = (transfer->status == USB_TRANSFER_STATUS_COMPLETED) ? ESP_OK : ESP_FAIL;
      ctx->actual_bytes = transfer->actual_num_bytes;
      
      // Copy data for IN transfers if successful
      if (ctx->result == ESP_OK && ctx->buffer && transfer->actual_num_bytes > 0) {
        // For control transfers, skip the setup packet (8 bytes)
        size_t data_offset = 0;
        size_t available_data = transfer->actual_num_bytes;
        
        // Check if this is a control transfer (endpoint 0)
        if (transfer->bEndpointAddress == 0 && available_data >= sizeof(usb_setup_packet_t)) {
          data_offset = sizeof(usb_setup_packet_t);
          available_data -= sizeof(usb_setup_packet_t);
        }
        
        if (available_data > 0 && ctx->buffer_size > 0) {
          size_t copy_size = std::min(available_data, ctx->buffer_size);
          memcpy(ctx->buffer, transfer->data_buffer + data_offset, copy_size);
          ESP_LOGV(TAG, "Transfer callback: copied %zu bytes (offset %zu)", copy_size, data_offset);
        }
      }

      // Signal completion
      if (ctx->done_sem) {
        xSemaphoreGive(ctx->done_sem);
      }
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

        // Ensure transfer size is multiple of MPS
        size_t transfer_size = data_out.size();
        if (usb_device_.max_packet_size > 0) {
          size_t remainder = transfer_size % usb_device_.max_packet_size;
          if (remainder != 0) {
            transfer_size += (usb_device_.max_packet_size - remainder);
          }
        }
        ESP_LOGV(TAG, "OUT transfer: data_size=%zu, mps=%u, transfer_size=%zu", 
                 data_out.size(), usb_device_.max_packet_size, transfer_size);

        // Create transfer context
        TransferContext out_ctx = {};
        out_ctx.done_sem = xSemaphoreCreateBinary();
        if (!out_ctx.done_sem) {
          ESP_LOGE(TAG, "Failed to create OUT transfer semaphore");
          return ESP_ERR_NO_MEM;
        }

        usb_transfer_t *transfer_out = nullptr;
        ret = usb_host_transfer_alloc(transfer_size, 0, &transfer_out);
        if (ret != ESP_OK || !transfer_out)
        {
          ESP_LOGE(TAG, "Failed to allocate OUT transfer: %s", esp_err_to_name(ret));
          vSemaphoreDelete(out_ctx.done_sem);
          return ret;
        }

        transfer_out->device_handle = usb_device_.dev_hdl;
        transfer_out->bEndpointAddress = usb_device_.ep_out;
        transfer_out->callback = usb_transfer_callback;
        transfer_out->context = &out_ctx;
        transfer_out->num_bytes = transfer_size;
        // Copy data and zero-pad the rest
        std::memcpy(transfer_out->data_buffer, data_out.data(), data_out.size());
        if (transfer_size > data_out.size()) {
          std::memset(transfer_out->data_buffer + data_out.size(), 0, transfer_size - data_out.size());
        }

        ret = usb_host_transfer_submit(transfer_out);
        if (ret == ESP_OK)
        {
          // Wait for transfer completion
          if (xSemaphoreTake(out_ctx.done_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
            ret = out_ctx.result;
            ESP_LOGV(TAG, "OUT transfer completed with result: %s", esp_err_to_name(ret));
          } else {
            ESP_LOGW(TAG, "OUT transfer timeout after %ums", timeout_ms);
            ret = ESP_ERR_TIMEOUT;
          }
        }
        else
        {
          ESP_LOGW(TAG, "Failed to submit OUT transfer: %s", esp_err_to_name(ret));
        }

        usb_host_transfer_free(transfer_out);
        vSemaphoreDelete(out_ctx.done_sem);
        
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
        
        // Ensure buffer size is multiple of MPS
        if (usb_device_.max_packet_size > 0) {
          size_t remainder = buffer_size % usb_device_.max_packet_size;
          if (remainder != 0) {
            buffer_size += (usb_device_.max_packet_size - remainder);
          }
        }
        ESP_LOGV(TAG, "IN transfer: mps=%u, buffer_size=%zu", usb_device_.max_packet_size, buffer_size);

        // Prepare buffer for received data
        std::vector<uint8_t> temp_buffer(buffer_size);
        
        // Create transfer context
        TransferContext in_ctx = {};
        in_ctx.done_sem = xSemaphoreCreateBinary();
        in_ctx.buffer = temp_buffer.data();
        in_ctx.buffer_size = buffer_size;
        
        if (!in_ctx.done_sem) {
          ESP_LOGE(TAG, "Failed to create IN transfer semaphore");
          return ESP_ERR_NO_MEM;
        }

        usb_transfer_t *transfer_in = nullptr;
        ret = usb_host_transfer_alloc(buffer_size, 0, &transfer_in);
        if (ret != ESP_OK || !transfer_in)
        {
          ESP_LOGE(TAG, "Failed to allocate IN transfer: %s", esp_err_to_name(ret));
          vSemaphoreDelete(in_ctx.done_sem);
          return ret;
        }

        transfer_in->device_handle = usb_device_.dev_hdl;
        transfer_in->bEndpointAddress = usb_device_.ep_in;
        transfer_in->callback = usb_transfer_callback;
        transfer_in->context = &in_ctx;
        transfer_in->num_bytes = buffer_size;

        ret = usb_host_transfer_submit(transfer_in);
        if (ret == ESP_OK)
        {
          // Wait for transfer completion
          if (xSemaphoreTake(in_ctx.done_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
            ret = in_ctx.result;
            
            if (ret == ESP_OK && in_ctx.actual_bytes > 0) {
              // Copy received data to output vector
              data_in.resize(in_ctx.actual_bytes);
              std::memcpy(data_in.data(), temp_buffer.data(), in_ctx.actual_bytes);
              ESP_LOGV(TAG, "USB IN transfer received %zu bytes", in_ctx.actual_bytes);
            }
          } else {
            ESP_LOGW(TAG, "IN transfer timeout after %ums", timeout_ms);
            ret = ESP_ERR_TIMEOUT;
          }
        }
        else
        {
          ESP_LOGW(TAG, "Failed to submit IN transfer: %s", esp_err_to_name(ret));
        }

        usb_host_transfer_free(transfer_in);
        vSemaphoreDelete(in_ctx.done_sem);
      }

      return ret;
    }

    esp_err_t UpsHidComponent::hid_get_report(uint8_t report_type, uint8_t report_id, uint8_t* data, size_t* data_len)
    {
      if (!device_connected_ || !usb_device_.dev_hdl || !data || !data_len || *data_len == 0) {
        ESP_LOGE(TAG, "HID GET_REPORT: Invalid parameters");
        return ESP_ERR_INVALID_ARG;
      }
      
      const uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
      const uint8_t bRequest = 0x01; // HID GET_REPORT
      const uint16_t wValue = (report_type << 8) | report_id;
      const uint16_t wIndex = usb_device_.interface_num;
      
      // Ensure reasonable buffer size limits
      size_t max_data_len = std::min(*data_len, static_cast<size_t>(64)); // HID reports are typically <= 64 bytes
      
      usb_transfer_t *transfer = nullptr;
      // Control transfers need setup packet + data phase allocation
      size_t transfer_size = sizeof(usb_setup_packet_t) + max_data_len;
      esp_err_t ret = usb_host_transfer_alloc(transfer_size, 0, &transfer);
      if (ret != ESP_OK || !transfer) {
        ESP_LOGE(TAG, "Failed to allocate control transfer (size=%zu): %s", transfer_size, esp_err_to_name(ret));
        return ret;
      }
      
      // Create transfer context for synchronization
      TransferContext ctx = {};
      ctx.done_sem = xSemaphoreCreateBinary();
      ctx.buffer = data;
      ctx.buffer_size = max_data_len;
      
      if (!ctx.done_sem) {
        usb_host_transfer_free(transfer);
        return ESP_ERR_NO_MEM;
      }
      
      // Set up USB control transfer
      transfer->device_handle = usb_device_.dev_hdl;
      transfer->bEndpointAddress = 0; // Control endpoint (required for control transfers)
      transfer->callback = usb_transfer_callback;
      transfer->context = &ctx;
      transfer->num_bytes = transfer_size; // Total transfer size including setup packet
      transfer->timeout_ms = 200; // Very short timeout to prevent task watchdog issues
      
      ESP_LOGV(TAG, "Control transfer setup: dev_hdl=%p, ep=0x%02X, size=%zu, timeout=%u", 
               transfer->device_handle, transfer->bEndpointAddress, transfer->num_bytes, transfer->timeout_ms);
      
      // Create setup packet with proper byte ordering for USB protocol
      usb_setup_packet_t *setup = (usb_setup_packet_t*)transfer->data_buffer;
      setup->bmRequestType = bmRequestType;
      setup->bRequest = bRequest;
      setup->wValue = wValue;     // (report_type << 8) | report_id - already in correct format
      setup->wIndex = wIndex;     // Interface number
      setup->wLength = (uint16_t)max_data_len; // Expected response length - explicit cast
      
      ESP_LOGD(TAG, "HID GET_REPORT: bmReqType=0x%02X, bReq=0x%02X, wValue=0x%04X, wIndex=0x%04X, wLen=%zu, dev_hdl=%p, intf_num=%d", 
               setup->bmRequestType, setup->bRequest, setup->wValue, setup->wIndex, max_data_len, usb_device_.dev_hdl, usb_device_.interface_num);
      
      // Ensure client handle is valid before submission
      if (!usb_device_.client_hdl) {
        ESP_LOGE(TAG, "HID GET_REPORT: Invalid client handle");
        ret = ESP_ERR_INVALID_STATE;
      } else {
        ret = usb_host_transfer_submit_control(usb_device_.client_hdl, transfer);
      }
      if (ret == ESP_OK) {
        // Wait for completion with very short timeout to prevent watchdog issues
        if (xSemaphoreTake(ctx.done_sem, pdMS_TO_TICKS(250)) == pdTRUE) {
          ret = ctx.result;
          if (ret == ESP_OK && ctx.actual_bytes >= sizeof(usb_setup_packet_t)) {
            size_t actual_data_len = ctx.actual_bytes - sizeof(usb_setup_packet_t);
            *data_len = std::min(max_data_len, actual_data_len);
            ESP_LOGD(TAG, "HID GET_REPORT success: received %zu bytes", *data_len);
          } else {
            ESP_LOGW(TAG, "HID GET_REPORT completed but no data: result=%s, actual_bytes=%zu", 
                     esp_err_to_name(ret), ctx.actual_bytes);
          }
        } else {
          ESP_LOGW(TAG, "HID GET_REPORT timeout after 250ms");
          ret = ESP_ERR_TIMEOUT;
        }
      } else {
        ESP_LOGE(TAG, "Failed to submit HID GET_REPORT: %s", esp_err_to_name(ret));
      }
      
      usb_host_transfer_free(transfer);
      vSemaphoreDelete(ctx.done_sem);
      return ret;
    }

    esp_err_t UpsHidComponent::hid_set_report(uint8_t report_type, uint8_t report_id, const uint8_t* data, size_t data_len)
    {
      if (!device_connected_ || !usb_device_.dev_hdl || !data) {
        return ESP_ERR_INVALID_ARG;
      }
      
      const uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
      const uint8_t bRequest = 0x09; // HID SET_REPORT
      const uint16_t wValue = (report_type << 8) | report_id;
      const uint16_t wIndex = usb_device_.interface_num;
      
      usb_transfer_t *transfer = nullptr;
      esp_err_t ret = usb_host_transfer_alloc(sizeof(usb_setup_packet_t) + data_len, 0, &transfer);
      if (ret != ESP_OK || !transfer) {
        ESP_LOGE(TAG, "Failed to allocate control transfer: %s", esp_err_to_name(ret));
        return ret;
      }
      
      // Create transfer context for synchronization
      TransferContext ctx = {};
      ctx.done_sem = xSemaphoreCreateBinary();
      
      if (!ctx.done_sem) {
        usb_host_transfer_free(transfer);
        return ESP_ERR_NO_MEM;
      }
      
      // Set up USB control transfer
      transfer->device_handle = usb_device_.dev_hdl;
      transfer->bEndpointAddress = 0; // Control endpoint
      transfer->callback = usb_transfer_callback;
      transfer->context = &ctx;
      transfer->num_bytes = sizeof(usb_setup_packet_t) + data_len;
      
      // Create setup packet
      usb_setup_packet_t *setup = (usb_setup_packet_t*)transfer->data_buffer;
      setup->bmRequestType = bmRequestType;
      setup->bRequest = bRequest;
      setup->wValue = wValue;
      setup->wIndex = wIndex;
      setup->wLength = data_len;
      
      // Copy data after setup packet
      if (data_len > 0) {
        memcpy(transfer->data_buffer + sizeof(usb_setup_packet_t), data, data_len);
      }
      
      ESP_LOGV(TAG, "HID SET_REPORT: type=0x%02X, id=0x%02X, len=%zu", report_type, report_id, data_len);
      
      ret = usb_host_transfer_submit_control(usb_device_.client_hdl, transfer);
      if (ret == ESP_OK) {
        // Wait for completion
        if (xSemaphoreTake(ctx.done_sem, pdMS_TO_TICKS(5000)) == pdTRUE) {
          ret = ctx.result;
          ESP_LOGV(TAG, "HID SET_REPORT success");
        } else {
          ESP_LOGW(TAG, "HID SET_REPORT timeout");
          ret = ESP_ERR_TIMEOUT;
        }
      } else {
        ESP_LOGW(TAG, "Failed to submit HID SET_REPORT: %s", esp_err_to_name(ret));
      }
      
      usb_host_transfer_free(transfer);
      vSemaphoreDelete(ctx.done_sem);
      return ret;
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

#ifdef USE_ESP32
    // USB Host Library task - based on working ESP32 NUT server implementation
    void UpsHidComponent::usb_host_lib_task(void *arg)
    {
      TaskHandle_t parent_task = static_cast<TaskHandle_t>(arg);
      ESP_LOGI(TAG, "USB Host Library task starting...");

      // Initialize USB Host library
      const usb_host_config_t host_config = {
          .skip_phy_setup = false,
          .intr_flags = ESP_INTR_FLAG_LEVEL1,
      };

      esp_err_t ret = usb_host_install(&host_config);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Host install failed: %s", esp_err_to_name(ret));
        vTaskDelete(nullptr);
        return;
      }

      ESP_LOGI(TAG, "USB Host library installed successfully");
      
      // Notify parent task that USB Host is ready
      xTaskNotifyGive(parent_task);

      // Main USB Host event loop
      while (true) {
        uint32_t event_flags;
        ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "USB Host event handling failed: %s", esp_err_to_name(ret));
          continue;
        }

        // Handle USB Host events
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
          usb_host_device_free_all();
          ESP_LOGI(TAG, "USB Event: NO_CLIENTS - freed all devices");
        }
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
          ESP_LOGI(TAG, "USB Event: ALL_FREE - all devices removed");
        }
      }
    }

    // USB client task for asynchronous operations
    void UpsHidComponent::usb_client_task(void *arg)
    {
      UpsHidComponent *component = static_cast<UpsHidComponent *>(arg);
      ESP_LOGI(TAG, "USB client task started");

      // Process USB client events in a loop
      while (component->usb_tasks_running_) {
        esp_err_t ret = usb_host_client_handle_events(component->usb_device_.client_hdl, 100);
        
        if (ret == ESP_ERR_TIMEOUT) {
          // Timeout is normal - just continue processing
          continue;
        } else if (ret != ESP_OK) {
          ESP_LOGW(TAG, "USB client event handling failed: %s", esp_err_to_name(ret));
          vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay on error
          continue;
        }
        
        // Process any cached data updates
        component->process_cached_data();
        
        // Small delay to prevent busy-waiting
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      
      ESP_LOGI(TAG, "USB client task stopping");
      vTaskDelete(NULL);
    }

    // Process cached UPS data from asynchronous operations
    void UpsHidComponent::process_cached_data()
    {
      std::lock_guard<std::mutex> cache_lock(ups_data_cache_.mutex);
      
      if (ups_data_cache_.data_valid) {
        // Update main data with cached data
        {
          std::lock_guard<std::mutex> data_lock(data_mutex_);
          ups_data_ = ups_data_cache_.data;
        }
        
        // Mark cache as processed
        ups_data_cache_.data_valid = false;
        
        ESP_LOGV(TAG, "Processed cached UPS data");
      }
    }

    // Asynchronous HID GET_REPORT operation
    esp_err_t UpsHidComponent::hid_get_report_async(uint8_t report_type, uint8_t report_id)
    {
      if (!device_connected_ || !usb_device_.dev_hdl) {
        return ESP_ERR_INVALID_STATE;
      }
      
      // For now, this is a placeholder for future async implementation
      // The current synchronous hid_get_report will be replaced with proper
      // async operations using queued transfers processed by usb_client_task
      
      ESP_LOGV(TAG, "Async HID GET_REPORT request: type=0x%02X, id=0x%02X", report_type, report_id);
      
      return ESP_OK;
    }

    // USB client event callback
    void UpsHidComponent::usb_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
    {
      UpsHidComponent *component = static_cast<UpsHidComponent *>(arg);

      switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
          ESP_LOGI(TAG, "USB_HOST_CLIENT_EVENT_NEW_DEV: device address %d", event_msg->new_dev.address);
          // Trigger device enumeration
          component->usb_device_enumerate();
          break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
          ESP_LOGI(TAG, "USB_HOST_CLIENT_EVENT_DEV_GONE: device handle %p", event_msg->dev_gone.dev_hdl);
          if (component->usb_device_.dev_hdl == event_msg->dev_gone.dev_hdl) {
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

  } // namespace ups_hid
} // namespace esphome