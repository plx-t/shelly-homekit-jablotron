#pragma once

#include "mgos_hap_chars.hpp"
#include "mgos_hap_service.hpp"

#include "shelly_common.hpp"
#include "shelly_component.hpp"
#include "shelly_input.hpp"
#include "shelly_output.hpp"

namespace shelly {
namespace hap {

class SecuritySystem : public Component, public mgos::hap::Service {
 public:
  SecuritySystem(int id, Input *status_input, Output *arm_output,
                 struct mgos_config_sw *cfg);
  virtual ~SecuritySystem();

  // Component interface
  Type type() const override;
  std::string name() const override;
  Status Init() override;
  StatusOr<std::string> GetInfo() const override;
  StatusOr<std::string> GetInfoJSON() const override;
  Status SetConfig(const std::string &config_json,
                   bool *restart_required) override;
  Status SetState(const std::string &state_json) override;

 private:
  Input *status_input_;
  Output *arm_output_;
  struct mgos_config_sw *cfg_;

  uint8_t current_state_ = 3;  // 3 = Disarmed
  uint8_t target_state_  = 3;
  bool    pulse_in_progress_ = false;
  int     verify_retries_    = 0;
  Input::HandlerID input_handler_id_ = Input::kInvalidHandlerID;

  static constexpr int kPulseMs   = 500;
  static constexpr int kVerifyMs  = 2000;
  static constexpr int kMaxRetries = 3;

  mgos::hap::UInt8Characteristic *current_state_char_ = nullptr;
  mgos::hap::UInt8Characteristic *target_state_char_  = nullptr;

  void OnInputChanged(Input::Event ev, bool state);
  void UpdateCurrentState(bool input_active);
  void SendPulse();
  void NotifyHomeKit();
  bool IsArmed() const { return current_state_ < 3; }
  bool WantsArmed() const { return target_state_ < 3; }

  static void PulseEndCallback(void *arg);
  static void VerifyStateCallback(void *arg);
  void VerifyState();
  
};

}  // namespace hap
}  // namespace shelly
