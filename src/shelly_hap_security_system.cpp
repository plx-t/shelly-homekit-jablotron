// shelly_hap_security_system.cpp
// Jablotron 100 HomeKit SecuritySystem service for Shelly 1 G3
//
// See shelly_hap_security_system.hpp for full design notes.

#include "shelly_hap_security_system.hpp"

#include "mgos.hpp"
#include "mgos_hap.h"

namespace shelly {
namespace hap {

// ─────────────────────────────────────────────────────────────────────────────
// HAP UUIDs  (Apple-defined short UUIDs, per HAP spec Table 12-3)
// ─────────────────────────────────────────────────────────────────────────────

static const HAPUUID kUUIDSecuritySystem =
    HAPUUIDCreateAppleVendorDefined(0x7E);   // Security System service
static const HAPUUID kUUIDCurrentState =
    HAPUUIDCreateAppleVendorDefined(0x66);   // Current Security System State
static const HAPUUID kUUIDTargetState =
    HAPUUIDCreateAppleVendorDefined(0x67);   // Target Security System State

// ─────────────────────────────────────────────────────────────────────────────
// Characteristic: CurrentSecuritySystemState  (read-only, notifiable)
//   0 = Stay Arm  1 = Away Arm  2 = Night Arm  3 = Disarmed  4 = Alarm Triggered
// ─────────────────────────────────────────────────────────────────────────────

static const HAPUInt8Characteristic kCurrentStateChar = {
    .format = kHAPCharacteristicFormat_UInt8,
    .uuid   = &kUUIDCurrentState,
    .iid    = 0,  // assigned by HAP layer at runtime
    .properties = {
        .readable               = true,
        .writable               = false,
        .supportsEventNotification = true,
        .hidden                 = false,
        .requiresTimedWrite     = false,
        .supportsAuthorizationData = false,
        .ip  = {.controlPoint  = false},
        .ble = {.supportsBroadcastNotification    = false,
                .supportsDisconnectedNotification = false,
                .readableWithoutSecurity          = false,
                .writableWithoutSecurity          = false},
    },
    .units       = kHAPCharacteristicUnits_None,
    .constraints = {.minimumValue = 0, .maximumValue = 4, .stepValue = 1},
    .callbacks   = {
        .handleRead  = SecuritySystem::HandleCurrentStateRead,
        .handleWrite = nullptr,
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// Characteristic: TargetSecuritySystemState  (read-write, notifiable)
//   0 = Stay Arm  1 = Away Arm  2 = Night Arm  3 = Disarm
// ─────────────────────────────────────────────────────────────────────────────

static const HAPUInt8Characteristic kTargetStateChar = {
    .format = kHAPCharacteristicFormat_UInt8,
    .uuid   = &kUUIDTargetState,
    .iid    = 0,
    .properties = {
        .readable               = true,
        .writable               = true,
        .supportsEventNotification = true,
        .hidden                 = false,
        .requiresTimedWrite     = false,
        .supportsAuthorizationData = false,
        .ip  = {.controlPoint  = false},
        .ble = {.supportsBroadcastNotification    = false,
                .supportsDisconnectedNotification = false,
                .readableWithoutSecurity          = false,
                .writableWithoutSecurity          = false},
    },
    .units       = kHAPCharacteristicUnits_None,
    .constraints = {.minimumValue = 0, .maximumValue = 3, .stepValue = 1},
    .callbacks   = {
        .handleRead  = SecuritySystem::HandleTargetStateRead,
        .handleWrite = SecuritySystem::HandleTargetStateWrite,
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// Service definition
// ─────────────────────────────────────────────────────────────────────────────

static const HAPCharacteristic *kServiceChars[] = {
    &kCurrentStateChar,
    &kTargetStateChar,
    nullptr,
};

static const HAPService kSecurityService = {
    .iid              = 0,
    .serviceType      = &kUUIDSecuritySystem,
    .debugDescription = "SecuritySystem",
    .name             = "Security System",
    .properties       = {
        .primaryService = true,
        .hidden         = false,
        .ble            = {.supportsConfiguration = false},
    },
    .linkedServices   = nullptr,
    .characteristics  = kServiceChars,
};

// static accessor used by shelly_init.cpp
const HAPService *SecuritySystem::GetHAPService() {
  return &kSecurityService;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

SecuritySystem::SecuritySystem(int id,
                               Input *status_input,
                               Output *arm_output,
                               HAPAccessoryServerRef *server,
                               const HAPAccessory *accessory,
                               struct mg_rpc *rpc)
    : Component(id, rpc),
      status_input_(status_input),
      arm_output_(arm_output),
      server_(server),
      accessory_(accessory),
      current_state_(3),       // assume Disarmed until input is read
      target_state_(3),
      pulse_in_progress_(false),
      verify_retries_(0) {}

SecuritySystem::~SecuritySystem() {
  if (status_input_) status_input_->RemoveEventHandler();
}

// ─────────────────────────────────────────────────────────────────────────────
// Init — called once at startup after peripherals are ready
// ─────────────────────────────────────────────────────────────────────────────

Status SecuritySystem::Init() {
  // Guarantee relay is open at boot — we never assume arm state
  arm_output_->SetState(false, "init");

  // Read real panel state from JB-111N right now
  bool input_active = status_input_->GetState();
  UpdateCurrentState(input_active);

  // Align target with the real state so HomeKit starts consistent
  target_state_ = IsArmed() ? 1 /* AwayArm */ : 3 /* Disarmed */;

  // Subscribe to future JB-111N relay changes
  status_input_->AddEventHandler(
      [this](Input::Event ev, bool state) { OnInputChanged(ev, state); });

  LOG(LL_INFO,
      ("SecuritySystem %d init: input=%s → current_state=%d target_state=%d",
       id(), input_active ? "ARMED" : "DISARMED", current_state_,
       target_state_));

  return Status::OK();
}

// ─────────────────────────────────────────────────────────────────────────────
// Input event — JB-111N relay changed
// Fires for ANY arm/disarm event: HomeKit, keypad, MyJablotron app, automation
// ─────────────────────────────────────────────────────────────────────────────

void SecuritySystem::OnInputChanged(Input::Event ev, bool state) {
  if (ev != Input::Event::kChange) return;

  uint8_t old_state = current_state_;
  UpdateCurrentState(state);

  if (current_state_ == old_state) return;  // noise guard

  // Keep target in sync when changed externally so HomeKit stays consistent
  if (!IsArmed()) {
    // Panel was disarmed externally (keypad, app…)
    target_state_ = 3;
  } else if (target_state_ == 3) {
    // Panel was armed externally — default display mode = AwayArm
    target_state_ = 1;
  }
  // If HomeKit initiated this change target_state_ already reflects intent

  NotifyHomeKit();

  LOG(LL_INFO,
      ("SecuritySystem %d: JB-111N change → state %d→%d (input=%s)",
       id(), old_state, current_state_, state ? "ARMED" : "DISARMED"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Derive current_state_ from raw GPIO level
// ─────────────────────────────────────────────────────────────────────────────

void SecuritySystem::UpdateCurrentState(bool input_active) {
  // JB-111N wired NO-mode to Shelly SW:
  //   input_active = true  → relay closed → section ARMED
  //   input_active = false → relay open   → section DISARMED
  //
  // Preserve arm sub-mode from target_state_ so HomeKit shows
  // Stay/Away/Night correctly rather than always defaulting to AwayArm.
  if (input_active) {
    current_state_ = (target_state_ < 3) ? target_state_ : 1;  // AwayArm default
  } else {
    current_state_ = 3;  // Disarmed
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pulse logic — 500ms relay closure → JA-111H-AD impulse toggle input
// ─────────────────────────────────────────────────────────────────────────────

void SecuritySystem::SendPulse() {
  if (pulse_in_progress_) {
    LOG(LL_WARN, ("SecuritySystem %d: pulse already in progress, skipping", id()));
    return;
  }
  pulse_in_progress_ = true;
  arm_output_->SetState(true, "pulse");
  LOG(LL_INFO, ("SecuritySystem %d: pulse START → JA-111H-AD", id()));
  mgos_set_timer(kPulseMs, 0, PulseEndCallback, this);
}

// static timer callback — ends the relay pulse and schedules verification
void SecuritySystem::PulseEndCallback(void *arg) {
  auto *self = static_cast<SecuritySystem *>(arg);
  self->arm_output_->SetState(false, "pulse-end");
  self->pulse_in_progress_ = false;
  LOG(LL_INFO,
      ("SecuritySystem %d: pulse END, verify in %dms", self->id(), kVerifyMs));
  mgos_set_timer(kVerifyMs, 0, VerifyStateCallback, self);
}

// static timer callback — check whether panel responded to the pulse
void SecuritySystem::VerifyStateCallback(void *arg) {
  static_cast<SecuritySystem *>(arg)->VerifyState();
}

void SecuritySystem::VerifyState() {
  bool want_armed = WantsArmed();
  bool is_armed   = IsArmed();

  if (want_armed == is_armed) {
    // Panel responded correctly — all good
    verify_retries_ = 0;
    LOG(LL_INFO,
        ("SecuritySystem %d: verify OK → state=%d", id(), current_state_));
    return;
  }

  if (verify_retries_ < kMaxRetries) {
    verify_retries_++;
    LOG(LL_WARN,
        ("SecuritySystem %d: verify FAIL (want_armed=%d is_armed=%d) "
         "retry %d/%d",
         id(), want_armed, is_armed, verify_retries_, kMaxRetries));
    SendPulse();
  } else {
    // Give up — revert HomeKit to reflect real panel state
    verify_retries_ = 0;
    target_state_   = IsArmed() ? 1 : 3;
    NotifyHomeKit();
    LOG(LL_ERROR,
        ("SecuritySystem %d: all retries exhausted, "
         "reverting HomeKit to real state=%d",
         id(), current_state_));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// HomeKit notification helper
// ─────────────────────────────────────────────────────────────────────────────

void SecuritySystem::NotifyHomeKit() {
  HAPAccessoryServerRaiseEvent(server_, &kCurrentStateChar,
                               &kSecurityService, accessory_);
  HAPAccessoryServerRaiseEvent(server_, &kTargetStateChar,
                               &kSecurityService, accessory_);
}

// ─────────────────────────────────────────────────────────────────────────────
// HAP characteristic handlers
// ─────────────────────────────────────────────────────────────────────────────

HAPError SecuritySystem::HandleCurrentStateRead(
    HAPAccessoryServerRef * /*server*/,
    const HAPUInt8CharacteristicReadRequest * /*request*/,
    uint8_t *value,
    void *context) {
  *value = static_cast<SecuritySystem *>(context)->current_state_;
  return kHAPError_None;
}

HAPError SecuritySystem::HandleTargetStateRead(
    HAPAccessoryServerRef * /*server*/,
    const HAPUInt8CharacteristicReadRequest * /*request*/,
    uint8_t *value,
    void *context) {
  *value = static_cast<SecuritySystem *>(context)->target_state_;
  return kHAPError_None;
}

HAPError SecuritySystem::HandleTargetStateWrite(
    HAPAccessoryServerRef * /*server*/,
    const HAPUInt8CharacteristicWriteRequest * /*request*/,
    uint8_t value,
    void *context) {
  auto *self = static_cast<SecuritySystem *>(context);

  if (value > 3) return kHAPError_InvalidData;

  bool want_armed = (value < 3);
  bool is_armed   = self->IsArmed();

  self->target_state_   = value;
  self->verify_retries_ = 0;

  LOG(LL_INFO,
      ("SecuritySystem %d: HomeKit→target=%d want_armed=%d is_armed=%d",
       self->id(), value, want_armed, is_armed));

  if (want_armed == is_armed) {
    // Panel is already in the desired arm/disarm state.
    // If switching between arm sub-modes (e.g. Away→Night) just update
    // the label — no relay action needed since the panel is already armed.
    if (is_armed && self->current_state_ != value) {
      self->current_state_ = value;
    }
    self->NotifyHomeKit();
  } else {
    // State must change — send impulse toggle to JA-111H-AD.
    // current_state_ will update automatically when JB-111N fires.
    self->SendPulse();
  }

  return kHAPError_None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Component interface stubs
// ─────────────────────────────────────────────────────────────────────────────

StatusOr<std::string> SecuritySystem::GetInfo() const {
  return mgos::SPrintf("cur:%d tgt:%d pulse:%d retries:%d",
                       current_state_, target_state_,
                       (int) pulse_in_progress_, verify_retries_);
}

StatusOr<std::string> SecuritySystem::GetInfoJSON() const {
  return mgos::SPrintf(
      R"({"id":%d,"current_state":%d,"target_state":%d,"pulse":%s,"retries":%d})",
      id(), current_state_, target_state_,
      pulse_in_progress_ ? "true" : "false",
      verify_retries_);
}

Status SecuritySystem::SetConfig(const std::string & /*config_json*/,
                                  bool * /*restart_required*/) {
  return Status::OK();
}

}  // namespace hap
}  // namespace shelly
