#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "ups_hid.h"

namespace esphome {
namespace ups_hid {

class UpsHidButton : public button::Button, public Component {
 public:
  void set_ups_hid_parent(UpsHidComponent *parent) { parent_ = parent; }
  void set_beeper_action(const std::string &action) { beeper_action_ = action; }
  
  void dump_config() override;

 protected:
  void press_action() override;
  
  UpsHidComponent *parent_{nullptr};
  std::string beeper_action_{};
};

}  // namespace ups_hid
}  // namespace esphome