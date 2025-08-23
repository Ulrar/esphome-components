# UPS HID Configuration Management

This directory provides a modular, maintainable approach to UPS HID device configuration using ESPHome packages with the new Protocol Factory system. Instead of maintaining large, complex configuration files, you can compose configurations from reusable components.

## 📦 Modular Configuration Packages

### **Core Packages** (Always Required)

#### `base_ups.yaml` 
Essential foundation for all UPS devices:
- ESP32-S3 hardware configuration
- Network setup (WiFi, OTA, Web server)
- UPS HID component initialization with Protocol Factory
- Status LED with smart patterns
- Essential binary sensors: `online`, `on_battery`, `low_battery`, `fault`
- Essential text sensors: `status` (as "UPS Status"), `protocol` (as "Detected Protocol")

#### `essential_sensors.yaml`
Core monitoring sensors (5 sensors) available on all protocols:
- **Sensors**: `battery_level`, `input_voltage`, `output_voltage`, `load_percent`, `runtime`  
- **Text Sensors**: `manufacturer`, `model`
- **Binary Sensors**: `charging`, `overload`
- **System Monitoring**: uptime, WiFi signal, IP address, ESPHome version
- Compatible with APC, CyberPower, and Generic protocols

### **Optional Enhancement Packages**

#### `extended_sensors.yaml` 
Advanced monitoring (17 additional sensors) for feature-rich devices:

**Enhanced Voltage Monitoring**:
- `battery_voltage`, `battery_voltage_nominal`, `input_voltage_nominal`
- `input_transfer_low`, `input_transfer_high`, `frequency`

**Power & Configuration**:
- `ups_realpower_nominal`, `ups_delay_shutdown`, `ups_delay_start`, `ups_delay_reboot`

**Dynamic Timer Monitoring** (negative values = no active countdown):
- `ups_timer_shutdown`, `ups_timer_start`, `ups_timer_reboot`
- `battery_runtime_low`

**Extended Device Information**:
- `serial_number`, `firmware_version`, `ups_beeper_status`, `input_sensitivity`
- `ups_mfr_date`, `battery_status`, `ups_firmware_aux`

**Smart Template**: Calculated load power using HID nominal power when available

#### `ups_controls.yaml`
Complete control functionality (10 buttons + test monitoring):

**Beeper Control**: `enable`, `disable`, `mute`, `test`
**UPS Testing**: `battery_quick`, `battery_deep`, `battery_stop`, `ups_test`, `ups_stop`  
**Test Monitoring**: `ups_test_result` text sensor

### **Device-Specific Optimization Packages**

#### `device_types/apc_backups_es.yaml`
APC Back-UPS ES series optimizations:
- Slower update intervals (15s - APC devices need more time)
- APC-specific power calculations (405W nominal for ES 700)
- Enhanced protocol debugging (`ups_hid.apc: DEBUG`)
- Fixed device-specific script references

#### `device_types/cyberpower_cp1500.yaml`
CyberPower CP1500 series optimizations:
- Fast update intervals (10s - CyberPower responds quickly)
- **Additional sensor**: `battery_charge_low` threshold
- **Enhanced power calculation**: Uses HID nominal power when available (900W default)
- **Device-specific text sensor**: `battery_mfr_date` (separate from UPS date)
- Rich monitoring script with comprehensive status logging

---

## 📊 **Complete Sensor Summary**

### **Package Composition Options:**

| Configuration Level | Packages Included | Total Sensors | Use Case |
|-------------------|------------------|--------------|----------|
| **Minimal** | `base_ups` + `essential_sensors` | **9 sensors** | Basic monitoring |
| **Complete** | + `extended_sensors` + `ups_controls` | **27 sensors + 10 controls** | Full featured |
| **Device-Optimized** | + device-specific package | **28+ sensors + 10 controls** | Production ready |

### **Sensor Breakdown by Type:**

#### **Numeric Sensors** (22 total available):
- **Essential (5)**: battery_level, input_voltage, output_voltage, load_percent, runtime
- **Extended (17)**: All enhanced voltage/power/timer/threshold monitoring

#### **Text Sensors** (10 total available):
- **Base (2)**: status, protocol *(in base_ups.yaml)*
- **Essential (2)**: manufacturer, model  
- **Extended (6)**: serial_number, firmware_version, ups_beeper_status, input_sensitivity, ups_mfr_date, battery_status, ups_firmware_aux
- **Controls (1)**: ups_test_result *(in ups_controls.yaml)*
- **Device-Specific (+1)**: battery_mfr_date *(CyberPower only)*

