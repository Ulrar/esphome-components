# UPS HID Component for ESPHome

A ESPHome component for monitoring UPS devices via USB connection on ESP32-S3. Direct USB HID communication with support for APC, CyberPower, and generic HID UPS devices.

## Features

- 🔋 **Real-time UPS monitoring**: Battery level, voltages, load, runtime, status
- 🧪 **UPS self-test control**: Battery tests (quick/deep), panel tests, with real-time result monitoring
- 🔊 **Beeper control**: Enable/disable/mute/test UPS audible alarms via HID write operations
- 🌈 **Visual status indicator**: RGB LED with customizable status colors
- 🏠 **Home Assistant integration**: Automatic entity discovery via ESPHome API
- 🔌 **Multi-protocol support**: APC HID, CyberPower HID, Generic HID
- 🎯 **Auto-detection**: Intelligent protocol detection based on USB vendor IDs
- 🔧 **Robust USB handling**: ESP-IDF v5.4 compatible with 3-tier reconnection recovery
- 🧪 **Simulation mode**: Test integration without physical UPS device

## Quick Start

### Hardware Requirements

- **ESP32-S3-DevKitC-1 v1.1** with USB OTG support
- **UPS device** with USB monitoring port
- **RGB LED** (optional, connected to GPIO48)
- **USB cable** (UPS to ESP32-S3)

### Basic Configuration

