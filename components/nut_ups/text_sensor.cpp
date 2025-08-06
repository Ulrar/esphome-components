#include "text_sensor.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace nut_ups
  {

    static const char *const TXT_TAG = "nut_ups.text_sensor";

    void NutUpsTextSensor::dump_config()
    {
      ESP_LOGCONFIG(TXT_TAG, "NUT UPS Text Sensor:");
      ESP_LOGCONFIG(TXT_TAG, "  Type: %s", sensor_type_.c_str());
      LOG_TEXT_SENSOR("  ", "Text Sensor", this);
    }

  } // namespace nut_ups
} // namespace esphome