// src/ShellyPlus1/shelly_init.cpp
//
// Shelly 1 G3 peripheral and component initialisation.
// Modified for Jablotron 100 SecuritySystem integration.
//
// Hardware mapping (Shelly 1 G3 / ESP32-C3):
//   GPIO 7  → SW input   ← JB-111N relay NO (closed = section armed)
//   GPIO 26 → Relay      → JA-111H-AD input 2 (impulse toggle)

#include "shelly_hap_security_system.hpp"
#include "shelly_input_pin.hpp"
#include "shelly_main.hpp"
#include "shelly_output_pin.hpp"
#include "shelly_temp_sensor.hpp"

namespace shelly {

void CreatePeripherals(std::vector<std::unique_ptr<Input>>    *inputs,
                       std::vector<std::unique_ptr<Output>>   *outputs,
                       std::vector<std::unique_ptr<PowerMeter>> *pms,
                       std::unique_ptr<TempSensor>            *sys_temp) {

  outputs->emplace_back(new OutputPin(1, 26, 1));

  auto *in1 = new InputPin(1, 7, 1, MGOS_GPIO_PULL_UP, true);
  in1->AddHandler(std::bind(&HandleInputResetSequence, in1, 26, _1, _2));
  in1->Init();
  inputs->emplace_back(in1);

  (void) pms;
  (void) sys_temp;
}

void CreateComponents(std::vector<std::unique_ptr<Component>>            *comps,
                      std::vector<std::unique_ptr<mgos::hap::Accessory>> *accs,
                      HAPAccessoryServerRef                               *server) {

  Input  *status_input = FindInput(1);
  Output *arm_output   = FindOutput(1);

  if (!status_input || !arm_output) {
    LOG(LL_ERROR, ("SecuritySystem: peripherals not found!"));
    return;
  }

  auto *acc = new mgos::hap::Accessory(
      SHELLY_HAP_AID_BASE,
      kHAPAccessoryCategory_SecuritySystems,
      "Jablotron Security",
      GetIdentifyCB(),
      server);

  acc->AddHAPService(&mgos::hap::kAccessoryInformationService);

  auto *sec = new hap::SecuritySystem(
      1,
      status_input,
      arm_output,
      nullptr  /* mgos_config_sw* — unused, pass nullptr */);

  sec->set_service_iid(2);
  acc->AddService(sec);

  auto st = sec->Init();
  if (!st.ok()) {
    LOG(LL_ERROR, ("SecuritySystem init failed: %s", st.ToString().c_str()));
    delete acc;
    return;
  }

  comps->emplace_back(sec);
  accs->emplace_back(acc);

  LOG(LL_INFO, ("SecuritySystem registered"));
}

}  // namespace shelly