```yaml
# example.yaml
esphome:
  name: ups-monitor

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

# Enable logging
logger:
  level: DEBUG
  baud_rate: 115200
  hardware_uart: UART0

# Enable Home Assistant API
api:
  encryption:
    key: !secret api_encryption_key

# Enable OTA updates
ota:
  - platform: esphome
    password: !secret ota_password

# WiFi configuration
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "UPS-Monitor Fallback"
    password: !secret ap_password

# Captive portal for WiFi setup
captive_portal:

# Optional web server
web_server:

# Optional: RGB status LED with effects (configured separately)
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO48
    num_leds: 1
    chipset: ws2812
    name: "UPS Status LED"
    id: ups_status_led
    restore_mode: ALWAYS_OFF
    effects:
      # Critical conditions - Fast red strobe (unmistakable emergency pattern)
      - strobe:
          name: "Critical Alert"
          colors:
            - state: true
              brightness: 100%
              red: 100%
              green: 0%
              blue: 0%
              duration: 150ms
            - state: false
              duration: 150ms
      
      # Battery operation - Slow orange fade (amber warning pattern)
      - pulse:
          name: "Battery Warning"
          transition_length: 1.5s
          update_interval: 1.5s
          min_brightness: 10%
          max_brightness: 95%
      
      # Charging status - Quick yellow double-blink (charging pattern)
      - strobe:
          name: "Charging Pattern"
          colors:
            - state: true
              brightness: 80%
              red: 100%
              green: 100%
              blue: 0%
              duration: 200ms
            - state: false
              duration: 200ms
            - state: true
              brightness: 80%
              red: 100%
              green: 100%
              blue: 0%
              duration: 200ms
            - state: false
              duration: 1400ms
      
      # Normal operation - Slow green breathing (peaceful pattern)
      - pulse:
          name: "Normal Status"
          transition_length: 3s
          update_interval: 3s
          min_brightness: 20%
          max_brightness: 60%
      
      # Fault/offline - Blue fade (distinct from other states)
      - pulse:
          name: "System Offline"
          transition_length: 2s
          update_interval: 2s
          min_brightness: 5%
          max_brightness: 40%

# Enable external components
external_components:
  - source:
      type: local
      path: components

# Configure the UPS HID component
ups_hid:
  id: ups_monitor           # Required ID for referencing
  update_interval: 30s      # How often to poll UPS data
  usb_vendor_id: 0x051D     # APC vendor ID (auto-detected if omitted)  
  usb_product_id: 0x0002    # Optional: specific product ID
  protocol_timeout: 15s     # USB communication timeout
  protocol: auto             # Protocol selection: auto, apc, cyberpower, generic
  simulation_mode: false    # Set to true for testing without UPS
    
# Define sensors (each sensor requires separate platform entry)
sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: battery_level
    name: "UPS Battery Level"
    unit_of_measurement: "%"
    device_class: battery
    state_class: measurement
    accuracy_decimals: 0
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: input_voltage
    name: "UPS Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: output_voltage
    name: "UPS Output Voltage" 
    unit_of_measurement: "V"
    device_class: voltage
    state_class: measurement
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: load_percent
    name: "UPS Load"
    unit_of_measurement: "%"
    state_class: measurement
    accuracy_decimals: 0
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: runtime
    name: "UPS Runtime"
    unit_of_measurement: "min"
    device_class: duration
    state_class: measurement
    accuracy_decimals: 0
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: frequency
    name: "UPS Frequency"
    unit_of_measurement: "Hz"
    state_class: measurement
    accuracy_decimals: 1

binary_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: online
    name: "UPS Online"
    id: ups_online
    device_class: connectivity
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: on_battery
    name: "UPS On Battery"
    id: on_battery
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: low_battery
    name: "UPS Low Battery"
    id: low_battery
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: charging
    name: "UPS Charging"
    id: battery_charging
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: fault
    name: "UPS Fault"
    id: ups_fault
    device_class: problem
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: overload
    name: "UPS Overload"
    id: overload_warning
    device_class: problem

text_sensor:
  # Device identification (available on all protocols)
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: manufacturer
    name: "UPS Manufacturer"
    icon: "mdi:factory"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: model
    name: "UPS Model"
    icon: "mdi:information-outline"

  # Device information (protocol-specific)
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: serial_number
    name: "UPS Serial Number"
    icon: "mdi:identifier"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: firmware_version
    name: "UPS Firmware Version"
    icon: "mdi:chip"

  # Communication status
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: protocol
    name: "Detected Protocol"
    icon: "mdi:connection"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: status
    name: "UPS Status"
    
  # Beeper status monitoring
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: ups_beeper_status
    name: "UPS Beeper Status"
    icon: "mdi:volume-high"

# UPS Beeper Control
button:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: enable
    name: "UPS Beeper Enable"
    icon: "mdi:volume-high"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: disable
    name: "UPS Beeper Disable"
    icon: "mdi:volume-off"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: mute
    name: "UPS Beeper Mute"
    icon: "mdi:volume-mute"
    # Note: Mute only works during active alarm conditions
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: test
    name: "UPS Beeper Test"
    icon: "mdi:bell-ring"

# Automated LED status control based on UPS state
interval:
  - interval: 2s
    then:
      - lambda: |-
          // Enhanced UPS status LED control with distinct visual patterns
          // Each state now has a unique color and pattern combination

          // CRITICAL CONDITIONS (Priority 1) - Fast red strobe
          if (id(low_battery).state || id(ups_fault).state || id(overload_warning).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 0.0, 0.0); // Bright red
            call.set_effect("Critical Alert");
            call.perform();
          }
          // BATTERY OPERATION (Priority 2) - Orange slow fade  
          else if (id(on_battery).state && !id(low_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 0.5, 0.0); // Orange amber
            call.set_effect("Battery Warning");
            call.perform();
          }
          // CHARGING STATUS (Priority 3) - Yellow double-blink pattern
          else if (id(battery_charging).state && id(ups_online).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 1.0, 0.0); // Yellow
            call.set_effect("Charging Pattern");
            call.perform();
          }
          // NORMAL OPERATION (Priority 4) - Gentle green breathing
          else if (id(ups_online).state && !id(on_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(0.0, 1.0, 0.0); // Green
            call.set_effect("Normal Status");
            call.perform();
          }
          // OFFLINE/UNKNOWN (Priority 5) - Dim blue fade
          else {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(0.0, 0.3, 1.0); // Blue
            call.set_effect("System Offline");
            call.perform();
          }
```

## Hardware Setup

### ESP32-S3 USB OTG Connection

```
UPS USB Port (Type-B)  ←→  USB Cable  ←→  ESP32-S3 USB OTG Port
```

### RGB LED Wiring (Optional)

```
GPIO48 (ESP32-S3) → RGB LED Data Pin
3.3V → RGB LED VCC
GND → RGB LED GND
```

### LED Status Indicators

