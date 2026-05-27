// src/shelly_hap_security_system.cpp
// Jablotron 100 HomeKit SecuritySystem integration for Shelly 1 G3

#include "shelly_hap_security_system.hpp"

#include "mgos.hpp"
#include "mgos_hap_chars.hpp"
#include "mgos_sys_config.h"

namespace shelly {
namespace hap {

SecuritySystem::SecuritySystem(int id, Input *status_input, Output *arm_output,
                               uint16_t iid_base)
    : Component(id),
      mgos::hap::Service(iid_base, &kHAPServiceType_SecuritySystem,
                         kHAPServiceDebugDescription_SecuritySystem),
      status_input_(status_input),
      arm_output_(arm_output) {
}

SecuritySystem::~SecuritySystem() {
  if (status_input_) status_input_->RemoveHandler(input_handler_id_);
}

Component::Type SecuritySystem::type() const {
  return Type::kSwitch;
}

std::string SecuritySystem::name() const {
  const char *n = mgos_sys_config_get_sw1_name();
  return (n ? n : "Security System");
}

Status SecuritySystem::Init() {
  arm_output_->SetState(false, "init");
  bool input_active = status_input_->GetState();
  UpdateCurrentState(input_active);
  target_state_ = IsArmed() ? 1 : 3;

  current_state_char_ = new mgos::hap::UInt8Characteristic(
      svc_.iid + 1, &kHAPCharacteristicType_SecuritySystemCurrentState, 0, 4, 1,
      [this](HAPAccessoryServerRef *, const HAPUInt8CharacteristicReadRequest *,
             uint8_t *value) {
        *value = current_state_;
        return kHAPError_None;
      },
      true, nullptr,
      kHAPCharacteristicDebugDescription_SecuritySystemCurrentState);
  AddChar(current_state_char_);

  target_state_char_ = new mgos::hap::UInt8Characteristic(
      svc_.iid + 2, &kHAPCharacteristicType_SecuritySystemTargetState, 0, 3, 1,
      [this](HAPAccessoryServerRef *, const HAPUInt8CharacteristicReadRequest *,
             uint8_t *value) {
        *value = target_state_;
        return kHAPError_None;
      },
      true,
      [this](HAPAccessoryServerRef *,
             const HAPUInt8CharacteristicWriteRequest *, uint8_t value) {
        if (value > 3) return kHAPError_InvalidData;
        bool want_armed = (value < 3);
        bool is_armed = IsArmed();
        target_state_ = (value < 3) ? 1 : 3;
        verify_retries_ = 0;
        if (want_armed == is_armed) {
          NotifyHomeKit();
        } else {
          SendPulse();
        }
        return kHAPError_None;
      },
      kHAPCharacteristicDebugDescription_SecuritySystemTargetState);
  AddChar(target_state_char_);

  input_handler_id_ = status_input_->AddHandler(
      [this](Input::Event ev, bool state) { OnInputChanged(ev, state); });

  LOG(LL_INFO,
      ("SecuritySystem %d init: input=%s cur=%d tgt=%d", id(),
       input_active ? "ARMED" : "DISARMED", current_state_, target_state_));
  return Status::OK();
}

void SecuritySystem::OnInputChanged(Input::Event ev, bool state) {
  if (ev != Input::Event::kChange) return;
  uint8_t old_state = current_state_;
  UpdateCurrentState(state);
  if (current_state_ == old_state) return;
  if (!IsArmed()) {
    target_state_ = 3;
  } else if (target_state_ == 3) {
    target_state_ = 1;
  }
  NotifyHomeKit();
  LOG(LL_INFO,
      ("SecuritySystem %d: state %d->%d", id(), old_state, current_state_));
}

void SecuritySystem::UpdateCurrentState(bool input_active) {
  current_state_ = input_active ? 1 : 3;
}

void SecuritySystem::SendPulse() {
  if (pulse_in_progress_) return;
  pulse_in_progress_ = true;
  arm_output_->SetState(true, "pulse");
  mgos_set_timer(kPulseMs, 0, PulseEndCallback, this);
}

void SecuritySystem::PulseEndCallback(void *arg) {
  auto *self = static_cast<SecuritySystem *>(arg);
  self->arm_output_->SetState(false, "pulse-end");
  self->pulse_in_progress_ = false;
  mgos_set_timer(kVerifyMs, 0, VerifyStateCallback, self);
}

void SecuritySystem::VerifyStateCallback(void *arg) {
  static_cast<SecuritySystem *>(arg)->VerifyState();
}

void SecuritySystem::VerifyState() {
  if (WantsArmed() == IsArmed()) {
    verify_retries_ = 0;
    return;
  }
  if (verify_retries_ < kMaxRetries) {
    verify_retries_++;
    SendPulse();
  } else {
    verify_retries_ = 0;
    target_state_ = IsArmed() ? 1 : 3;
    NotifyHomeKit();
  }
}

void SecuritySystem::NotifyHomeKit() {
  if (current_state_char_) current_state_char_->RaiseEvent();
  if (target_state_char_) target_state_char_->RaiseEvent();
}

StatusOr<std::string> SecuritySystem::GetInfo() const {
  return mgos::SPrintf("cur:%d tgt:%d", current_state_, target_state_);
}

StatusOr<std::string> SecuritySystem::GetInfoJSON() const {
  const char *n = mgos_sys_config_get_sw1_name();
  return mgos::JSONPrintStringf(
      "{id: %d, type: %d, name: %Q, svc_type: %d, hk_state_inverted: %B,"
      " valve_type: %d, in_mode: %d, in_inverted: %B, initial: %d,"
      " state: %B, auto_off: %B, auto_off_delay: %.3f, state_led_en: %d,"
      " out_inverted: %B, hdim: %B}",
      id(), (int) type(), (n ? n : "Security System"), 4, false, -1,
      (int) mgos_sys_config_get_sw1_in_mode(),
      (bool) mgos_sys_config_get_sw1_in_inverted(),
      (int) mgos_sys_config_get_sw1_initial_state(), current_state_ < 3, false,
      0.0, -1, (bool) mgos_sys_config_get_sw1_out_inverted(), false);
}

Status SecuritySystem::SetConfig(const std::string &config_json,
                                 bool *restart_required) {
  LOG(LL_INFO, ("SetConfig: [%s]", config_json.c_str()));
  struct mgos_config_sw *cfg =
      (struct mgos_config_sw *) mgos_sys_config_get_sw1();
  char *name = nullptr;
  int in_mode = -2, svc_type = -2, in_inverted = -1, out_inverted = -1;
  json_scanf(config_json.c_str(), config_json.size(),
             "{name: %Q, in_mode: %d, svc_type: %d, in_inverted: %B,"
             " out_inverted: %B}",
             &name, &in_mode, &svc_type, &in_inverted, &out_inverted);
  LOG(LL_INFO, ("SetConfig: in_inv=%d out_inv=%d", in_inverted, out_inverted));
  if (name != nullptr) cfg->name = name;
  if (in_mode != -2) cfg->in_mode = in_mode;
  if (in_inverted >= 0) cfg->in_inverted = in_inverted;
  if (out_inverted >= 0) cfg->out_inverted = out_inverted;
  if (svc_type != -2 && svc_type != cfg->svc_type) {
    cfg->svc_type = svc_type;
    if (restart_required) *restart_required = true;
  }
  return mgos_sys_config_save(&mgos_sys_config, false, nullptr)
             ? Status::OK()
             : mgos::Errorf(STATUS_UNKNOWN, "failed to save config");
}

Status SecuritySystem::SetState(const std::string &state_json) {
  int state = -1;
  struct json_token state_tok = JSON_INVALID_TOKEN;
  json_scanf(state_json.c_str(), state_json.size(), "{state: {state: %T}}",
             &state_tok);
  if (state_tok.len > 0) state = (state_tok.ptr[0] == 't') ? 1 : 0;
  if (state < 0) return Status::OK();
  bool want_armed = (state != 0);
  bool is_armed = IsArmed();
  target_state_ = want_armed ? 1 : 3;
  if (want_armed != is_armed) SendPulse();
  NotifyHomeKit();
  return Status::OK();
}

}  // namespace hap
}  // namespace shelly
