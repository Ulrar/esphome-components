# ESPHome NUT UPS Component

A production-ready ESPHome component for monitoring UPS devices via USB connection on ESP32-S3. Based on Network UPS Tools (NUT) protocols with support for APC, CyberPower, and generic HID UPS devices.

## Installation

### Using External Components (Recommended)

```yaml
# Add to your ESPHome configuration
external_components:
  - source: github://bullshit/esphome-nut-ups
    components: [ nut_ups ]
```

### Manual Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/bullshit/esphome-nut-ups.git
   ```

2. Copy the `components/nut_ups` directory to your ESPHome project:
   ```bash
   cp -r esphome-nut-ups/components/nut_ups /config/esphome/components/
   ```

3. Use local components in your configuration:
   ```yaml
   external_components:
     - source:
         type: local
         path: components
   ```

## Features

- 🔋 **Real-time UPS monitoring**: Battery level, voltages, load, runtime, status
- 🌈 **Visual status indicator**: RGB LED with customizable status colors
- 🏠 **Home Assistant integration**: Automatic entity discovery via ESPHome API
- 🔌 **Multi-protocol support**: APC Smart, APC HID, CyberPower HID, Generic HID
- 🎯 **Auto-detection**: Intelligent protocol detection based on USB vendor IDs
- 🛡️ **Production-ready**: Thread-safe, error rate limiting, comprehensive logging
- 🧪 **Simulation mode**: Test integration without physical UPS device

## Quick Start

### Hardware Requirements

- **ESP32-S3-DevKitC-1 v1.1** with USB OTG support
- **UPS device** with USB monitoring port
- **RGB LED** (optional, connected to GPIO38)
- **USB-A to USB-B cable** (UPS to ESP32-S3)

### Basic Configuration

```yaml
# example.yaml
esphome:
  name: ups-monitor
  platform: ESP32
  board: esp32-s3-devkitc-1

# Enable external components
external_components:
  - source:
      type: local
      path: components

# Configure the NUT UPS component
nut_ups:
  id: ups_monitor           # Required ID for referencing
  update_interval: 30s      # How often to poll UPS data
  usb_vendor_id: 0x051D     # APC vendor ID (auto-detected if omitted)  
  usb_product_id: 0x0002    # Optional: specific product ID
  protocol_timeout: 15s     # USB communication timeout
  auto_detect_protocol: true # Enable automatic protocol detection
  simulation_mode: false    # Set to true for testing without UPS

# Optional: RGB status LED (configured separately)
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO38
    num_leds: 1
    chipset: ws2812
    name: "UPS Status LED"
    id: ups_status_led
    
# Define sensors (each sensor requires separate platform entry)
sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: battery_level
    name: "UPS Battery Level"
    unit_of_measurement: "%"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: input_voltage
    name: "UPS Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: output_voltage
    name: "UPS Output Voltage" 
    unit_of_measurement: "V"
    device_class: voltage
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: load_percent
    name: "UPS Load"
    unit_of_measurement: "%"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: runtime
    name: "UPS Runtime"
    unit_of_measurement: "min"
    device_class: duration
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: frequency
    name: "UPS Frequency"
    unit_of_measurement: "Hz"

binary_sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: online
    name: "UPS Online"
    device_class: connectivity
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: on_battery
    name: "UPS On Battery"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: low_battery
    name: "UPS Low Battery"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: charging
    name: "UPS Charging"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: fault
    name: "UPS Fault"
    device_class: problem

text_sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: manufacturer
    name: "UPS Manufacturer"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: model
    name: "UPS Model"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: protocol
    name: "UPS Protocol"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: status
    name: "UPS Status"