- 🟢 **Green Breathing**: UPS online, normal operation (slow, gentle pulse)
- 🟠 **Orange Fade**: UPS running on battery power (moderate fade pattern)  
- 🟡 **Yellow Double-Blink**: Battery charging (distinct double-flash pattern)
- 🔴 **Red Strobe**: Critical conditions - low battery, fault, overload (fast emergency flash)
- 🔵 **Blue Fade**: System offline/unknown (dim, slow fade)

## Supported UPS Devices

### Tested Compatible Models

| Vendor | Models | Protocol | Vendor ID | Beeper Control |
|--------|--------|----------|-----------|----------------|
| **APC** | Back-UPS ES Series, Smart-UPS | APC HID | 0x051D | ✅ Confirmed |
| **CyberPower** | CP1500EPFCLCD, CP1000PFCLCD | CyberPower HID | 0x0764 | ✅ Confirmed |
| **Tripp Lite** | SMART1500LCDT, UPS series | Generic HID | 0x09AE | ⚠️ Limited |
| **Eaton/MGE** | Ellipse, Evolution series | Generic HID | 0x06DA | ⚠️ Limited |
| **Belkin** | Older USB UPS models | Generic HID | 0x050D | ⚠️ Limited |

**Beeper Control Legend:**
- ✅ **Confirmed**: Full beeper control tested and working (enable/disable/mute/test)
- ⚠️ **Limited**: Basic support via generic HID (device-dependent functionality)

### Protocol Compatibility Matrix

| Protocol | Communication | Auto-Detection | Read Features | Write Features |
|----------|---------------|----------------|---------------|----------------|
| **APC HID** | USB HID reports | ✅ | Battery, voltage, status | ✅ Beeper control |
| **CyberPower HID** | Vendor-specific HID | ✅ | Extended sensors, config | ✅ Beeper control |
| **Generic HID** | Standard HID-PDC | ✅ | Basic monitoring | ⚠️ Limited writes |

## Configuration Reference

### Component Options

```yaml
ups_hid:
  id: ups_monitor                # Required component ID for sensor references
  update_interval: 30s           # Polling interval (minimum 5s recommended)
  
  # USB Device Configuration
  usb_vendor_id: 0x051D          # USB Vendor ID (hex), auto-detected if omitted
  usb_product_id: 0x0002         # USB Product ID (hex), optional
  
  # Protocol Settings
  protocol: auto                 # Protocol selection: auto, apc, cyberpower, generic
  protocol_timeout: 15s          # Communication timeout (5s-60s)
  
  # Testing Options
  simulation_mode: false         # Enable simulation for testing (no USB required)

# Status LED configured separately (not part of ups_hid component)
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO48                  # GPIO pin for RGB LED
    num_leds: 1
    chipset: ws2812              # LED type (ws2812, ws2811, etc.)
    name: "UPS Status LED"
    id: ups_status_led
```

### Sensor Platform

Each sensor requires a separate platform entry with `ups_hid_id` and `type`:

```yaml
sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor      # Reference to component ID
    type: battery_level          # Battery charge percentage (0-100%)
    name: "Battery Level"
    unit_of_measurement: "%"
    device_class: battery
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: input_voltage          # AC input voltage (V)
    name: "Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: output_voltage         # AC output voltage (V)
    name: "Output Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: load_percent           # UPS load percentage (0-100%)
    name: "Load Percentage"
    unit_of_measurement: "%"
    accuracy_decimals: 1
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: runtime               # Estimated runtime (minutes)
    name: "Runtime"
    unit_of_measurement: "min"
    device_class: duration
    accuracy_decimals: 0
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: frequency             # AC frequency (Hz)
    name: "Frequency"
    unit_of_measurement: "Hz"
    accuracy_decimals: 1
```

**Available sensor types:**
- `battery_level`: Battery charge percentage (0-100%)
- `input_voltage`: AC input voltage (V)
- `output_voltage`: AC output voltage (V)  
- `load_percent`: UPS load percentage (0-100%)
- `runtime`: Estimated runtime (minutes)
- `frequency`: AC frequency (Hz)

### Binary Sensor Platform

