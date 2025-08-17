#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sensor.h"
#include "binary_sensor.h"
#include "text_sensor.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

#ifdef USE_ESP32
#include "esp_err.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
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

    static const char *const TAG = "ups_hid";

    // UPS status flags
    enum UpsStatus
    {
      UPS_STATUS_UNKNOWN = 0,
      UPS_STATUS_ONLINE = 1 << 0,
      UPS_STATUS_ON_BATTERY = 1 << 1,
      UPS_STATUS_LOW_BATTERY = 1 << 2,
      UPS_STATUS_REPLACE_BATTERY = 1 << 3,
      UPS_STATUS_CHARGING = 1 << 4,
      UPS_STATUS_FAULT = 1 << 5,
      UPS_STATUS_OVERLOAD = 1 << 6,
      UPS_STATUS_CALIBRATING = 1 << 7,
      UPS_STATUS_OFF = 1 << 8
    };

    // UPS protocols
    enum UpsProtocol
    {
      PROTOCOL_UNKNOWN = 0,
      PROTOCOL_APC_SMART,
      PROTOCOL_APC_HID,
      PROTOCOL_CYBERPOWER_HID,
      PROTOCOL_GENERIC_HID
    };

    // UPS data structure with proper initialization
    struct UpsData
    {
      float battery_level{NAN};
      float input_voltage{NAN};
      float output_voltage{NAN};
      float load_percent{NAN};
      float runtime_minutes{NAN};
      float frequency{NAN};
      uint32_t status_flags{UPS_STATUS_UNKNOWN};
      std::string model{};
      std::string manufacturer{};
      std::string serial_number{};
      std::string firmware_version{};
      UpsProtocol detected_protocol{PROTOCOL_UNKNOWN};
      
      // Additional sensor fields
      float battery_voltage{NAN};
      float battery_voltage_nominal{NAN};
      float input_voltage_nominal{NAN};
      float input_transfer_low{NAN};
      float input_transfer_high{NAN};
      float ups_realpower_nominal{NAN};
      int16_t ups_delay_shutdown{-1};
      int16_t ups_delay_start{-1};
      std::string ups_beeper_status{};
      std::string input_sensitivity{};
      
      // Reset all data to default values
      void reset() {
        battery_level = NAN;
        input_voltage = NAN;
        output_voltage = NAN;
        load_percent = NAN;
        runtime_minutes = NAN;
        frequency = NAN;
        status_flags = UPS_STATUS_UNKNOWN;
        model.clear();
        manufacturer.clear();
        serial_number.clear();
        firmware_version.clear();
        detected_protocol = PROTOCOL_UNKNOWN;
        
        // Reset additional sensor fields
        battery_voltage = NAN;
        battery_voltage_nominal = NAN;
        input_voltage_nominal = NAN;
        input_transfer_low = NAN;
        input_transfer_high = NAN;
        ups_realpower_nominal = NAN;
        ups_delay_shutdown = -1;
        ups_delay_start = -1;
        ups_beeper_status.clear();
        input_sensitivity.clear();
      }
      
      // Check if core data is valid
      bool is_valid() const {
        return !std::isnan(battery_level) || !std::isnan(input_voltage) || 
               !std::isnan(output_voltage) || status_flags != UPS_STATUS_UNKNOWN;
      }
    };

    // Forward declarations
    class UpsProtocolBase;
    class ApcSmartProtocol;
    class ApcHidProtocol;
    class CyberPowerProtocol;
    class GenericHidProtocol;

    class UpsHidComponent : public PollingComponent
    {
    public:
      UpsHidComponent() : usb_device_{}, usb_mutex_(nullptr), usb_lib_task_handle_(nullptr),
                          usb_client_task_handle_(nullptr), usb_host_initialized_(false), 
                          device_connected_(false), usb_tasks_running_(false) {
#ifdef USE_ESP32
        memset(&usb_device_, 0, sizeof(usb_device_));
        ups_data_cache_.last_update_time = 0;
        ups_data_cache_.data_valid = false;
#endif
      }
      
      ~UpsHidComponent() {
#ifdef USE_ESP32
        // Log any remaining suppressed errors before cleanup
        log_suppressed_errors(usb_error_limiter_);
        log_suppressed_errors(protocol_error_limiter_);
        usb_deinit();
#endif
      }

      void setup() override;
      void update() override;
      void dump_config() override;
      float get_setup_priority() const override { return setup_priority::DATA; }

      // Configuration setters with validation
      void set_simulation_mode(bool simulation_mode) { simulation_mode_ = simulation_mode; }
      void set_usb_vendor_id(uint16_t vendor_id) { 
        if (vendor_id != 0) usb_vendor_id_ = vendor_id; 
      }
      void set_usb_product_id(uint16_t product_id) { 
        if (product_id != 0) usb_product_id_ = product_id; 
      }
      void set_protocol_timeout(uint32_t timeout_ms) { 
        // Bound timeout between 5 seconds and 5 minutes for safety
        protocol_timeout_ms_ = std::max(static_cast<uint32_t>(5000), 
                                       std::min(timeout_ms, static_cast<uint32_t>(300000)));
      }
      void set_auto_detect_protocol(bool auto_detect) { auto_detect_protocol_ = auto_detect; }

      // Data getters for sensors (thread-safe)
      UpsData get_ups_data() const { 
        std::lock_guard<std::mutex> lock(data_mutex_);
        return ups_data_; 
      }
      bool is_connected() const { return connected_; }
      std::string get_protocol_name() const;
      
      // USB device info getters
      uint16_t get_usb_vendor_id() const { return usb_vendor_id_; }
      uint16_t get_usb_product_id() const { return usb_product_id_; }
      bool is_input_only_device() const { return usb_device_.is_input_only; }

      // Sensor registration methods
      void register_sensor(sensor::Sensor *sens, const std::string &type);
      void register_binary_sensor(binary_sensor::BinarySensor *sens, const std::string &type);
      void register_text_sensor(text_sensor::TextSensor *sens, const std::string &type);

    protected:
      bool simulation_mode_{false};
      uint16_t usb_vendor_id_{0x051D};  // APC default
      uint16_t usb_product_id_{0x0002}; // Back-UPS ES series default
      uint32_t protocol_timeout_ms_{10000};
      bool auto_detect_protocol_{true};

      bool connected_{false};
      uint32_t last_successful_read_{0};
      uint32_t consecutive_failures_{0};
      uint32_t max_consecutive_failures_{5};  // Limit re-detection attempts
      UpsData ups_data_;
      mutable std::mutex data_mutex_;  // Protect ups_data_ access
      
      // Error rate limiting to prevent log spam
      struct ErrorRateLimit {
        uint32_t last_error_time{0};
        uint32_t error_count{0};
        uint32_t suppressed_count{0};
        static constexpr uint32_t RATE_LIMIT_MS = 5000;  // 5 seconds between repeated errors
        static constexpr uint32_t MAX_BURST = 3;         // Allow 3 errors before rate limiting
      };
      ErrorRateLimit usb_error_limiter_;
      ErrorRateLimit protocol_error_limiter_;

      std::unique_ptr<UpsProtocolBase> active_protocol_;
      std::unordered_map<std::string, sensor::Sensor *> sensors_;
      std::unordered_map<std::string, binary_sensor::BinarySensor *> binary_sensors_;
      std::unordered_map<std::string, text_sensor::TextSensor *> text_sensors_;

      // Core methods
      bool initialize_usb();
      bool detect_ups_protocol();
      bool read_ups_data();
      void update_sensors();
      void simulate_ups_data();
      
      // Error rate limiting helpers
      bool should_log_error(ErrorRateLimit& limiter);
      void log_suppressed_errors(ErrorRateLimit& limiter);

    public:
      // USB communication methods (accessible by protocol classes)
      bool usb_write(const std::vector<uint8_t> &data);
      bool usb_read(std::vector<uint8_t> &data, uint32_t timeout_ms = 1000);
      
      // HID class requests (UPS-specific communication)
      esp_err_t hid_get_report(uint8_t report_type, uint8_t report_id, uint8_t* data, size_t* data_len);
      esp_err_t hid_set_report(uint8_t report_type, uint8_t report_id, const uint8_t* data, size_t data_len);
      

    protected:
#ifdef USE_ESP32
      // USB Host structures
      struct UsbDevice {
        usb_host_client_handle_t client_hdl;
        usb_device_handle_t dev_hdl;
        uint8_t dev_addr;
        uint16_t vid;
        uint16_t pid;
        bool is_hid_device;
        bool is_input_only;        // True if device has no OUT endpoint (input-only HID device)
        uint8_t interface_num;
        uint8_t ep_in;
        uint8_t ep_out;
        uint16_t max_packet_size;
      };

      // USB Host member variables
      UsbDevice usb_device_;
      SemaphoreHandle_t usb_mutex_;
      TaskHandle_t usb_lib_task_handle_;
      TaskHandle_t usb_client_task_handle_;
      bool usb_host_initialized_;
      bool device_connected_;
      volatile bool usb_tasks_running_;
      
      // Asynchronous data cache for non-blocking access
      struct UpsDataCache {
        UpsData data;
        std::mutex mutex;
        uint32_t last_update_time;
        bool data_valid;
      } ups_data_cache_;

      // ESP32-specific USB handling
      esp_err_t usb_init();
      void usb_deinit();
      esp_err_t usb_host_lib_init();
      esp_err_t usb_client_register();
      esp_err_t usb_device_enumerate();
      bool usb_is_ups_device(const usb_device_desc_t *desc);
      esp_err_t usb_claim_interface();
      esp_err_t usb_get_endpoints();
      
      // USB communication
      esp_err_t usb_transfer_sync(const std::vector<uint8_t> &data_out, std::vector<uint8_t> &data_in, uint32_t timeout_ms);
      
      // Transfer completion tracking
      struct TransferContext {
        SemaphoreHandle_t done_sem;
        esp_err_t result;
        size_t actual_bytes;
        uint8_t* buffer;
        size_t buffer_size;
      };
      
      // USB Host event handling
      static void usb_host_lib_task(void *arg);
      static void usb_client_task(void *arg);
      static void usb_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg);
      static void usb_transfer_callback(usb_transfer_t *transfer);
      
      // Asynchronous USB operations
      esp_err_t hid_get_report_async(uint8_t report_type, uint8_t report_id);
      void process_cached_data();