```

## Hardware Setup

### ESP32-S3 USB OTG Connection

```
UPS USB Port (Type-B)  ←→  USB Cable  ←→  ESP32-S3 USB OTG Port
```

### RGB LED Wiring (Optional)

```
GPIO38 (ESP32-S3) → RGB LED Data Pin
3.3V → RGB LED VCC
GND → RGB LED GND
```

### LED Status Indicators

- 🟢 **Green**: UPS online, normal operation
- 🟠 **Orange**: UPS running on battery power
- 🔴 **Red (blinking)**: Low battery or critical status
- 🔵 **Blue**: Charging or maintenance mode
- ⚫ **Off**: UPS disconnected or component error

## Supported UPS Devices

### Tested Compatible Models

| Vendor | Models | Protocol | Vendor ID |
|--------|--------|----------|-----------|
| **APC** | Back-UPS ES Series, Smart-UPS | APC Smart, APC HID | 0x051D |
| **CyberPower** | CP1500EPFCLCD, CP1000PFCLCD | CyberPower HID | 0x0764 |
| **Tripp Lite** | SMART1500LCDT, UPS series | Generic HID | 0x09AE |
| **Eaton/MGE** | Ellipse, Evolution series | Generic HID | 0x06DA |
| **Belkin** | Older USB UPS models | Generic HID | 0x050D |

### Protocol Compatibility Matrix

| Protocol | Communication | Auto-Detection | Features |
|----------|---------------|----------------|----------|
| **APC Smart** | Serial over USB | ✅ | Full monitoring, legacy APC |
| **APC HID** | USB HID reports | ✅ | Modern APC, enhanced features |
| **CyberPower HID** | Vendor-specific HID | ✅ | CyberPower optimized |
| **Generic HID** | Standard HID-PDC | ✅ | Universal fallback |

## Configuration Reference

### Component Options

```yaml
nut_ups:
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

# Status LED configured separately (not part of nut_ups component)
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO38                  # GPIO pin for RGB LED
    num_leds: 1
    chipset: ws2812              # LED type (ws2812, ws2811, etc.)
    name: "UPS Status LED"
    id: ups_status_led
```

### Sensor Platform

Each sensor requires a separate platform entry with `nut_ups_id` and `type`:

```yaml
sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor      # Reference to component ID
    type: battery_level          # Battery charge percentage (0-100%)
    name: "Battery Level"
    unit_of_measurement: "%"
    device_class: battery
    accuracy_decimals: 1
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: input_voltage          # AC input voltage (V)
    name: "Input Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    accuracy_decimals: 1
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: output_voltage         # AC output voltage (V)
    name: "Output Voltage"
    unit_of_measurement: "V"
    device_class: voltage
    accuracy_decimals: 1
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: load_percent           # UPS load percentage (0-100%)
    name: "Load Percentage"
    unit_of_measurement: "%"
    accuracy_decimals: 1
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: runtime               # Estimated runtime (minutes)
    name: "Runtime"
    unit_of_measurement: "min"
    device_class: duration
    accuracy_decimals: 0
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
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

Each binary sensor requires `nut_ups_id` and `type`:

```yaml
binary_sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: online                 # UPS is online (AC power available)
    name: "UPS Online"
    device_class: connectivity
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: on_battery             # UPS running on battery
    name: "On Battery"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: low_battery            # Battery level is low
    name: "Low Battery"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: charging               # Battery is charging
    name: "Charging"
    device_class: battery
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: fault                  # UPS fault condition
    name: "Fault"
    device_class: problem
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
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

Each text sensor requires `nut_ups_id` and `type`:

```yaml
text_sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: manufacturer           # UPS manufacturer name
    name: "Manufacturer"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: model                  # UPS model number
    name: "Model"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: serial_number          # UPS serial number (if available)
    name: "Serial Number"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: firmware_version       # UPS firmware version (if available)
    name: "Firmware"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: protocol               # Detected protocol name
    name: "Protocol"
    
  - platform: nut_ups
    nut_ups_id: ups_monitor
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
nut_ups:
  auto_detect_protocol: false    # Disable auto-detection
  # Component will use APC Smart Protocol by default
```

### Performance Tuning

```yaml
nut_ups:
  id: ups_monitor
  update_interval: 10s           # Faster polling (minimum 5s recommended)
  protocol_timeout: 5s           # Faster timeout for responsive networks
  
# Individual sensor update intervals are controlled by the component
```

### Simulation Mode

For testing without physical UPS:

```yaml
nut_ups:
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
    nut_ups: DEBUG
    nut_ups.apc: DEBUG
    nut_ups.cyberpower: DEBUG
    nut_ups.generic: DEBUG
