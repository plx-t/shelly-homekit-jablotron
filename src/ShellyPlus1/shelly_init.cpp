/*
 * Shelly 1 G3 init — Jablotron 100 SecuritySystem
 *
 * GPIO 7  → SW input  ← JB-111N relay NO (closed = section armed)
 * GPIO 26 → Relay     → JA-111H-AD input 2 (impulse toggle)
 */

#include "mgos.hpp"
#include "mgos_hap_accessory.hpp"

#include "shelly_hap_security_system.hpp"
#include "shelly_input_pin.hpp"
#include "shelly_main.hpp"
#include "shelly_output.hpp"
#include "shelly_temp_sensor.hpp"

namespace shelly {

void CreatePeripherals(std::vector<std::unique_ptr<Input>> *inputs,
                       std::vector<std::unique_ptr<Output>> *outputs,
                       std::vector<std::unique_ptr<PowerMeter>> *pms,
                       std::unique_ptr<TempSensor> *sys_temp) {
  outputs->emplace_back(new OutputPin(1, 26, 1));

  auto *in1 = new InputPin(1, 7, 1, MGOS_GPIO_PULL_UP, true);
  in1->AddHandler(std::bind(&HandleInputResetSequence, in1, 26, _1, _2));
  in1->Init();
  inputs->emplace_back(in1);

  (void) pms;
  (void) sys_temp;
}

void CreateComponents(std::vector<std::unique_ptr<Component>> *comps,
                      std::vector<std::unique_ptr<mgos::hap::Accessory>> *accs,
                      HAPAccessoryServerRef *svr) {
  Input *in = FindInput(1);
  Output *out = FindOutput(1);

  if (!in || !out) {
    LOG(LL_ERROR, ("SecuritySystem: peripherals not found"));
    return;
  }

  // Pass IID base via constructor — same pattern as MotionSensor, Lock, etc.
  auto *sec = new hap::SecuritySystem(1, in, out, SHELLY_HAP_IID_BASE_SWITCH);

  auto st = sec->Init();
  if (!st.ok()) {
    LOG(LL_ERROR, ("SecuritySystem init: %s", st.ToString().c_str()));
    delete sec;
    return;
  }

  auto *acc = new mgos::hap::Accessory(
      SHELLY_HAP_AID_BASE_SWITCH, kHAPAccessoryCategory_SecuritySystems,
      mgos_sys_config_get_sw1_name(), GetIdentifyCB(), svr);
  acc->AddService(sec);

  comps->emplace_back(sec);
  accs->emplace_back(acc);
}

}  // namespace shelly
