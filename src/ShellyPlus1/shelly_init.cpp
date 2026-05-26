// src/ShellyPlus1/shelly_init.cpp
//
// Shelly 1 G3 peripheral and component initialisation.
// Modified for Jablotron 100 SecuritySystem integration.
//
// Original CreateComponents creates a Switch + optional sensors.
// This version replaces the Switch with a SecuritySystem service.
//
// Hardware mapping (Shelly 1 G3 / ESP32-C3):
//   GPIO 7  → SW input   ← JB-111N relay NO (closed = section armed)
//   GPIO 26 → Relay      → JA-111H-AD input 2 (impulse toggle)
//
// Diff from stock:
//   + #include "shelly_hap_security_system.hpp"
//   + CreateComponents instantiates SecuritySystem instead of Switch

#include "shelly_hap_security_system.hpp"
#include "shelly_input_pin.hpp"
#include "shelly_main.hpp"
#include "shelly_output_pin.hpp"
#include "shelly_temp_sensor.hpp"

// The stock ShellyPlus1 init also supports temp sensors via AddOn;
// we preserve that wiring but the security system uses SW + Relay only.

namespace shelly {

// ─────────────────────────────────────────────────────────────────────────────
// CreatePeripherals — GPIO wiring, identical to stock Shelly 1 G3 init
// ─────────────────────────────────────────────────────────────────────────────

void CreatePeripherals(std::vector<std::unique_ptr<Input>>    *inputs,
                       std::vector<std::unique_ptr<Output>>   *outputs,
                       std::vector<std::unique_ptr<PowerMeter>> *pms,
                       std::unique_ptr<TempSensor>            *sys_temp) {

  // Relay output — GPIO 26, active-high
  outputs->emplace_back(new OutputPin(1, 26, 1));

  // SW input — GPIO 7, active-high, with reset-sequence handler on GPIO 26
  auto *in1 = new InputPin(1, 7, 1, MGOS_GPIO_PULL_UP, true);
  in1->AddHandler(std::bind(&HandleInputResetSequence, in1, 26, _1, _2));
  in1->Init();
  inputs->emplace_back(in1);

  (void) pms;
  (void) sys_temp;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateComponents — register HAP accessories
// SecuritySystem replaces the stock Switch service
// ─────────────────────────────────────────────────────────────────────────────

void CreateComponents(std::vector<std::unique_ptr<Component>>     *comps,
                      std::vector<std::unique_ptr<mgos::hap::Accessory>> *accs,
                      HAPAccessoryServerRef                        *server) {

  // Retrieve peripherals created above
  Input  *status_input = FindInput(1);   // SW  → JB-111N
  Output *arm_output   = FindOutput(1);  // Relay → JA-111H-AD

  if (!status_input || !arm_output) {
    LOG(LL_ERROR, ("SecuritySystem: peripherals not found!"));
    return;
  }

  // Build the single HAP accessory
  auto *acc = new mgos::hap::Accessory(
      SHELLY_HAP_AID_BASE,
      kHAPAccessoryCategory_SecuritySystems,
      "Jablotron Security",            // display name in Home app
      GetIdentifyCB(),
      server);

  // Add mandatory HAP AccessoryInformation service (name, fw version, etc.)
  acc->AddHAPService(&mgos::hap::kAccessoryInformationService);

  // Add our custom SecuritySystem service
  acc->AddHAPService(hap::SecuritySystem::GetHAPService());

  // Instantiate the component — it wires input events and manages HAP state
  auto *sec = new hap::SecuritySystem(
      1,
      status_input,
      arm_output,
      server,
      acc->GetHAPAccessory(),
      mgos_rpc_get_global());

  auto st = sec->Init();
  if (!st.ok()) {
    LOG(LL_ERROR, ("SecuritySystem init failed: %s", st.ToString().c_str()));
    delete sec;
    delete acc;
    return;
  }

  comps->emplace_back(sec);
  accs->emplace_back(acc);

  LOG(LL_INFO, ("SecuritySystem: registered as HAP SecuritySystems accessory"));
}

}  // namespace shelly
