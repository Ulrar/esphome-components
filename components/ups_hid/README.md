# UPS HID Component for ESPHome

A production-ready ESPHome component for monitoring UPS devices via USB connection on ESP32-S3. Direct USB HID communication with support for APC, CyberPower, and generic HID UPS devices.

## Features

- 🔋 **Real-time UPS monitoring**: Battery level, voltages, load, runtime, status
- 🌈 **Visual status indicator**: RGB LED with customizable status colors
- 🏠 **Home Assistant integration**: Automatic entity discovery via ESPHome API
- 🔌 **Multi-protocol support**: APC HID, CyberPower HID, Generic HID
- 🎯 **Auto-detection**: Intelligent protocol detection based on USB vendor IDs
- 🛡️ **Production-ready**: Thread-safe, error rate limiting, comprehensive logging
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
  auto_detect_protocol: true # Enable automatic protocol detection
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
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: manufacturer
    name: "UPS Manufacturer"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: model
    name: "UPS Model"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: protocol
    name: "UPS Protocol"
    
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: status
    name: "UPS Status"

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

| Vendor | Models | Protocol | Vendor ID |
|--------|--------|----------|-----------|
| **APC** | Back-UPS ES Series, Smart-UPS | APC HID | 0x051D |
| **CyberPower** | CP1500EPFCLCD, CP1000PFCLCD | CyberPower HID | 0x0764 |
| **Tripp Lite** | SMART1500LCDT, UPS series | Generic HID | 0x09AE |
| **Eaton/MGE** | Ellipse, Evolution series | Generic HID | 0x06DA |
| **Belkin** | Older USB UPS models | Generic HID | 0x050D |

### Protocol Compatibility Matrix

| Protocol | Communication | Auto-Detection | Features |
|----------|---------------|----------------|----------|
| **APC HID** | USB HID reports | ✅ | Modern APC, enhanced features |
| **CyberPower HID** | Vendor-specific HID | ✅ | CyberPower optimized |
| **Generic HID** | Standard HID-PDC | ✅ | Universal fallback |

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
  auto_detect_protocol: true     # Enable automatic protocol detection
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
- `manufacturer`: UPS manufacturer name
- `model`: UPS model number/name
- `serial_number`: UPS serial number (if available)
- `firmware_version`: UPS firmware version (if available)  
- `protocol`: Currently detected protocol name
- `status`: Combined UPS status text

## Advanced Configuration

### Custom Protocol Priority

```yaml
ups_hid:
  auto_detect_protocol: false    # Disable auto-detection
  # Component will use APC HID Protocol by default
```

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
- Try manual protocol selection: `auto_detect_protocol: false`
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

**Connection issues**:
```
[W][ups_hid:461] Failed to acquire USB mutex for write
[E][ups_hid:264] No compatible UPS protocol detected for vendor 0x051D
```

**Rate limiting (normal)**:
```
[W][ups_hid:185] Suppressed 5 similar error messages in the last 5000 ms
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

## Development

### Adding Custom Protocols

1. Inherit from `UpsProtocolBase`
2. Implement required methods: `detect()`, `initialize()`, `read_data()`
3. Add to protocol detection in `detect_ups_protocol()`
4. Update vendor ID mapping in `ups_vendors.h`

### Testing

```yaml
# Use simulation mode for development
ups_hid:
  simulation_mode: true
```