```

### Log Analysis

**Normal operation logs**:
```
[D][nut_ups:240] Detected known UPS vendor: APC (0x051D)
[I][nut_ups:371] Successfully detected APC HID protocol (took 156ms)
[D][nut_ups.apc:89] Battery: 85.0%, Runtime: 45.2 min
```

**Connection issues**:
```
[W][nut_ups:461] Failed to acquire USB mutex for write
[E][nut_ups:264] No compatible UPS protocol detected for vendor 0x051D
```

**Rate limiting (normal)**:
```
[W][nut_ups:185] Suppressed 5 similar error messages in the last 5000 ms
```

### Performance Monitoring

Monitor these metrics in Home Assistant:
- Update intervals (should be consistent)
- Protocol detection time (< 500ms typical)
- Error rates (should be minimal)
- USB communication success rate

## Advanced LED Status Control with Priorities

The component can control an RGB LED to display UPS status with priority-based visual indicators:

### LED Priority System

1. **🔴 Critical** (Highest Priority): Red blinking - Low battery, UPS fault, overload
2. **🟠 Battery Operation** (Medium Priority): Orange pulsing - Running on battery (normal level)  
3. **🟡 Charging** (Low Priority): Yellow pulsing - Battery charging while online
4. **🟢 Normal Operation**: Green breathing - Online, normal operation
5. **⚫ Offline/Unknown**: LED off - UPS disconnected or error

### Complete LED Configuration

```yaml
# RGB LED with custom effects
light:
  - platform: esp32_rmt_led_strip
    rgb_order: GRB
    pin: GPIO38
    num_leds: 1
    chipset: ws2812
    name: "UPS Status LED"
    id: ups_status_led
    restore_mode: ALWAYS_OFF
    effects:
      - strobe:
          name: "Critical Strobe"
          colors:
            - state: true
              brightness: 100%
              red: 100%
              green: 0%
              blue: 0%
              duration: 250ms
            - state: false
              duration: 250ms
      - pulse:
          name: "Battery Pulse"
          transition_length: 1s
          update_interval: 1s
          min_brightness: 20%
          max_brightness: 80%
      - pulse:
          name: "Normal Breathing"
          transition_length: 2s
          update_interval: 2s
          min_brightness: 30%
          max_brightness: 70%
      - pulse:
          name: "Battery Operation"
          transition_length: 800ms
          update_interval: 800ms
          min_brightness: 40%
          max_brightness: 90%

# Priority-based LED control logic
interval:
  - interval: 2s
    then:
      - lambda: |-
          // Advanced UPS status LED control with priorities

          // Critical conditions (highest priority) - Red blinking
          if (id(low_battery).state || id(ups_fault).state || id(overload_warning).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_effect("Critical Strobe");
            call.perform();
          }
          // Battery operation (medium priority) - Orange pulsing
          else if (id(on_battery).state && !id(low_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 0.6, 0.0); // Orange
            call.set_effect("Battery Operation");
            call.perform();
          }
          // Charging (low priority) - Yellow pulsing
          else if (id(battery_charging).state && id(ups_online).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(1.0, 1.0, 0.0); // Yellow
            call.set_effect("Battery Pulse");
            call.perform();
          }
          // Normal operation (online, not on battery) - Green breathing
          else if (id(ups_online).state && !id(on_battery).state) {
            auto call = id(ups_status_led).turn_on();
            call.set_rgb(0.0, 1.0, 0.0); // Green
            call.set_effect("Normal Breathing");
            call.perform();
          }
          // Offline/Unknown - LED off
          else {
            auto call = id(ups_status_led).turn_off();
            call.perform();
          }
```

## Integration Examples

### Advanced ESPHome Automations

```yaml
# Automated response scripts
script:
  - id: notify_critical
    then:
      - logger.log:
          format: "CRITICAL UPS EVENT: Battery=%.1f%%, Runtime=%.1f min, Status=%s"
          args:
            [
              "id(battery_level).state",
              "id(runtime_remaining).state", 
              "id(ups_status_text).state.c_str()",
            ]
          level: ERROR

  - id: ups_shutdown_warning
    then:
      - logger.log:
          format: "UPS SHUTDOWN WARNING: Battery critically low, runtime remaining: %.1f minutes"
          args: ["id(runtime_remaining).state"]
          level: WARN
      # Add your shutdown logic here if needed

  - id: power_restored
    then:
      - logger.log:
          format: "POWER RESTORED: UPS back online, battery at %.1f%%"
          args: ["id(battery_level).state"]
          level: INFO