Each binary sensor requires `ups_hid_id` and `type`:

```yaml
binary_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: online                 # UPS is online (AC power available)
    name: "UPS Online"
    device_class: connectivity
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: on_battery             # UPS running on battery
    name: "On Battery"
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: low_battery            # Battery level is low
    name: "Low Battery"
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: charging               # Battery is charging
    name: "Charging"
    device_class: battery
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: fault                  # UPS fault condition
    name: "Fault"
    device_class: problem
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: overload               # UPS is overloaded
    name: "Overload"
    device_class: problem
```

**Available binary sensor types:**
- `online`: UPS is online (AC power available)
- `on_battery`: UPS running on battery power
- `low_battery`: Battery level is critically low
- `charging`: Battery is currently charging
- `fault`: UPS has a fault condition
- `overload`: UPS is overloaded

### Text Sensor Platform

Each text sensor requires `ups_hid_id` and `type`:

```yaml
text_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: manufacturer           # UPS manufacturer name
    name: "Manufacturer"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: model                  # UPS model number
    name: "Model"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: serial_number          # UPS serial number (if available)
    name: "Serial Number"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: firmware_version       # UPS firmware version (if available)
    name: "Firmware"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: protocol               # Detected protocol name
    name: "Protocol"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: status                 # Combined status string
    name: "Status"
```

**Available text sensor types:**

### Device Identification (Available on all protocols):
- `manufacturer`: UPS manufacturer name (e.g., "APC", "CPS", "Generic")
- `model`: UPS model number/name (e.g., "Back-UPS ES", "CP1500EPFCLCD")
- `protocol`: Currently detected protocol name (e.g., "APC HID Protocol")
- `status`: Combined UPS status text (e.g., "Online Charging")

### Device Information (Protocol-specific support):
- `serial_number`: UPS serial number
  - ✅ **CyberPower**: Full HID report parsing (e.g., "CRMLX2000234")
  - ⚠️ **APC**: Basic support (may be limited by device)
  - ⚠️ **Generic**: Device-dependent parsing
- `firmware_version`: UPS firmware version
  - ✅ **CyberPower**: Full HID report parsing (e.g., "CR01505B4")
  - ⚠️ **APC**: Basic support (may be limited by device)
  - ⚠️ **Generic**: Device-dependent parsing

### HID Configuration (Protocol-specific support):
- `ups_beeper_status`: UPS beeper/alarm configuration
  - ✅ **CyberPower**: Full parsing ("enabled", "disabled", "muted")
  - ⚠️ **APC**: Basic support (may be limited by device)
  - ⚠️ **Generic**: Device-dependent parsing
- `input_sensitivity`: UPS input voltage sensitivity setting
  - ✅ **CyberPower**: Full parsing ("high", "normal", "low")
  - ⚠️ **APC**: Basic support (may be limited by device)
  - ⚠️ **Generic**: Device-dependent parsing

**Legend**: ✅ = Full Support, ⚠️ = Basic/Limited Support

> **Note**: Device information availability depends on the UPS model and protocol implementation. CyberPower devices provide the most comprehensive device information through dedicated HID reports.

### Button Platform (Beeper Control)

The component supports UPS beeper control via HID SET_REPORT operations:

```yaml
button:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: enable            # Action: enable, disable, mute, test
    name: "UPS Beeper Enable"
    icon: "mdi:volume-high"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: disable
    name: "UPS Beeper Disable"
    icon: "mdi:volume-off"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: mute
    name: "UPS Beeper Mute"
    icon: "mdi:volume-mute"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: test
    name: "UPS Beeper Test"
    icon: "mdi:bell-ring"
```

**Available beeper actions:**
- `enable`: Enable UPS beeper for all alarm conditions
- `disable`: Disable UPS beeper (silent operation)
- `mute`: Temporarily silence current beeper alarms (only works during active alarms)
- `test`: Test beeper functionality (enables/disables to verify write operations)

**Protocol Support:**
- ✅ **APC Devices**: Uses report IDs 0x18 and 0x78 (multi-report fallback)
- ✅ **CyberPower Devices**: Uses report ID 0x0c (dedicated beeper control)
- ⚠️ **Generic HID**: Limited support (device-dependent)

