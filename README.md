# ESPHome Components Collection

A collection of production-ready ESPHome components for various hardware integrations and monitoring solutions.

## Available Components

### 🔋 UPS HID Component (`ups_hid`)

Monitor UPS devices via direct USB connection on ESP32-S3. Supports APC, CyberPower, and generic HID UPS devices with real-time monitoring of battery status, power conditions, and device information.

**Key Features:**
- Real-time UPS monitoring (battery, voltage, load, runtime)
- Multi-protocol support (APC Smart, APC HID, CyberPower HID, Generic HID)
- Auto-detection of UPS protocols
- RGB LED status indicator support
- Home Assistant integration
- Simulation mode for testing

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

## Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork the repository**: [github.com/bullshit/esphome-components](https://github.com/bullshit/esphome-components)
2. **Create a feature branch**: `git checkout -b feature/your-feature-name`
3. **Follow coding guidelines**: See development section above
4. **Test thoroughly**: Include both unit and integration tests
5. **Update documentation**: Add/update README files as needed
6. **Submit a Pull Request**: Include detailed description of changes

### Development Environment

This repository includes a complete development environment with:

- **DevContainer**: Pre-configured development environment
- **VSCode Integration**: Tasks, debugging, and extensions
- **Development Tools**: USB scanning, vendor management, etc.
- **Test Configurations**: Example YAML files for testing

## Support

For issues and questions:

1. **Check component documentation** - Each component has detailed troubleshooting
2. **Enable debug logging** in your ESPHome configuration
3. **Search existing issues**: [GitHub Issues](https://github.com/bullshit/esphome-components/issues)
4. **Create new issue** with:
   - Component name and version
   - Complete ESPHome configuration
   - Relevant log output
   - Hardware information

## License

This project is open source and available under the MIT License.

## Roadmap

### Planned Components

- 📡 **RF Module**: ASK/OOK transceiver integration
- 🌡️ **Multi-Sensor Hub**: I2C sensor aggregation
- 🔌 **Smart Relay Controller**: Multi-channel relay management
- 📊 **Data Logger**: SD card logging with web interface

### Current Status

- ✅ **UPS HID Component**: Production ready
- 🔄 **Documentation**: Ongoing improvements
- 🔄 **Test Coverage**: Expanding automated tests
- 📋 **Future Components**: Planning and design phase

Stay tuned for updates and new component releases!