#endif
    };

    // Base class for UPS protocols
    class UpsProtocolBase
    {
    public:
      explicit UpsProtocolBase(UpsHidComponent *parent) : parent_(parent) {}
      virtual ~UpsProtocolBase() = default;

      virtual bool detect() = 0;
      virtual bool initialize() = 0;
      virtual bool read_data(UpsData &data) = 0;
      virtual UpsProtocol get_protocol_type() const = 0;
      virtual std::string get_protocol_name() const = 0;

    protected:
      UpsHidComponent *parent_;

      bool send_command(const std::vector<uint8_t> &cmd, std::vector<uint8_t> &response, uint32_t timeout_ms = 1000);
      std::string bytes_to_string(const std::vector<uint8_t> &data);
    };

    // APC Smart Protocol implementation
    class ApcSmartProtocol : public UpsProtocolBase
    {
    public:
      explicit ApcSmartProtocol(UpsHidComponent *parent) : UpsProtocolBase(parent) {}

      bool detect() override;
      bool initialize() override;
      bool read_data(UpsData &data) override;
      UpsProtocol get_protocol_type() const override { return PROTOCOL_APC_SMART; }
      std::string get_protocol_name() const override { return "APC Smart Protocol"; }

    private:
      bool send_smart_command(char cmd, std::string &response);
      float parse_voltage(const std::string &response);
      float parse_percentage(const std::string &response);
      uint32_t parse_status_flags(const std::string &response);
    };


    // Generic HID Protocol implementation
    class GenericHidProtocol : public UpsProtocolBase
    {
    public:
      explicit GenericHidProtocol(UpsHidComponent *parent) : UpsProtocolBase(parent) {}

      bool detect() override;
      bool initialize() override;
      bool read_data(UpsData &data) override;
      UpsProtocol get_protocol_type() const override { return PROTOCOL_GENERIC_HID; }
      std::string get_protocol_name() const override { return "Generic HID"; }
      
    private:
      bool parse_generic_report(const std::vector<uint8_t>& response, UpsData& data);
    };

  } // namespace ups_hid
} // namespace esphome