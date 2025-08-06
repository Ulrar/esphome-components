#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "nut_ups.h"

namespace esphome
{
  namespace nut_ups
  {

    class NutUpsSensor : public sensor::Sensor, public Component
    {
    public:
      void set_sensor_type(const std::string &type) { sensor_type_ = type; }
      void dump_config() override;

    protected:
      std::string sensor_type_;
    };

  } // namespace nut_ups
} // namespace esphome