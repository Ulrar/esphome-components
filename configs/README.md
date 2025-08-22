# UPS HID Configuration Management

This directory provides a modular, maintainable approach to UPS HID device configuration using ESPHome packages. Instead of maintaining large, complex configuration files, you can compose configurations from reusable components.

## Modular Configuration Packages

### Core Packages

#### `base_ups.yaml` 
Essential foundation for all UPS devices:
- ESP32-S3 hardware configuration
- Network setup (WiFi, OTA, Web server)
- UPS HID component initialization
- Status LED with basic patterns
- Essential binary sensors for LED automation

#### `essential_sensors.yaml`
Core monitoring sensors available on all UPS protocols:
- Battery level, input/output voltage, load percentage
- Runtime remaining, basic device information
- System monitoring (uptime, WiFi signal)
- Compatible with APC, CyberPower, and Generic protocols

#### `extended_sensors.yaml` 
Advanced sensors for feature-rich devices:
- Battery voltage (actual and nominal)
- Input transfer limits and frequency
- UPS configuration parameters
- Power calculations and analysis
- Optimized for CyberPower devices

#### `ups_controls.yaml`
Interactive control functionality:
- Beeper control (enable/disable/mute/test)
- Battery testing (quick/deep/stop)
- Panel testing (start/stop)
- Test result monitoring

### Device-Specific Packages

#### `device_types/apc_backups_es.yaml`
APC Back-UPS ES series optimizations:
- Slower update intervals (APC devices need more time)
- APC-specific power calculations (405W nominal)
- Enhanced protocol debugging
- Device-specific status monitoring

#### `device_types/cyberpower_cp1500.yaml`
CyberPower CP1500 series optimizations:
- Fast update intervals (CyberPower responds quickly)
- Advanced threshold monitoring
- Extended device information
- Rich sensor data utilization

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