**Important Notes:**
- **Mute functionality**: Only works when UPS is actively beeping (during power outages or fault conditions)
- **Test behavior**: Verifies write operations but produces no sound unless there's an active alarm
- **State persistence**: Beeper settings are stored in UPS NVRAM and persist across reboots
- **Real-world testing**: To hear actual beeper operation, test during actual power loss events

**Beeper Status Monitoring:**
```yaml
text_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: ups_beeper_status         # Shows: "enabled", "disabled", or "muted"
    name: "UPS Beeper Status"
    icon: "mdi:volume-high"
```

### UPS Test Control (Battery & Panel Tests)

The component supports UPS self-test operations compatible with Network UPS Tools (NUT) standard:

```yaml
button:
  # Battery Tests
  - platform: ups_hid
    ups_hid_id: ups_monitor
    test_action: battery_quick      # Quick battery test (10-15 seconds)
    name: "UPS Quick Battery Test"
    icon: "mdi:battery-plus"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    test_action: battery_deep       # Deep battery test (2-5 minutes)
    name: "UPS Deep Battery Test"
    icon: "mdi:battery-charging"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    test_action: battery_stop       # Stop any running battery test
    name: "UPS Stop Battery Test"
    icon: "mdi:battery-remove"

  # Panel Tests  
  - platform: ups_hid
    ups_hid_id: ups_monitor
    test_action: ups_test          # Panel/system test (LEDs, buzzer)
    name: "UPS Panel Test"
    icon: "mdi:test-tube"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    test_action: ups_stop          # Stop panel test
    name: "UPS Stop Panel Test"
    icon: "mdi:stop"
```

**Available test actions:**
- `battery_quick`: Quick battery capacity test (10-15 seconds)
- `battery_deep`: Comprehensive battery test (2-5 minutes)
- `battery_stop`: Abort any running battery test
- `ups_test`: Test panel indicators and systems
- `ups_stop`: Stop panel/system test

**Test Result Monitoring:**
```yaml
text_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: ups_test_result          # Current test status
    name: "UPS Test Result"
    icon: "mdi:test-tube"
```

**Test Result Values (NUT Compatible):**
- `"No test initiated"` - Default state
- `"In progress"` - Test currently running
- `"Done and passed"` - ✅ Test completed successfully
- `"Done and warning"` - ⚠️ Test completed with warnings
- `"Done and error"` - ❌ Test failed
- `"Aborted"` - 🛑 Test was stopped/aborted
- `"Test scheduled"` - 📅 Test scheduled but not started

**Protocol Support:**
- ✅ **APC Devices**: Battery test (0x52), Panel test (0x79) - Full NUT compatibility
- ✅ **CyberPower Devices**: Battery test (0x14), Generic test support
- ⚠️ **Generic HID**: Multi-report fallback (device-dependent)

**Important Test Behavior:**
- **Battery Tests**: UPS may switch to battery power briefly during testing
- **Deep Tests**: Can take several minutes - don't interrupt
- **Panel Tests**: Test LED indicators and audible alarms
- **Automatic Results**: Test results update automatically in real-time
- **State Persistence**: Test results persist until next test is initiated

**Testing Best Practices:**
```yaml
# Monitor test progress with automation
automation:
  - alias: "UPS Test Completed"
    trigger:
      platform: state
      entity_id: text_sensor.ups_test_result
      from: "In progress"
    condition:
      condition: not
      conditions:
        - condition: state
          entity_id: text_sensor.ups_test_result
          state: "In progress"
    action:
      service: notify.mobile_app
      data:
        message: "UPS test completed: {{ states('text_sensor.ups_test_result') }}"
```


```yaml
# Enable detailed USB debugging if needed
logger:
  level: DEBUG
  logs:
    ups_hid: DEBUG                 # Component debugging
    ups_hid.apc: DEBUG            # APC protocol debugging
    ups_hid.cyberpower: DEBUG     # CyberPower protocol debugging
```

## Advanced Configuration

### Protocol Selection

You can now manually select which UPS protocol to use instead of relying on auto-detection:

