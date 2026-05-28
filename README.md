[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Gitter](https://badges.gitter.im/shelly-homekit/community.svg)](https://gitter.im/shelly-homekit/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

# Open Source Apple HomeKit Firmware for Shelly Devices

This firmware exposes Shelly devices as Apple HomeKit accessories.

Firmware is compatible with stock and can be uploaded via OTA (Watch a 2 minute [video](https://www.youtube.com/watch?v=BZc-kp4dDRw)), for more info take a look at the flashing wiki [here](https://github.com/mongoose-os-apps/shelly-homekit/wiki/Flashing#updating-from-stock-firmware).

Reverting to stock firmware is also possible [see here](https://github.com/mongoose-os-apps/shelly-homekit/wiki/Flashing#reverting-to-stock-firmware).

---

## Jablotron 100 HomeKit Security System Integration

This fork adds a **Security System** HAP service type for integrating a [Jablotron 100](https://www.jablotron.com/) alarm panel with Apple HomeKit using a **Shelly 1 Gen3**.

### How it works

The Shelly 1 G3 connects to two Jablotron bus modules:

| Jablotron Module | Role | Shelly Connection |
|---|---|---|
| **JB-111N** (BUS relay) | Reports armed/disarmed state | SW input (GPIO 10) |
| **JA-111H-AD TRB** (BUS relay receiver) | Receives arm/disarm impulse | Relay output (GPIO 5) |

When you arm or disarm via HomeKit (or the web UI), the Shelly sends a **500ms pulse** to the JA-111H-AD, which toggles the Jablotron alarm state. The JB-111N input reports the current armed state back to HomeKit in real time.

### Wiring

```
Jablotron JB-111N (NO output) → Shelly SW input
Shelly relay (NO) → Jablotron JA-111H-AD TRB input
```

Use **COM/NO** on JB-111N for the Shelly SW input (or COM/NC with Inverted Input enabled in the web UI).

### Configuration

1. Flash the firmware to a **Shelly 1 Gen3**
2. Open the Shelly web UI
3. In the **Switch 1** component, set **HAP Service Type** to **Security System**
4. Set a name (e.g. "Jablotron")
5. Click **Save** — the device will reboot and appear as a Security System in HomeKit

### Web UI

The Security System component shows:
- **Status**: Armed (Away) / Disarmed with color coding (red/green)
- **Arm / Disarm button**: triggers a 500ms relay pulse
- **Inverted Input**: flip logic if using COM/NC on JB-111N
- **Name** and **HAP Service Type** fields

### HomeKit behavior

| HomeKit State | Meaning |
|---|---|
| Disarmed | JB-111N contact open (alarm off) |
| Away Armed | JB-111N contact closed (alarm on) |

Arm/Disarm from HomeKit sends a pulse → Jablotron toggles state → JB-111N reports new state back → HomeKit updates.

### Verification / retry logic

After each arm/disarm pulse, the firmware waits 2 seconds and verifies the JB-111N state matches the requested state. If not, it retries up to 3 times before giving up and syncing the HomeKit state to reality.

---

## Supported devices and features

### Gen 3 Devices

|                                            |[1 G3]   |[1PM G3] |[2PM G3] |[I4 G3]  |[Mini1 G3]|[Mini1PM G3]|[PlugS G3]
|-                                           |-        |-        |-        |-        |-         |-           |-   
|Switch & Co.<sup>1</sup>                    |✓        |✓        |✓        |✗        |✓         |✓           |✓
|Stateless Input<sup>2</sup>                 |✓        |✓        |✓        |✓        |✓         |✓           |✗  
|Sensors<sup>3</sup>                         |✓        |✓        |✓        |✓        |✓         |✓           |✗  
|Garage door opener                          |✓        |✓        |✓        |✗        |✓         |✓           |✗  
|Roller shutter mode                         |✗        |✗        |✓        |✗        |✗         |✗           |✗  
|Power measurement                           |✗        |✓        |✓        |✗        |✗         |✓           |✓
|Temperature/Humidity measurement<sup>4</sup>|✓        |✓        |✓        |✓        |✗         |✗           |✗
|**Security System (Jablotron)**             |**✓**    |✗        |✗        |✗        |✗         |✗           |✗

### Plus devices

|                                            |[+1]|[+1Mini]|[+1PMMini]|[+1PM]|[+2PM]|+i4 [AC]/[DC]|[+Plug S]|[+Uni]
|-                                           |-   |-       |-         |-     |-     |-            |-        |-     
|Switch & Co.<sup>1</sup>                    |✓   |✓       |✓         |✓     |✓     |✗            |✓        |✓     
|Stateless Input<sup>2</sup>                 |✓   |✓       |✓         |✓     |✓     |✓            |✗        |✓    
|Sensors<sup>3</sup>                         |✓   |✓       |✓         |✓     |✓     |✓            |✗        |✓     
|Garage door opener                          |✓   |✓       |✓         |✓     |✓     |✗            |✗        |✓    
|Roller shutter mode                         |✗   |✗       |✗         |✗     |✓     |✗            |✗        |✗     
|Power measurement                           |✗   |✗       |-         |✓     |✓     |✗            |✓        |✗     
|Temperature/Humidity measurement<sup>4</sup>|✓   |✗       |✗         |✓     |✓     |✓            |✗        |✓     

### Light Controllers

|                         |[+RGBWPM]|
|-|-|
|Brightness control       |✓|
|CCT                      |✓|
|RGB(W)                   |✓|
|Switch & Co <sup>1</sup> |✓|
|Power measurement        |-|


### Pro devices

Currently not supported.

### Gen 1 switches

||[1]|1PM|[1L]|[Plug]|[PlugS]|2|2.5|i3|[UNI]|
|-|-|-|-|-|-|-|-|-|-|
|Switch & Co.<sup>1</sup>|✓|✓|✓|✓|✓|✓|✓|✗|✓|
|Stateless Input<sup>2</sup>|✓|✓|✓|✗|✗|✓|✓|✓|✓|
|Sensors<sup>3</sup>|✓|✓|✓|✗|✗|✓|✓|✓|✓|
|Temperature/Humidity measurement|✓<sup>4</sup>|✓<sup>4</sup>|✗|✗|✗|✗|✗|✗|✓|
|Garage door opener|✓|✓|✗|✗|✗|✓|✓|✗|✓|
|Roller shutter mode|✗|✗|✗|✗|✗|✗|✓|✗|✗|
|Power measurement|✗|✓|-|✓|✓|✗|✓|✗|✗|

### Gen 1 light bulbs / led strips

||[Duo]|[Duo RGBW]|[Vintage]|[RGBW2]|
|-|-|-|-|-|
|Brightness control|✓|✓|✓|✓|
|CCT|✓|✗|✗|✓|
|RGB(W)|✗|✓|✗|✓|
|Power measurement|-|-|-|-|

_Notes:_  
_✓: supported_  
_-: possible but not supported yet_  
_✗: not possible_  
_1: includes lock, outlet, valve and security system (Jablotron fork only)_  
_2: includes doorbell_  
_3: includes motion, occupancy, contact, smoke, leak_  
_4: with [Sensor AddOn/Shelly Plus AddOn](https://shop.shelly.cloud/temperature-sensor-addon-for-shelly-1-1pm-wifi-smart-home-automation#312) and DS18B20 sensor(s) (maximum 5 for Shelly Plus Addon, maximum 3 for Sensor AddOn) or 1 DHT sensor_

Features that are not yet supported:
 * Cloud connections: no Shelly Cloud, no MQTT
 * Remote actions (web hooks)
 * Valve with timer support

## Quick Start

### Updating from stock firmware

  * **Important:** Please update to the latest stock firmware prior to converting to Shelly-HomeKit (1.6 or later on Plus/Gen3 Devices).

  * Watch a 2 minute [video](https://www.youtube.com/watch?v=BZc-kp4dDRw).

    * *New:* One link for all device types: `http://A.B.C.D/ota?url=http://shelly.rojer.cloud/update`

  * See [here](https://github.com/mongoose-os-apps/shelly-homekit/wiki/Flashing#updating-from-stock-firmware) for detailed instructions.

### Flashing this fork (Jablotron integration)

Download the latest `shelly-homekit-Shelly1Gen3.zip` from [GitHub Actions](https://github.com/plx-t/shelly-homekit-jablotron/actions) and flash via the Shelly web UI:

**Firmware → Update from file → Choose File → Upload**

Or via curl:
```
curl -F "file=@shelly-homekit-Shelly1Gen3.zip" http://<SHELLY_IP>/update
```

## Documentation

See our [Wiki](https://github.com/mongoose-os-apps/shelly-homekit/wiki).

## Getting Support

If you'd like to report a bug or a missing feature, please use [GitHub issue tracker](https://github.com/mongoose-os-apps/shelly-homekit/issues).

## Contributions and Development

Code contributions are welcome! Check out [open issues](https://github.com/mongoose-os-apps/shelly-homekit/issues) and feel free to pick one up.

See [here](https://github.com/mongoose-os-apps/shelly-homekit/wiki/Development) for development environment setup.

## License

This firmware is free software and is distributed under [Apache 2.0 license](LICENSE).

[1]: https://www.shelly.cloud/en/products/shop/1xs1
[+1]: https://www.shelly.cloud/en/products/shop/shelly-plus-1
[+1Mini]: https://www.shelly.cloud/en/products/shop/shelly-plus-1-mini
[+1PMMini]: https://www.shelly.cloud/en/products/shop/shelly-plus-1pm-mini
[+Uni]: https://www.shelly.cloud/en/products/shop/shelly-plus-uni
[Mini1 G3]: https://www.shelly.cloud/en/products/shop/shelly-1-mini-gen-3
[Mini1PM G3]: https://www.shelly.cloud/en/products/shop/shelly-1-pm-mini-gen3
[2PM G3]: https://www.shelly.cloud/en/products/shop/shelly-2pm-gen3
[1PM G3]: https://www.shelly.cloud/en/products/shop/shelly-1pm-gen3
[1 G3]: https://www.shelly.cloud/en/products/shop/shelly-1-gen3
[I4 G3]: https://www.shelly.cloud/en/products/shop/shelly-i4-gen3
[PlugS G3]: https://www.shelly.com/products/shelly-plug-s-gen3
[+1PM]: https://www.shelly.cloud/en/products/shop/shelly-plus-1-pm-2-pack/shelly-plus-1-pm
[+2PM]: https://www.shelly.cloud/en/products/shop/shelly-plus-2-pm
[+RGBWPM]: https://www.shelly.cloud/en/products/shop/shelly-plus-rgbw-pm
[+Plug S]: https://www.shelly.cloud/en/products/shop/shelly-plus-plug-s
[1L]: https://www.shelly.cloud/en/products/shop/shelly-1l
[Plug]: https://www.shelly.cloud/en/products/shop/1xplug
[PlugS]: https://www.shelly.cloud/en/products/shop/shelly-plug-s
[AC]: https://www.shelly.cloud/en-de/products/product-overview/splusi4x1
[DC]: https://www.shelly.cloud/en-de/products/product-overview/shelly-plus-i4-dc
[UNI]: https://www.shelly.cloud/en/products/shop/shelly-uni-1
[RGBW2]: https://www.shelly.cloud/en/products/shop/shelly-rgbw2-1
[Duo RGBW]: https://www.shelly.cloud/en/search?query=%22Shelly+Duo+-+RGBW%22
[Duo]: https://www.shelly.cloud/en/search?query=%22Shelly+Duo%22
[Vintage]: https://www.shelly.cloud/en/search?query=vintage
