# ESPHome Components Collection

A collection of production-ready ESPHome components for various hardware integrations and monitoring solutions.

## Available Components

### 🔋 UPS HID Component (`ups_hid`)

Monitor UPS devices via direct USB connection on ESP32-S3. Supports APC, CyberPower, and generic HID UPS devices with real-time monitoring of battery status, power conditions, and device information.

**Key Features:**
- **Real-time UPS monitoring**: Battery, voltage, load, runtime, and 15+ sensors
- **Multi-protocol support**: APC HID, CyberPower HID, Generic HID with auto-detection
- **UPS Control**: Beeper control (enable/disable/mute/test) and battery testing
- **Home Assistant integration**: Full device discovery and management
- **Developer-friendly**: Simulation mode, comprehensive logging

[📖 Full Documentation](components/ups_hid/README.md)

## Installation

### Using External Components (Recommended)

Add to your ESPHome configuration:

```yaml
external_components:
  - source: github://bullshit/esphome-components
    components: [ ups_hid ]  # Add more components as needed
```

### Manual Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/bullshit/esphome-components.git
   ```

2. Copy component directories to your ESPHome project:
   ```bash
   cp -r esphome-components/components/* /config/esphome/components/
   ```

3. Use local components in your configuration:
   ```yaml
   external_components:
     - source:
         type: local
         path: components
   ```

## Quick Start

### UPS HID Component Example

```yaml
esphome:
  name: ups-monitor
  platform: ESP32
  board: esp32-s3-devkitc-1

external_components:
  - source:
      type: local
      path: components

# Configure UPS monitoring
ups_hid:
  id: ups_monitor
  update_interval: 30s
  simulation_mode: false  # Set to true for testing

# Monitor battery level
sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: battery_level
    name: "UPS Battery Level"

# Monitor UPS status
binary_sensor:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    type: online
    name: "UPS Online"

# Control UPS beeper
button:
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: enable
    name: "UPS Beeper Enable"
  - platform: ups_hid
    ups_hid_id: ups_monitor
    beeper_action: mute
    name: "UPS Beeper Mute"
```

## Development

### Project Structure

```
components/
├── ups_hid/                 # UPS HID monitoring component
│   ├── README.md           # Component-specific documentation
│   ├── __init__.py         # Component configuration
│   ├── ups_hid.h           # Main component header
│   ├── ups_hid.cpp         # Main component implementation
│   ├── sensor.py           # Sensor platform
│   ├── binary_sensor.py    # Binary sensor platform
│   ├── text_sensor.py      # Text sensor platform
│   └── ...                 # Protocol implementations
├── [future_component]/      # Additional components
│   └── ...
└── ...

tools/
├── generate_vendor_list.py # Vendor ID synchronization tool
├── scan-usb.sh             # USB device scanning utility
└── README.md               # Tools documentation

.vscode/
├── tasks.json              # VSCode development tasks
```

### Development Tools

- **Generate Vendor List**: `python3 tools/generate_vendor_list.py`
  - Synchronizes vendor IDs between C++ and Python code
- **Scan USB Devices**: `bash tools/scan-usb.sh`
  - Lists connected ESP32, UPS, and serial devices
- **VSCode Tasks**: Integrated development tasks via Command Palette

### Adding New Components

1. Create component directory: `components/your_component/`
2. Implement core files:
   - `__init__.py` - Component configuration and validation
   - `your_component.h` - Component header
   - `your_component.cpp` - Component implementation
   - Platform files (`sensor.py`, etc.) as needed
3. Add component documentation: `components/your_component/README.md`
4. Update this main README with component description
5. Test with both real hardware and simulation where applicable

### Coding Guidelines

- Follow ESPHome coding standards and patterns
- Don't use exceptions (ESPHome disables them)
- Include comprehensive logging with appropriate levels
- Implement thread-safe code where necessary
- Provide simulation modes for testing
- Include detailed documentation and examples

## Hardware Requirements

Component-specific requirements are documented in each component's README:

- **UPS HID**: ESP32-S3-DevKitC-1 v1.1 with USB OTG support
- **Future components**: Requirements will be listed here

## License

Copyright bullshit <coding@ow-software.pl>

## Roadmap

### Planned Components

- 🌐 **NUT Server** (`nut_server`): A NUT (Network UPS Tools) server implementation that uses the ups_hid component for network-accessible UPS monitoring and supports minimal NUT commands with authentication
- 💡 **UPS Status LED** (`ups_status_led`): Smart LED status indicator component for UPS monitoring

### Current Status

- ✅ **UPS HID Component**: ESP-IDF v5.4 compatible  
  - ✅ **Hardware validated** - 100% success in stale data prevention testing
  - ✅ **Robust USB management** - Clean disconnect detection and graceful state corruption recovery  
  - ✅ **Safety-critical** - All sensors immediately show "unavailable" after disconnect (never stale data)
  - ✅ **Real-world tested** - APC Back-UPS ES 700G with USB power cycling
  - ✅ **Advanced protocols** - 15+ sensors per device with comprehensive monitoring
  - ✅ **UPS control** - Beeper control and battery testing via HID write operations
- ✅ **ESP-IDF Framework**: Fully migrated to v5.4.2 with enhanced stability
- ✅ **Documentation**: Comprehensive component and API documentation
- ✅ **Development Tools**: VSCode integration, USB scanning, vendor management
- 📋 **Future Components**: Planning and design phase