```yaml
ups_hid:
  protocol: auto                 # Default: automatic selection based on USB vendor ID
  # protocol: apc                # Force APC HID protocol
  # protocol: cyberpower         # Force CyberPower HID protocol  
  # protocol: generic            # Force Generic HID protocol
```

**Protocol Options:**

- **`auto`** (default): Automatically select protocol based on USB vendor ID
  - APC devices (0x051D): Uses APC HID Protocol
  - CyberPower (0x0764): Uses CyberPower HID Protocol
  - Unknown devices: Falls back to Generic HID Protocol

- **`apc`**: Force APC HID Protocol
  - Use for APC devices: Back-UPS ES, Smart-UPS series
  - Comprehensive sensor support with 20+ HID reports
  - Battery and beeper testing (device-dependent)

- **`cyberpower`**: Force CyberPower HID Protocol
  - Use for CyberPower CP series devices
  - Enhanced sensor support with 12+ additional sensors
  - Runtime scaling and advanced thresholds

- **`generic`**: Force Generic HID Protocol
  - Universal fallback for unknown UPS brands
  - Basic 5-sensor support with intelligent detection
  - Limited beeper/testing functionality

**When to use manual selection:**
- Testing different protocols on the same device
- Troubleshooting protocol detection issues
- Using non-standard USB vendor/product IDs
- Forcing generic protocol for maximum compatibility

### Performance Tuning

```yaml
ups_hid:
  id: ups_monitor
  update_interval: 10s           # Faster polling (minimum 5s recommended)
  protocol_timeout: 5s           # Faster timeout for responsive networks
  
# Individual sensor update intervals are controlled by the component
```

### Simulation Mode

For testing without physical UPS:

```yaml
ups_hid:
  id: ups_monitor
  simulation_mode: true          # Enables realistic simulated data
  update_interval: 5s            # Faster updates to see simulation changes
  
logger:
  level: DEBUG                   # See simulation data changes
```

## Troubleshooting

### Common Issues

#### 1. "No UPS devices found during initial enumeration"

**Cause**: USB connection or device power issues
**Solutions**:
- Verify USB cable connection (UPS ↔ ESP32-S3)
- Ensure UPS is powered on and USB monitoring is enabled
- Check ESP32-S3 USB OTG is functioning
- Try different USB cable

#### 2. "No compatible UPS protocol detected"

**Cause**: Unsupported UPS model or communication failure
**Solutions**:
- Check if your UPS model is in the supported devices list
- Enable debug logging: `logger: level: DEBUG`
- Try manual protocol selection: `protocol: generic` or `protocol: apc`
- Verify USB vendor/product IDs in logs

#### 3. "Failed to acquire USB mutex" errors

**Cause**: USB communication conflicts or hardware issues
**Solutions**:
- Increase protocol timeout: `protocol_timeout: 15s`
- Check for USB hardware conflicts
- Restart ESP32-S3 device
- Verify stable power supply

#### 4. "Protocol detection rate limited" warnings

**Cause**: Multiple failed detection attempts (normal protection)
**Solutions**:
- Wait 5 seconds for rate limit reset
- This is normal behavior protecting the UPS device
- Check physical connections if persistent

#### 5. "ESP_ERR_INVALID_STATE" during USB operations - known issue that requires esp32-idf v5.5+

**Cause**: Normal behavior when UPS physically disconnected but USB library has stale references
**What you'll see**:
```
E (22037) USB HOST: usbh_devs_open error: ESP_ERR_INVALID_STATE
W: USB device 1 in invalid state - forcing cleanup and retry
I: Attempting complete USB client re-registration
I: USB client registered successfully
```
**Solutions**:
- **This is normal behavior** - no action required
- System automatically recovers using 3-tier strategy
- All sensors will show "unavailable" until UPS physically reconnected

#### 6. "Beeper control not working"

**Cause**: UPS doesn't support write operations or incorrect protocol
**Solutions**:
- Verify your UPS model supports beeper control (check supported devices list)
- Enable debug logging to see HID SET_REPORT attempts: `ups_hid.apc: DEBUG`
- Test during actual power outage to hear mute functionality
- Check beeper status sensor for current state
- Some UPS models have hardware beeper disable switches

