#include "binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nut_ups {

static const char *const TAG_BINARY = "nut_ups.binary_sensor";

void NutUpsBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG_BINARY, "NUT UPS Binary Sensor:");
  ESP_LOGCONFIG(TAG_BINARY, "  Type: %s", sensor_type_.c_str());
  LOG_BINARY_SENSOR("  ", "Binary Sensor", this);
}

}  // namespace nut_ups
}  // namespace esphome