# ESPHome Components Collection

A collection of ESPHome components for various hardware integrations and monitoring solutions.

## Available Components

### 🔋 UPS HID Component (`ups_hid`)

Monitor UPS devices via direct USB connection on ESP32-S3. Supports APC, CyberPower, and generic HID UPS devices with real-time monitoring of battery status, power conditions, and device information.

**Key Features:**
- **Real-time UPS monitoring**: Battery, voltage, load, runtime, and 15+ sensors
- **Multi-protocol support**: APC HID, CyberPower HID, Generic HID with auto-detection
- **UPS Control**: Beeper control (enable/disable/mute/test) and battery testing
- ⏱️ **Delay configuration**: Configure UPS shutdown, start, and reboot delays via USB HID
- **Home Assistant integration**: Full device discovery and management
- **Developer-friendly**: Simulation mode, comprehensive logging

[📖 Full Documentation](components/ups_hid/README.md)

### 💡 UPS Status LED Component (`ups_status_led`)

Smart LED status indicator for UPS monitoring with automatic pattern management and night mode. Provides visual status indication using solid colors with thread-safe runtime configuration.

**Key Features:**
- **7 solid color patterns**: Critical (red), Battery (orange), Charging (yellow), Normal (green), Offline (blue), No Data (purple), Error (white)
- **Night mode**: Time-based brightness dimming with color compensation for WS2812 LEDs
- **Home Assistant controls**: Enable/disable, brightness, night mode settings via web UI
- **Thread-safe operation**: Safe concurrent access from web UI and main loop
- **Minimum brightness logic**: 20% minimum ensures meaningful enable/disable distinction

[📖 Full Documentation](components/ups_status_led/README.md)

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
├── ups_status_led/         # Smart LED status indicator component
│   ├── README.md           # Component documentation
│   ├── __init__.py         # Component configuration
│   ├── ups_status_led.h    # Component header
│   └── ups_status_led.cpp  # Component implementation
├── [future_component]/      # Additional components
│   └── ...
└── ...

tools/
├── scan-usb.sh             # USB device scanning utility
└── README.md               # Tools documentation

.vscode/
├── tasks.json              # VSCode development tasks
```

### Development Tools

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
- **UPS Status LED**: WS2812 LED strip (1 LED), requires time component for night mode
- **Future components**: Requirements will be listed here

## License

Copyright bullshit <coding@ow-software.pl>

## Roadmap

### Planned Components

- 🌐 **NUT Server** (`nut_server`): A NUT (Network UPS Tools) server implementation that uses the ups_hid component for network-accessible UPS monitoring and supports minimal NUT commands with authentication

### Current Status

- ✅ **UPS HID Component**: ESP-IDF v5.4 compatible  
  - ✅ **Hardware validated** - 100% success in stale data prevention testing
  - ✅ **Robust USB management** - Clean disconnect detection and graceful state corruption recovery  
  - ✅ **Safety-critical** - All sensors immediately show "unavailable" after disconnect (never stale data)
  - ✅ **Real-world tested** - APC Back-UPS ES 700G with USB power cycling
  - ✅ **Advanced protocols** - 15+ sensors per device with comprehensive monitoring
  - ✅ **UPS control** - Beeper control and battery testing via HID write operations
- ✅ **UPS Status LED Component**: Thread-safe LED indicator with night mode
  - ✅ **SOLID architecture** - Clean separation of concerns within ESPHome constraints
  - ✅ **Thread-safe operation** - Safe concurrent web UI and main loop access
  - ✅ **Smart brightness** - 20% minimum hardware brightness for meaningful controls
  - ✅ **Night mode** - Time-based dimming with WS2812 color compensation
  - ✅ **Home Assistant integration** - Full web UI controls and status reporting
- ✅ **ESP-IDF Framework**: Fully migrated to v5.4.2 with enhanced stability
- ✅ **Documentation**: Comprehensive component and API documentation
- ✅ **Development Tools**: VSCode integration, USB scanning, vendor management
- 📋 **Future Components**: Planning and design phase
