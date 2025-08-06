#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace nut_ups
  {

    static const char *const S_TAG = "nut_ups.sensor";

    void NutUpsSensor::dump_config()
    {
      ESP_LOGCONFIG(S_TAG, "NUT UPS Sensor:");
      ESP_LOGCONFIG(S_TAG, "  Type: %s", sensor_type_.c_str());
      LOG_SENSOR("  ", "Sensor", this);
    }

  } // namespace nut_ups
} // namespace esphome