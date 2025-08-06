#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "nut_ups.h"

namespace esphome
{
  namespace nut_ups
  {

    class NutUpsBinarySensor : public binary_sensor::BinarySensor, public Component
    {
    public:
      void set_sensor_type(const std::string &type) { sensor_type_ = type; }
      void dump_config() override;

    protected:
      std::string sensor_type_;
    };

  } // namespace nut_ups
} // namespace esphome