### Debug Logging

Enable detailed logging for troubleshooting:

```yaml
logger:
  level: DEBUG
  logs:
    ups_hid: DEBUG
    ups_hid.apc: DEBUG
    ups_hid.cyberpower: DEBUG
    ups_hid.generic: DEBUG
```

### Log Analysis

**Normal operation logs**:
```
[D][ups_hid:240] Detected known UPS vendor: APC (0x051D)
[I][ups_hid:371] Successfully detected APC HID protocol (took 156ms)
[D][ups_hid.apc:89] Battery: 85.0%, Runtime: 45.2 min
```

**USB disconnection sequence** (normal behavior):
```
[I][ups_hid:2166][usb_client] UPS device disconnected
[I][ups_hid:2195][usb_client] Device cleanup complete
[D][ups_hid:650] Force-publishing NAN for cleared sensor 'battery_level'
[D][text_sensor:069] 'UPS Manufacturer': Sending state 'Unknown'
[I][ups_hid:159] Cleared stale UPS data and forced sensor updates
```

**USB state corruption recovery** (normal behavior):
```
[D][esp-idf:000] E: USB HOST: usbh_devs_open error: ESP_ERR_INVALID_STATE
[W][ups_hid:1140] USB device 1 in invalid state - forcing cleanup and retry
[W][ups_hid:1175] Attempting complete USB client re-registration
[I][ups_hid:1189] USB client re-registration successful
```

**Connection issues**:
```
[W][ups_hid:461] Failed to acquire USB mutex for write
[E][ups_hid:264] No compatible UPS protocol detected for vendor 0x051D
```

**Rate limiting (normal)**:
```
[W][ups_hid:185] Suppressed 5 similar error messages in the last 5000 ms
```

**Beeper control success**:
```
[I][ups_hid.apc:123] APC beeper enabled successfully with report ID 0x18
[D][ups_hid.cyberpower:87] CyberPower beeper muted successfully
```

**Beeper control issues**:
```
[W][ups_hid.apc:145] Failed to set beeper with all report IDs
[E][ups_hid:567] HID SET_REPORT failed: ESP_ERR_TIMEOUT
```

### Performance Monitoring

Monitor these metrics in Home Assistant:
- Update intervals (should be consistent)
- Protocol detection time (< 500ms typical)
- Error rates (should be minimal)
- USB communication success rate

## Advanced LED Status Control

The component can control an RGB LED to display UPS status with priority-based visual indicators:

### LED Priority System

1. **🔴 Critical Alert** (Priority 1): Fast red strobe - Low battery, UPS fault, overload (150ms flash)
2. **🟠 Battery Warning** (Priority 2): Orange slow fade - Running on battery power (1.5s cycle)
3. **🟡 Charging Pattern** (Priority 3): Yellow double-blink - Battery charging while online (200ms double-flash)
4. **🟢 Normal Status** (Priority 4): Green gentle breathing - Online, normal operation (3s cycle)
5. **🔵 System Offline** (Priority 5): Blue dim fade - UPS disconnected or unknown state (2s cycle)

### Complete LED Configuration

