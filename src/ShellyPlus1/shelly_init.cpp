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
  outputs->emplace_back(new OutputPin(1, RELAY1_GPIO, 1));

  auto *in1 = new InputPin(1, SWITCH1_GPIO, 1, MGOS_GPIO_PULL_NONE, true);
  in1->AddHandler(std::bind(&HandleInputResetSequence, in1, LED_GPIO, _1, _2));
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

  std::unique_ptr<hap::SecuritySystem> sec(
      new hap::SecuritySystem(1, in, out, SHELLY_HAP_IID_BASE_SWITCH));
  auto st = sec->Init();
  if (!st.ok()) {
    LOG(LL_ERROR, ("SecuritySystem init failed: %s", st.ToString().c_str()));
    return;
  }

  hap::SecuritySystem *sec2 = sec.get();
  comps->push_back(std::move(sec));

  mgos::hap::Accessory *pri_acc = accs->front().get();
  pri_acc->SetCategory(kHAPAccessoryCategory_SecuritySystems);
  pri_acc->AddService(sec2);

  (void) svr;
}

}  // namespace shelly