# Event-driven automations with binary sensor triggers
binary_sensor:
  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: online
    name: "UPS Online"
    id: ups_online
    on_press:
      then:
        - script.execute: power_restored
    on_release:
      then:
        - logger.log:
            format: "POWER OUTAGE: UPS switched to battery power"
            level: WARN

  - platform: nut_ups
    nut_ups_id: ups_monitor
    type: low_battery
    name: "Low Battery Warning"
    id: low_battery
    on_press:
      then:
        - script.execute: ups_shutdown_warning
        - script.execute: notify_critical

# Template sensors for calculations
sensor:
  - platform: template
    name: "UPS Load Power"
    id: ups_load_power
    unit_of_measurement: "W"
    device_class: power
    state_class: measurement
    accuracy_decimals: 1
    lambda: |-
      if (!std::isnan(id(output_voltage).state) && !std::isnan(id(load_percentage).state)) {
        // Estimate power assuming typical UPS efficiency and load factor
        float estimated_power = (id(output_voltage).state * id(load_percentage).state) / 100.0;
        return estimated_power * 8.0; // Assume 8A max current for estimation
      }
      return NAN;
    update_interval: 30s
```

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

### Grafana Dashboard

Monitor UPS metrics over time:
- Battery level trends
- Voltage stability
- Load patterns
- Power outage frequency

## Development

### Adding Custom Protocols

1. Inherit from `UpsProtocolBase`
2. Implement required methods: `detect()`, `initialize()`, `read_data()`
3. Add to protocol detection in `detect_ups_protocol()`
4. Update vendor ID mapping in `ups_vendors.h`

### Testing

```yaml
# Use simulation mode for development
nut_ups:
  simulation_mode: true
```

## FAQ

**Q: Can I monitor multiple UPS devices?**
A: Each ESP32-S3 supports one UPS via USB OTG. Use multiple ESP32-S3 devices for multiple UPS units.

**Q: Does this work with network-attached UPS devices?**
A: No, this component requires direct USB connection. For network UPS monitoring, use the ESPHome NUT client component.

**Q: What's the power consumption?**
A: Typical ESP32-S3 power consumption: ~100-200mA @ 5V during normal operation.

**Q: Can I use this with other ESP32 variants?**
A: Only ESP32-S3 supports USB OTG required for direct UPS communication.

## License

This component is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork the repository**: [github.com/bullshit/esphome-nut-ups](https://github.com/bullshit/esphome-nut-ups)
2. **Create a feature branch**: `git checkout -b feature/your-feature-name`
3. **Test your changes**: Ensure code compiles and functions correctly
4. **Submit a Pull Request**: Include detailed description of changes

### Development Guidelines

- Follow ESPHome coding standards and patterns
- Don't use exceptions (ESPHome disables them)
- Include comprehensive logging with appropriate levels
- Test with both real hardware and simulation mode
- Update documentation for new features

## Support

For issues and questions:

1. **Check this documentation first** - Most common issues are covered
2. **Enable debug logging** and analyze the output:
   ```yaml
   logger:
     level: DEBUG
     logs:
       nut_ups: DEBUG
   ```
3. **Search existing issues**: [GitHub Issues](https://github.com/bullshit/esphome-nut-ups/issues)
4. **Create new issue** with:
   - Complete ESPHome configuration
   - Relevant log output
   - UPS model and vendor/product IDs
   - ESP32-S3 board information

### Reporting Bugs

Include this information in bug reports:
- ESPHome version
- ESP32-S3 board model  
- UPS make/model and USB vendor/product IDs
- Complete error logs with DEBUG level enabled
- Your ESPHome configuration (sanitized)

### Feature Requests

For new UPS protocol support:
- UPS vendor and model information
- USB vendor/product IDs (check logs)
- Any available protocol documentation
- Sample communication logs if possible