```yaml
# RGB LED with custom effects
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO48
    num_leds: 1
    chipset: ws2812
    name: "UPS Status LED"
    id: ups_status_led
    restore_mode: ALWAYS_OFF
    effects:
      # Critical conditions - Fast red strobe (unmistakable emergency pattern)
      - strobe:
          name: "Critical Alert"
          colors:
            - state: true
              brightness: 100%
              red: 100%
              green: 0%
              blue: 0%
              duration: 150ms
            - state: false
              duration: 150ms
      
      # Battery operation - Slow orange fade (amber warning pattern)
      - pulse:
          name: "Battery Warning"
          transition_length: 1.5s
          update_interval: 1.5s
          min_brightness: 10%
          max_brightness: 95%
      
      # Charging status - Quick yellow double-blink (charging pattern)
      - strobe:
          name: "Charging Pattern"
          colors:
            - state: true
              brightness: 80%
              red: 100%
              green: 100%
              blue: 0%
              duration: 200ms
            - state: false
              duration: 200ms
            - state: true
              brightness: 80%
              red: 100%
              green: 100%
              blue: 0%
              duration: 200ms
            - state: false
              duration: 1400ms
      
      # Normal operation - Slow green breathing (peaceful pattern)
      - pulse:
          name: "Normal Status"
          transition_length: 3s
          update_interval: 3s
          min_brightness: 20%
          max_brightness: 60%
      
      # Fault/offline - Blue fade (distinct from other states)
      - pulse:
          name: "System Offline"
          transition_length: 2s
          update_interval: 2s
          min_brightness: 5%
          max_brightness: 40%

# Priority-based LED control logic
interval:
  - interval: 2s
    then:
      - lambda: |-
          // Enhanced UPS status LED control with distinct visual patterns
          // Each state now has a unique color and pattern combination

          // CRITICAL CONDITIONS (Priority 1) - Fast red strobe
          if (id(low_battery).state || id(ups_fault).state || id(overload_warning).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 0.0, 0.0); // Bright red
            call.set_effect("Critical Alert");
            call.perform();
          }
          // BATTERY OPERATION (Priority 2) - Orange slow fade  
          else if (id(on_battery).state && !id(low_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 0.5, 0.0); // Orange amber
            call.set_effect("Battery Warning");
            call.perform();
          }
          // CHARGING STATUS (Priority 3) - Yellow double-blink pattern
          else if (id(battery_charging).state && id(ups_online).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 1.0, 0.0); // Yellow
            call.set_effect("Charging Pattern");
            call.perform();
          }
          // NORMAL OPERATION (Priority 4) - Gentle green breathing
          else if (id(ups_online).state && !id(on_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(0.0, 1.0, 0.0); // Green
            call.set_effect("Normal Status");
            call.perform();
          }
          // OFFLINE/UNKNOWN (Priority 5) - Dim blue fade
          else {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(0.0, 0.3, 1.0); // Blue
            call.set_effect("System Offline");
            call.perform();
          }
```

## Integration Examples

### Home Assistant Automation

```yaml
# automation.yaml
automation:
  - alias: "UPS Power Loss Alert"
    trigger:
      - platform: state
        entity_id: binary_sensor.ups_on_battery
        to: 'on'
    action:
      - service: notify.mobile_app
        data:
          message: "⚡ Power outage detected! UPS running on battery."
          
  - alias: "UPS Low Battery Critical"
    trigger:
      - platform: state
        entity_id: binary_sensor.ups_low_battery
        to: 'on'
    action:
      - service: script.shutdown_servers
      - service: notify.mobile_app
        data:
          message: "🔋 UPS battery critically low! Initiating shutdown sequence."
```

## FAQ

**Q: Can I monitor multiple UPS devices?**
A: Each ESP32-S3 supports one UPS via USB OTG. Use multiple ESP32-S3 devices for multiple UPS units.

**Q: Does this work with network-attached UPS devices?**
A: No, this component requires direct USB connection. For network UPS monitoring, use a different ESPHome component that supports network communication.

**Q: What's the power consumption?**
A: Typical ESP32-S3 power consumption: ~100-200mA @ 5V during normal operation.

**Q: Can I use this with other ESP32 variants?**
A: Only ESP32-S3 supports USB OTG required for direct UPS communication.

**Q: Why doesn't the beeper test button make any sound?**
A: UPS beepers are event-driven and only sound during actual alarm conditions (power loss, low battery, faults). The test button verifies write operations work correctly. To hear the actual beeper, test during a real power outage.

**Q: Does the mute button work?**
A: Mute only works when the UPS is actively beeping (during alarms). It acknowledges the current alarm and silences the beeper until the next alarm condition occurs.

**Q: Do beeper settings persist after ESP32 restart?**
A: Yes, beeper settings are stored in the UPS device's NVRAM and persist across both ESP32 and UPS reboots.

## Development

### Adding Custom Protocols

1. Inherit from `UpsProtocolBase`
2. Implement required methods: `detect()`, `initialize()`, `read_data()`
3. Register protocol using `REGISTER_UPS_PROTOCOL_FOR_VENDOR()` macro

### Testing

```yaml
# Use simulation mode for development
ups_hid:
  simulation_mode: true
```