#### **Binary Sensors** (6 total):
- **Base (4)**: online, on_battery, low_battery, fault  
- **Essential (2)**: charging, overload

#### **Button Controls** (10 total, all in ups_controls.yaml):
- **Beeper (4)**: enable, disable, mute, test
- **Testing (6)**: battery_quick, battery_deep, battery_stop, ups_test, ups_stop

#### **Smart Templates** (2 calculated sensors):
- **Generic**: UPS Load Power (uses HID data when available)
- **Device-Specific**: Device-optimized power calculations

## Testing Configurations

### `testing/basic_test.yaml`
Minimal configuration for protocol detection and basic functionality:
```yaml
packages:
  base_ups: !include ../base_ups.yaml
  essential: !include ../essential_sensors.yaml
```
**Use for**: Initial device testing, protocol verification, basic connectivity

### `testing/beeper_test.yaml`
Focused beeper control testing:
```yaml
packages:
  base_ups: !include ../base_ups.yaml
  essential: !include ../essential_sensors.yaml
  controls: !include ../ups_controls.yaml
```
**Use for**: Testing beeper functionality, control operations

### `testing/simulation_test.yaml`
Complete testing without physical hardware:
```yaml
packages:
  base_ups: !include ../base_ups.yaml
  essential: !include ../essential_sensors.yaml
  extended: !include ../extended_sensors.yaml
  controls: !include ../ups_controls.yaml
```
**Use for**: Development, CI/CD testing, feature demonstration

## Production Examples

### `examples/apc-ups-monitor.yaml`
Complete APC UPS production configuration:
- Modular package composition
- Device-specific optimizations
- Custom automation examples
- Network and notification setup

### `examples/rack-ups-monitor.yaml`
Complete CyberPower UPS production configuration:
- Advanced sensor utilization
- Smart threshold monitoring
- Rich data analysis
- Enhanced automation

## Usage Patterns

### 1. Basic UPS Monitoring
```yaml
packages:
  base_ups: !include configs/base_ups.yaml
  essential: !include configs/essential_sensors.yaml
```
**Result**: ~150 lines → Essential monitoring only

### 2. Full-Featured UPS
```yaml
packages:
  base_ups: !include configs/base_ups.yaml
  essential: !include configs/essential_sensors.yaml  
  extended: !include configs/extended_sensors.yaml
  controls: !include configs/ups_controls.yaml
  device: !include configs/device_types/cyberpower_cp1500.yaml
```
**Result**: ~300 lines → Complete functionality

### 3. Custom Configuration
```yaml
substitutions:
  name: "my-custom-ups"
  update_interval: "30s"
  
packages:
  base_ups: !include configs/base_ups.yaml
  essential: !include configs/essential_sensors.yaml

# Add custom sensors or overrides here
sensor:
  - platform: template
    name: "Custom Power Calculation"
    # ... custom logic
```

## Configuration Management Best Practices

### 1. **Start Simple**
Begin with `basic_test.yaml` to verify device detection and protocol compatibility.

### 2. **Layer Functionality**
Add packages incrementally:
- Start with `base_ups.yaml` + `essential_sensors.yaml`
- Add `extended_sensors.yaml` if you need advanced metrics
- Include `ups_controls.yaml` for interactive features
- Apply device-specific packages last

### 3. **Use Substitutions for Customization**
Override defaults without modifying packages:
```yaml
substitutions:
  name: "office-ups"
  friendly_name: "Office UPS Monitor"
  update_interval: "30s"
  simulation_mode: "false"
```

### 4. **Device-Specific Optimization**
- **APC devices**: Use slower update intervals (15-30s)
- **CyberPower devices**: Can handle faster updates (5-10s)  
- **Generic devices**: Start conservative (30s) and adjust

### 5. **Environment-Specific Files**
```yaml
# development.yaml
substitutions:
  log_level: "VERBOSE"
  update_interval: "5s"
  simulation_mode: "true"

# production.yaml  
substitutions:
  log_level: "INFO"
  update_interval: "30s" 
  simulation_mode: "false"
```

### 6. **Testing Strategy**
- **Protocol Testing**: Use `basic_test.yaml`
- **Feature Testing**: Use specific test configurations
- **Integration Testing**: Use `simulation_test.yaml`
- **Production Validation**: Use device-specific examples
