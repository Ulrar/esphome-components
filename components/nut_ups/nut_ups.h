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
#include <atomic>

#ifdef USE_ESP32
#include "esp_err.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// USB HID Class defines
#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03
#endif

#endif

namespace esphome
{
  namespace nut_ups
  {

    static const char *const TAG = "nut_ups";

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
      PROTOCOL_CYBERPOWER_HID,
      PROTOCOL_GENERIC_HID
    };

    // UPS data structure
    struct UpsData
    {
      float battery_level = NAN;
      float input_voltage = NAN;
      float output_voltage = NAN;
      float load_percent = NAN;
      float runtime_minutes = NAN;
      float frequency = NAN;
      uint32_t status_flags = UPS_STATUS_UNKNOWN;
      std::string model = "";
      std::string manufacturer = "";
      std::string serial_number = "";
      std::string firmware_version = "";
      UpsProtocol detected_protocol = PROTOCOL_UNKNOWN;
    };

    // Forward declarations
    class UpsProtocolBase;
    class ApcSmartProtocol;
    class CyberPowerProtocol;
    class GenericHidProtocol;

    class NutUpsComponent : public PollingComponent
    {
    public:
      NutUpsComponent() : usb_device_{}, usb_mutex_(nullptr), usb_task_handle_(nullptr), 
                          usb_host_initialized_(false), device_connected_(false) {
#ifdef USE_ESP32
        memset(&usb_device_, 0, sizeof(usb_device_));
#endif
      }
      
      ~NutUpsComponent() {
#ifdef USE_ESP32
        usb_deinit();
#endif
      }

      void setup() override;
      void update() override;
      void dump_config() override;
      float get_setup_priority() const override { return setup_priority::DATA; }

      // Configuration setters
      void set_simulation_mode(bool simulation_mode) { simulation_mode_ = simulation_mode; }
      void set_usb_vendor_id(uint16_t vendor_id) { usb_vendor_id_ = vendor_id; }
      void set_usb_product_id(uint16_t product_id) { usb_product_id_ = product_id; }
      void set_protocol_timeout(uint32_t timeout_ms) { protocol_timeout_ms_ = timeout_ms; }
      void set_auto_detect_protocol(bool auto_detect) { auto_detect_protocol_ = auto_detect; }

      // Data getters for sensors
      const UpsData &get_ups_data() const { return ups_data_; }
      bool is_connected() const { return connected_; }
      std::string get_protocol_name() const;

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
      std::atomic<uint32_t> last_successful_read_{0};
      UpsData ups_data_;

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

    public:
      // USB communication methods (accessible by protocol classes)
      bool usb_write(const std::vector<uint8_t> &data);
      bool usb_read(std::vector<uint8_t> &data, uint32_t timeout_ms = 1000);

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
        uint8_t interface_num;
        uint8_t ep_in;
        uint8_t ep_out;
        uint16_t max_packet_size;
      };

      // USB Host member variables
      UsbDevice usb_device_;
      SemaphoreHandle_t usb_mutex_;
      TaskHandle_t usb_task_handle_;
      bool usb_host_initialized_;
      bool device_connected_;

      // ESP32-specific USB handling
      esp_err_t usb_init();
      void usb_deinit();
      esp_err_t usb_host_lib_init();
      esp_err_t usb_client_register();
      esp_err_t usb_device_open();
      esp_err_t usb_device_enumerate();
      bool usb_is_ups_device(const usb_device_desc_t *desc);
      esp_err_t usb_claim_interface();
      esp_err_t usb_get_endpoints();
      
      // USB communication
      esp_err_t usb_transfer_sync(const std::vector<uint8_t> &data_out, std::vector<uint8_t> &data_in, uint32_t timeout_ms);
      
      // USB Host event handling
      static void usb_host_lib_task(void *arg);
      static void usb_client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg);
#endif
    };

    // Base class for UPS protocols
    class UpsProtocolBase
    {
    public:
      explicit UpsProtocolBase(NutUpsComponent *parent) : parent_(parent) {}
      virtual ~UpsProtocolBase() = default;

      virtual bool detect() = 0;
      virtual bool initialize() = 0;
      virtual bool read_data(UpsData &data) = 0;
      virtual UpsProtocol get_protocol_type() const = 0;
      virtual std::string get_protocol_name() const = 0;

    protected:
      NutUpsComponent *parent_;

      bool send_command(const std::vector<uint8_t> &cmd, std::vector<uint8_t> &response, uint32_t timeout_ms = 1000);
      std::string bytes_to_string(const std::vector<uint8_t> &data);
    };

    // APC Smart Protocol implementation
    class ApcSmartProtocol : public UpsProtocolBase
    {
    public:
      explicit ApcSmartProtocol(NutUpsComponent *parent) : UpsProtocolBase(parent) {}

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

    // CyberPower HID Protocol implementation
    class CyberPowerProtocol : public UpsProtocolBase
    {
    public:
      explicit CyberPowerProtocol(NutUpsComponent *parent) : UpsProtocolBase(parent) {}

      bool detect() override;
      bool initialize() override;
      bool read_data(UpsData &data) override;
      UpsProtocol get_protocol_type() const override { return PROTOCOL_CYBERPOWER_HID; }
      std::string get_protocol_name() const override { return "CyberPower HID"; }

    private:
      struct HidReport
      {
        uint8_t report_id;
        std::vector<uint8_t> data;
      };

      bool send_hid_report(const HidReport &report, HidReport &response);
      bool parse_hid_data(const HidReport &report, UpsData &data);
      bool parse_status_report(const HidReport &report, UpsData &data);
      bool parse_battery_report(const HidReport &report, UpsData &data);
      bool parse_voltage_report(const HidReport &report, UpsData &data);
      bool parse_device_info_report(const HidReport &report, UpsData &data);
    };

    // Generic HID Protocol implementation
    class GenericHidProtocol : public UpsProtocolBase
    {
    public:
      explicit GenericHidProtocol(NutUpsComponent *parent) : UpsProtocolBase(parent) {}

      bool detect() override;
      bool initialize() override;
      bool read_data(UpsData &data) override;
      UpsProtocol get_protocol_type() const override { return PROTOCOL_GENERIC_HID; }
      std::string get_protocol_name() const override { return "Generic HID"; }
    };

  } // namespace nut_ups
} // namespace esphome