# Development Tools

This folder contains development tools for ESP32 ESPHome component development and debugging utilities.

## Protocol Debug Collection

### collect-protocol-debug.sh

Automated collection of debug information for new UPS protocol support requests. This tool gathers all necessary data for implementing new UPS protocols.

**Usage:**
```bash
./tools/collect-protocol-debug.sh <ups_name>
```

**Features:**
- Collects USB device information and descriptors
- Captures NUT driver debug output for different states (online, battery, charging)
- Extracts HID paths and report IDs automatically
- Documents UPS variables and supported commands
- Creates organized output directory with all debug files
- Generates markdown-formatted summary ready for GitHub issues

**Requirements:**
- NUT (Network UPS Tools) installed
- UPS configured in `/etc/nut/ups.conf`
- sudo access for USB device access

**Output:**
Creates a timestamped directory containing:
- USB device descriptors and IDs
- HID report captures for each power state
- Complete UPS variable listings
- Summary report for issue creation

## USB Device Management

### scan-usb.sh

Lists ESP32, UPS, and related USB devices connected to the system. Useful for identifying device paths and vendor/product IDs during development.

## NUT Server Testing

### test_nut_server.py

Comprehensive test script for validating the NUT server component functionality. Tests NUT protocol commands, authentication, and concurrent connections.

**Usage:**
```bash
# Basic test with default settings
./test_nut_server.py 192.168.1.200

# Full test with custom parameters
./test_nut_server.py 192.168.1.200 --port 3493 --username nutmon --password nutsecret --ups test_ups

# Run specific test
./test_nut_server.py 192.168.1.200 --test auth

# Test concurrent connections
./test_nut_server.py 192.168.1.200 --test concurrent
```

**Test Categories:**
- `basic` - Test unauthenticated commands (HELP, VERSION, UPSDVER)
- `auth` - Test authenticated commands (LIST, GET VAR)
- `instant` - Test instant commands (beeper control)
- `error` - Test error handling with invalid commands
- `concurrent` - Test multiple simultaneous connections
- `all` - Run all tests (default)

## Protocol Development

The UPS HID component uses a modern self-registering protocol system. New protocols automatically register themselves using macros:

### Adding New Vendor-Specific Protocols

1. Create a new protocol class inheriting from `UpsProtocolBase`
2. Implement required methods: `detect()`, `initialize()`, `read_data()`
3. Register the protocol using the registration macro:

   ```cpp
   // At the end of your protocol .cpp file
   REGISTER_UPS_PROTOCOL_FOR_VENDOR(0x1234, my_protocol, 
       esphome::ups_hid::create_my_protocol, 
       "My Protocol Name", 
       "Description of my protocol", 
       100);  // Priority
   ```

### Universal Compatibility

All unknown UPS vendors automatically use the Generic HID protocol as a fallback, providing basic monitoring capabilities without requiring vendor-specific code. This ensures broad compatibility with minimal maintenance.