// shelly_hap_security_system.hpp
// HomeKit SecuritySystem service for Shelly 1 G3 + Jablotron 100
//
// Hardware wiring:
//   status_input  (SW)    ← JB-111N relay NO contact  (closed = section armed)
//   arm_output    (Relay) → JA-111H-AD input 2        (impulse toggle arm/disarm)
//
// Logic:
//   - JB-111N is ground truth; current_state_ always mirrors it
//   - Commands send a 500ms pulse only if state needs to change
//   - External changes (keypad, app) are detected via input event and
//     pushed to HomeKit immediately
//   - After every pulse, state is verified against JB-111N with retries

#pragma once

#include "shelly_common.hpp"
#include "shelly_component.hpp"
#include "shelly_input.hpp"
#include "shelly_output.hpp"

#include "HAP.h"

namespace shelly {
namespace hap {

class SecuritySystem : public Component {
 public:
  SecuritySystem(int id,
                 Input *status_input,
                 Output *arm_output,
                 HAPAccessoryServerRef *server,
                 const HAPAccessory *accessory,
                 struct mg_rpc *rpc);

  virtual ~SecuritySystem();

  // Component interface
  Status Init() override;
  StatusOr<std::string> GetInfo() const override;
  StatusOr<std::string> GetInfoJSON() const override;
  Status SetConfig(const std::string &config_json,
                   bool *restart_required) override;

  // Returns the static HAP service descriptor for registration
  static const HAPService *GetHAPService();

  // HAP characteristic read/write callbacks (public for static linkage)
  static HAPError HandleCurrentStateRead(
      HAPAccessoryServerRef *server,
      const HAPUInt8CharacteristicReadRequest *request,
      uint8_t *value,
      void *context);

  static HAPError HandleTargetStateRead(
      HAPAccessoryServerRef *server,
      const HAPUInt8CharacteristicReadRequest *request,
      uint8_t *value,
      void *context);

  static HAPError HandleTargetStateWrite(
      HAPAccessoryServerRef *server,
      const HAPUInt8CharacteristicWriteRequest *request,
      uint8_t value,
      void *context);

 private:
  Input *const status_input_;
  Output *const arm_output_;
  HAPAccessoryServerRef *const server_;
  const HAPAccessory *const accessory_;

  // HAP state values
  // CurrentSecuritySystemState: 0=StayArm 1=AwayArm 2=NightArm 3=Disarmed 4=AlarmTriggered
  // TargetSecuritySystemState:  0=StayArm 1=AwayArm 2=NightArm 3=Disarmed
  uint8_t current_state_;
  uint8_t target_state_;

  // Pulse guard — prevents overlapping relay pulses
  bool pulse_in_progress_;

  // Retry counter for post-pulse state verification
  int verify_retries_;

  static constexpr int kPulseMs      = 500;   // relay ON duration (ms)
  static constexpr int kVerifyMs     = 3000;  // wait for panel to respond (ms)
  static constexpr int kMaxRetries   = 2;     // max verification retries

  // Convenience helpers
  bool IsArmed()    const { return current_state_ != 3; }
  bool WantsArmed() const { return target_state_  != 3; }

  // Called by Input event handler when JB-111N relay state changes
  void OnInputChanged(Input::Event ev, bool state);

  // Derive current_state_ from raw input level
  void UpdateCurrentState(bool input_active);

  // Raise HAP events for both current and target state characteristics
  void NotifyHomeKit();

  // Send a kPulseMs pulse to the JA-111H-AD toggle input
  void SendPulse();

  // Called kVerifyMs after a pulse; retries if panel didn't respond
  void VerifyState();

  // Static timer trampolines
  static void PulseEndCallback(void *arg);
  static void VerifyStateCallback(void *arg);
};

}  // namespace hap
}  // namespace shelly
