# MHI-AC-Ctrl for ESPHome (ESP-IDF)

ESPHome external component for controlling Mitsubishi Heavy Industries air conditioners over the MHI SPI-style bus using ESP32-class hardware.

This project is an ESP-IDF-focused rewrite and hardening of the existing MHI-AC-Ctrl ESPHome integration. It keeps the ESPHome and Home Assistant entity model while separating protocol decoding, transport, state, publication, diagnostics, command confirmation, and hardware-driver responsibilities.

Transport drivers are intentionally modular and portable. Each driver owns its ESPHome schema, target restrictions, compile definition, ESP-IDF dependencies, construction, hardware state, and diagnostics. The controller, transport manager, protocol decoder, command coordinator, and entity code depend only on common transport contracts. Adding or adapting a backend therefore requires local driver registration, code-generation wiring, tests, and hardware evidence rather than concrete-driver changes throughout the component.

The major rewrite and feature implementation phase is now substantially complete. The project has moved into compatibility validation, maintenance, and incremental protocol discovery rather than further architectural replacement.

This is not a clean-room protocol project. It builds on the original community MHI-AC-Ctrl work, upstream ESPHome component behaviour, and public MHI trace and capture knowledge.

## Current status

The primary runtime targets are the original **ESP32** and **ESP32-S3**, both using ESP-IDF. Core climate, fan, vane, 3D Auto, command-confirmation, worker, diagnostics, and transport work is implemented and hardware-tested on the available units.

- `fast_gpio_rx` with `fast_gpio_tx` remains the conservative compatibility baseline and the internal recovery path for hardware-assisted transports. Hardware validation on both ESP32 and ESP32-S3 showed clean protocol RX and reliable semantic commands, with the known cost of roughly 200 ms-class synchronous RX loop occupancy.
- `external_clock_rx` with `fast_gpio_tx` is **experimental**. Testing on both ESP32 and ESP32-S3 showed intermittent checksum/synchronisation corruption. The no-worker path is more reliable for commands, but the RX integrity issue remains, so this backend is not currently release-validated.
- `rmt_spi_rx` with `fast_gpio_tx` is a validated hardware-assisted split path on ESP32-S3 and completed an approximately 47.5-hour soak with clean RX protocol health. Hardware comparison testing showed `command_worker: false` is strongly preferred because worker polling materially increases FastGPIO TX misses and command-confirmation failures.
- `rmt_cs_spi` is the preferred consolidated FIFO-backed full-duplex path for the original dual-core ESP32 and ESP32-S3. It owns RX and TX, derives an internal chip-select boundary from the SCK idle gap, and uses `SPI_DMA_DISABLED` because 20-byte and 33-byte MHI frames fit within the SPI slave FIFO transaction capacity. Both worker and no-worker configurations have passed short hardware validation.
- The original ESP32 `rmt_cs_spi` path applies a target-specific mode-3 receive-edge correction. ESP32-S3 uses the normal ESP-IDF mode-3 configuration.
- The command coordinator now confirms commands from returned MOSI state, suppresses duplicates, retries bounded failures, and supersedes stale horizontal/3D confirmation generations with the latest composite intent.
- Vertical vane, horizontal vane, and 3D Auto mapping completed an 80-case hardware matrix with all requested combinations confirmed.
- Four-speed and three-speed fan profiles are supported. Four-speed is the default and exposes Quiet as a distinct protocol state.
- Outdoor Unit Silent Mode is available as an opt-in switch using the discovered `0xDD` operation-data feedback and `0x21` command path. It is model-dependent and still requires wider hardware validation.
- ESP32-C3 has compile coverage through the legacy FastGPIO path, but runtime operation is not yet validated.

The remaining work is primarily wider hardware compatibility testing, ESP32-C3 runtime validation, model-specific protocol discovery, documentation, and normal maintenance. No further major backend rewrite is currently planned.

Implemented functionality includes 20-byte and 33-byte frames, climate control, configurable fan profiles, vertical and horizontal vanes, 3D Auto, command confirmation, duplicate suppression, latest-intent command coalescing, common status sensors, opdata sensors, room-temperature publication control, external temperature input, and detailed runtime diagnostics.


### Runtime Active Mode

An optional Active Mode switch allows the component to remain connected as a
receive-only observer without transmitting commands or background frames:

```yaml
switch:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    active_mode:
      name: MHI Active Mode
```

Turning Active Mode off keeps RX, state publication, transport health, and
diagnostics running. Pending commands and staged TX are cleared immediately.
Turning it back on resumes transmission without replaying work queued before or
while listen-only mode was active. Active Mode defaults to on after boot.

## Driver selection

`rx_driver` is the primary selection. For split transports, TX is selected automatically. Existing configurations may still specify `tx_driver` explicitly.

The modular transport boundary keeps hardware-specific implementation details out of the rest of the component. Selecting one driver compiles only that primary backend and its required recovery path. An unselected driver does not add runtime branches or alter protocol, command, state, or entity behaviour. Portability does not imply universal chip support: every driver declares the exact ESP32 variants and framework it supports, and invalid selections fail during configuration.

See [Developing a new driver](docs/drivers/developing-drivers.md) for the required C++ contract, Python registration, compile guards, tests, and hardware-validation evidence.

### Driver combinations

| RX selection | Effective TX | Status |
|---|---|---|
| `fast_gpio_rx` | `fast_gpio_tx` | **Validated compatibility baseline on ESP32 and ESP32-S3** |
| `external_clock_rx` | `fast_gpio_tx` | **Experimental; RX integrity failure on ESP32 and ESP32-S3** |
| `rmt_spi_rx` | `fast_gpio_tx` | **Validated on ESP32-S3; no worker recommended** |
| `rmt_cs_spi` | Integrated full-duplex TX | **Preferred; validated on ESP32 and ESP32-S3** |

`Validated` means the path has passed hardware operation and regression testing on the listed target, while broader board and air-conditioner compatibility evidence may still be collected. `Experimental` means the backend is useful for engineering and comparison work but has a known hardware-validation failure and should not be presented as a stable runtime choice.

The split-driver paths remain available for compatibility and diagnostics. The full-duplex `rmt_cs_spi` path is now one FIFO-backed implementation across both supported chip families; the temporary non-DMA driver label and the DMA implementation have been removed.

`command_worker` remains disabled by default. Hardware testing shows that worker suitability is transport-dependent: it coexists cleanly with integrated `rmt_cs_spi`, but is not recommended for split `rmt_spi_rx` or `external_clock_rx` configurations that still depend on timing-sensitive `fast_gpio_tx`.

### Hardware driver guide

| ESP chip | Validated hardware | Recommended selection | Status | Notes |
|---|---|---|---|---|
| ESP32 | M5Stack Atom based on original ESP32 | `rmt_cs_spi` for preferred full-duplex operation; `fast_gpio_rx` as the compatibility fallback | **Validated** | `rmt_cs_spi` passed 33-byte RX/TX and command testing with and without the command worker. `external_clock_rx` remains experimental because intermittent RX corruption was reproduced on this target. |
| ESP32-S3 | Current ESP32-S3 test unit; M5Stack Atom S3 Lite | `rmt_cs_spi` for preferred full-duplex operation; `rmt_spi_rx` with `fast_gpio_tx` as the validated split alternative; `fast_gpio_rx` as compatibility fallback | **Validated** | `rmt_spi_rx` is clean with the worker disabled. `external_clock_rx` remains experimental because intermittent RX corruption was reproduced on this target. |
| ESP32-C3 | No runtime-validated board yet | `fast_gpio_rx` with `fast_gpio_tx` | **In development** | Representative compile coverage exists, including 20-byte frames and the three-speed fan profile. Runtime behaviour is not validated. |

Add tested boards or modules to the matching chip row as results become available. Keep one consolidated row per ESP chip family rather than creating a row for every board.

Conservative default configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: fast_gpio_rx
```

Preferred FIFO-backed full-duplex configuration for ESP32 or ESP32-S3:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8       # use pins appropriate to the selected board
  mosi_pin: 38
  miso_pin: 39
  rx_driver: rmt_cs_spi
  command_worker: true
```

`rmt_cs_spi` owns RX and TX and rejects a separate `tx_driver` override. No physical CS pin is required; RMT derives the internal transaction boundary from the SCK idle gap.

Validated split ESP32-S3 configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: rmt_spi_rx
  tx_driver: fast_gpio_tx
  command_worker: false
  rmt_spi_rx:
    frame_gap_us: 1000
```

For this split backend, leave the command worker disabled unless deliberately reproducing worker-specific diagnostics. Hardware comparison testing found materially more FastGPIO TX misses and command-confirmation failures with the worker enabled.

See [the driver documentation](docs/drivers/README.md) for backend design, hardware constraints, tuning options, and invalid combinations. See [`DIAGNOSTICS.md`](DIAGNOSTICS.md) for runtime counters, health interpretation, soak-test evidence, and troubleshooting. The consolidated bus, frame, field, and confirmation findings are in [`notes/FINDINGS_MHI_PROTOCOL.md`](notes/FINDINGS_MHI_PROTOCOL.md).

## Hardware assumptions

The MHI bus exposes SCK, MOSI, and MISO, with the air conditioner acting as the clock master. There is no physical chip-select line. Confirm the GPIO pin mapping for the specific board and installation before flashing; example pin numbers are not universal.

## Installation

Add this repository as an ESPHome external component.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Astute4185/MHI-AC-Ctrl-ESPHome_ESP-IDF
      ref: master
    components: [MhiAcCtrl]
```

## Examples

| Example | Purpose | Transport |
|---|---|---|
| [`examples/simple.yaml`](examples/simple.yaml) | Minimal conservative starting point | `fast_gpio_rx` with automatically selected `fast_gpio_tx` |
| [`examples/rmt_cs_spi.yaml`](examples/rmt_cs_spi.yaml) | Minimal hardware-assisted ESP32/ESP32-S3 configuration | Integrated `rmt_cs_spi` with `command_worker` |
| [`examples/full.yaml`](examples/full.yaml) | Full entity and sensor configuration | Integrated `rmt_cs_spi` with `command_worker` |
| [`examples/external_sensor.yaml`](examples/external_sensor.yaml) | Home Assistant room-temperature input | Integrated `rmt_cs_spi` with `command_worker` |
| [`examples/simple-energy-measurement.yaml`](examples/simple-energy-measurement.yaml) | Estimated power and session energy | Integrated `rmt_cs_spi` with `command_worker` |
| [`examples/UniversalAircoController.yaml`](examples/UniversalAircoController.yaml) | Universal Airco Controller v1.0 | Integrated `rmt_cs_spi` with board-specific pins |

The example pin values are not universal. Confirm the board mapping before flashing. The Universal Airco Controller installation guide is in [`UniversalAircoController/README.md`](UniversalAircoController/README.md).

## Minimal component configuration

Conservative FastGPIO configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: fast_gpio_rx
  room_temp_timeout: 60
```

Required component-level fields:

```text
id
frame_size
sck_pin
mosi_pin
miso_pin
```

Optional base fields:

```text
rx_driver
tx_driver
command_worker
room_temp_timeout
room_temperature_publish_interval
room_temperature_immediate_delta
opdata_freshness_timeout
opdata_poll_intervals
external_temperature_sensor
external_temperature_fallback_on_homeassistant_disconnect
external_temperature_disconnect_grace_period
fan_profile
power_estimation
```

Transport and command-worker tuning is optional and should normally be left at the selected backend's defaults. Driver-specific options are configured under the selected driver's nested subsection rather than as shared component fields. See [driver summary and tuning guide](docs/drivers/README.md#calling-a-driver-tunable) for the available tunables and when to use them.

## Room temperature publication rate limiting

The indoor unit can report small room-temperature changes many times per second. The component applies one shared publication gate to both the climate entity's `Current Temperature` and the standalone `room_temperature` sensor.

The defaults are:

```yaml
MhiAcCtrl:
  room_temperature_publish_interval: 15s
  room_temperature_immediate_delta: 1.0
```

The first reading is published immediately. Later changes smaller than `room_temperature_immediate_delta` are published no more than once per `room_temperature_publish_interval`. A change equal to or larger than the immediate delta is published without waiting. Other climate changes, such as mode, fan or target temperature, remain immediate and do not force a suppressed room-temperature value through.

For a unit that alternates between `20.25°C` and `20.75°C`, the default `1.0°C` immediate delta rate limits the noise while still allowing a larger real change to be reported promptly. Set `room_temperature_immediate_delta: 0.0` to publish every changed reading immediately.

## Worker mode

The command worker is optional and disabled by default:

```yaml
command_worker: false
```

The default preserves the conservative main-loop scheduling model. Enable the worker only when the selected transport has been validated with it:

```yaml
command_worker: true
```

The worker prepares immutable command frames, coordinates command lifecycle and confirmation, and drains supported queue-backed RX transports. The selected transport still owns all real-time bus timing, and confirmation begins only after the transport reports that the command frame was actually clocked onto the bus.

Current hardware guidance is transport-specific:

- `rmt_cs_spi`: worker and no-worker configurations both pass; worker mode is supported.
- `rmt_spi_rx` + `fast_gpio_tx`: use `command_worker: false`; worker polling increased TX misses and command-confirmation failures in hardware testing.
- `external_clock_rx` + `fast_gpio_tx`: keep the worker disabled for diagnostics; the backend still has an independent RX-integrity failure and is experimental.
- `fast_gpio_rx`: RX remains synchronous and main-loop driven regardless of the worker setting; the no-worker path is the validated compatibility configuration.

For queue-backed drivers, decoded status and opdata can be committed to bounded latest-value snapshots while the ESPHome main loop applies those snapshots and publishes entities.

Separate `rx_worker` and `tx_worker` settings are no longer used.

See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the current ownership and lifecycle model. See [`DIAGNOSTICS.md`](DIAGNOSTICS.md#command-worker-diagnostics) for worker counters and interpretation.

## Frame size

Use `frame_size: 20` for older units that do not support the larger frame layout.

Use `frame_size: 33` for units that support the larger frame layout and need features such as horizontal vanes or 3D Auto feedback.

If you see repeated checksum or signature failures, verify `frame_size` before changing anything else.

## Climate

```yaml
climate:
  - platform: MhiAcCtrl
    name: Guest Bedroom AC
    mhi_ac_ctrl_id: mhi_ac
    visual_min_temperature: 16
    visual_max_temperature: 31
```

The default setpoint step is **1°C**; no extra configuration is required.
To opt into native 0.5°C setpoints, add `setpoint_step: 0.5`.
The setting controls both the advertised HA target-temperature step and
rounding of commands received through the UI or API. The existing
`visual_temperature_step` and standard ESPHome
`visual.temperature_step.target_temperature` options are accepted as
compatible alternatives; do not specify conflicting step values.

MHI DB2 supports 0.5°C encoding, but individual indoor units may round a
half-degree request to a whole degree. Climate state is published from
confirmed decoded MHI state, so the dial will revert if the AC reports a
different setpoint. `temperature_offset: true` is a legacy setting that
rounds upwards and **does not currently implement** the external room-
temperature compensation necessary to emulate half-degree resolution on
an AC that rejects it. It should not be combined with a 0.5°C setpoint step.

Example opt-in:

```yaml
climate:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    name: Guest Bedroom AC
    setpoint_step: 0.5
```

## Selects

```yaml
select:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    fan_speed:
      name: Fan Speed
    vertical_vanes:
      name: Fan Control Up Down
    horizontal_vanes:
      name: Fan Control Left Right
```

The component defaults to the four-speed profile. No `fan_profile` setting is required for normal use:

```yaml
MhiAcCtrl:
  id: mhi_ac
  # fan_profile defaults to four_speed
```

The default exposes:

- Auto
- Quiet
- Low
- Medium
- High

Mapping:

```text
MOSI 0 -> Quiet
MOSI 1 -> Low
MOSI 2 -> Medium
MOSI 6 -> High
MOSI 7 -> Auto
```

Hardware testing on both available AC models confirmed that protocol value `0` is accepted, returned in status, and distinct from Low. Quiet, Low, Medium, High, and Auto all completed command confirmation successfully.

For a model that genuinely does not support Quiet, explicitly select the compatibility profile:

```yaml
MhiAcCtrl:
  fan_profile: three_speed
```

The three-speed profile exposes Auto, Low, Medium, and High. It presents received protocol values `0` and `1` as Low, while outgoing Low commands continue to use protocol value `1`.

The selected profile controls climate traits, fan-select options, status publishing, TX command encoding, and command confirmation.

Supported vertical vane options:

- Up
- Up/Center
- Center/Down
- Down
- Swing

Supported horizontal vane options:

- Left
- Left/Center
- Center
- Center/Right
- Right
- Wide
- Spot
- Swing

Horizontal vane feedback requires 33-byte status frames.

## Switches

```yaml
switch:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    vanes_3d_auto:
      name: 3D Auto
    outdoor_unit_silent_mode:
      name: Outdoor Unit Silent Mode
```

`outdoor_unit_silent_mode` is an opt-in, model-dependent protocol feature. Configuring the switch adds the Silent Mode `0xC0/0xDD` request to the operation-data cycle. The entity does not assume `OFF` at startup: it publishes only after a valid Silent Mode response has been observed. Writes use the dedicated `DB6=0x80`, `DB9=0x21`, `DB10=0x00|0x01` command form and are confirmed from returned `0xDD/0x80` operation-data feedback rather than optimistic publication. Indoor fan **Quiet** and outdoor-unit **Silent Mode** are separate protocol functions.

3D Auto is supported on the 33-byte frame path. Hardware capture and the complete louver matrix confirmed that 3D Auto is `DB17` bit `0x04` and that horizontal vane plus 3D Auto form one composite `DB16`/`DB17` command domain.

The command builder therefore preserves the companion state:

- a horizontal-only change preserves the current 3D Auto bit;
- a 3D-only change preserves the current horizontal position or swing state;
- a newer cross-field request supersedes a stale pending generation and transmits the latest combined `0x60` intent immediately.

The final hardware matrix confirmed all 80 vertical, horizontal, and 3D Auto combinations with no retry exhaustion and no pending confirmation at completion.

See [`notes/FINDINGS_LOUVERS_3D_AUTO.md`](notes/FINDINGS_LOUVERS_3D_AUTO.md) for the frame positions, complete mapping tables, confirmation rules, and hardware evidence.

## Binary sensors

```yaml
binary_sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    power:
      name: Power
    defrost:
      name: Defrost
    vanes_3d_auto_enabled:
      name: 3D Auto enabled
```

`vanes_3d_auto_enabled` is read-only feedback from decoded 33-byte status frames.

## Text sensors

```yaml
text_sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    error_code:
      name: Error code
    protection_state:
      name: Protection state
```

`protection_state` is decoded from opdata when the unit provides it.

## Sensors

```yaml
sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    room_temperature:
      name: Room temperature
    target_temperature:
      name: Target temperature
    outdoor_temperature:
      name: Outdoor temperature
    return_air_temperature:
      name: Return air temperature
    compressor_frequency:
      name: Compressor frequency
    current_power:
      name: Current
    indoor_unit_fan_speed:
      name: Indoor unit fan speed
    outdoor_unit_fan_speed:
      name: Outdoor unit fan speed
    indoor_unit_total_run_time:
      name: Indoor unit total run time
    compressor_total_run_time:
      name: Compressor total run time
    energy_used:
      name: Energy used
    estimated_power:
      name: Estimated power
    estimated_energy:
      name: Estimated energy
    indoor_unit_thi_r1:
      name: Indoor unit THI R1
    indoor_unit_thi_r2:
      name: Indoor unit THI R2
    indoor_unit_thi_r3:
      name: Indoor unit THI R3
    outdoor_unit_tho_r1:
      name: Outdoor unit THO R1
    outdoor_unit_expansion_valve:
      name: Outdoor unit expansion valve
    outdoor_unit_discharge_pipe:
      name: Outdoor unit discharge pipe
    outdoor_unit_discharge_pipe_super_heat:
      name: Outdoor unit discharge pipe super heat
    protection_state_number:
      name: Protection state number
```

Opdata sensors are validity-gated. If the air conditioner does not provide a field, the entity should remain unavailable instead of publishing a bogus zero.

### Estimated power and energy

Units that expose CT/current opdata but not native energy can publish optional derived values:

```yaml
MhiAcCtrl:
  id: mhi_ac
  power_estimation:
    nominal_voltage: 230.0
    # Optional: voltage_sensor: mains_voltage
    # Optional: voltage_timeout: 5min
    power_factor: 0.95  # fallback PF
    # Optional: power_factor_sensor: mains_power_factor
    # Optional: power_factor_timeout: 5min
    standby_power: 0.0
    max_sample_interval: 5min

sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    estimated_power:
      name: MHI Estimated Power
    estimated_energy:
      name: MHI Estimated Energy
```

`estimated_power` is calculated from decoded CT/current, voltage, and power factor.
By default the estimator uses `nominal_voltage`. Optionally reference any ESPHome
numeric sensor as `power_estimation.voltage_sensor` (for example, a Home Assistant
voltage sensor imported with `platform: homeassistant`). The estimator uses external
voltage **only when a valid reading between 80 and 300 V has arrived within
`voltage_timeout` (default: 5 minutes)**; otherwise it falls back to the configured
nominal voltage. Invalid readings (including NaN/zero) immediately invalidate the
external source. Updates to voltage do not trigger MHI traffic or energy samples;
the latest valid voltage is applied when the next CT sample arrives.

For a Home Assistant voltage source, add the following entry to your existing `sensor:`
list, alongside the `platform: MhiAcCtrl` entry:

```yaml
sensor:
  - platform: homeassistant
    id: mains_voltage
    entity_id: sensor.your_mains_voltage
    internal: true
  - platform: homeassistant
    id: mains_power_factor
    entity_id: sensor.your_mains_power_factor
    internal: true
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    estimated_power:
      name: MHI Estimated Power
    estimated_energy:
      name: MHI Estimated Energy
```

Then configure `voltage_sensor: mains_voltage` and
`power_factor_sensor: mains_power_factor` under `MhiAcCtrl.power_estimation`.
The device must receive regular source state updates. `voltage_timeout` and
`power_factor_timeout` (both default `5min`) independently cause stale sources
to fall back to `nominal_voltage` and configured `power_factor`. Invalid source
values immediately trigger their respective fallback. Valid external power
factor is a **dimensionless ratio in (0, 1]**, e.g. `0.92`, not 92 percent; if
your meter reports percentages, scale by 0.01 before feeding ESPHome. Both
inputs must represent the **air-conditioner's load**: voltage from the same
supply is appropriate, but power factor measured for a shared circuit is not
necessarily the air-conditioner's power factor. Either sensor can be omitted.
An external input update does not trigger MHI traffic or an energy sample;
values are applied on the next CT sample. These changes cannot overcome the
MHI CT measurement's coarse resolution.

`standby_power` is an optional floor used only when decoded status
confirms that the AC is off. No standby consumption is assumed by default.

`estimated_energy` integrates consecutive fresh current samples and skips
intervals longer than `max_sample_interval`, preventing a stale current value
from being integrated across an outage or transport change. It is a boot-session
total and may reset after restart; Home Assistant can handle resets for
`total_increasing` sensors. Native `energy_used` remains separate and should be
preferred when the unit provides it. These values are estimates, not
revenue-grade measurements.

### Optional per-OPDATA polling intervals

By default, all enabled OPDATA fields continue using the original round-robin
scheduler. You can move individual fields onto a faster, time-based schedule:

```yaml
MhiAcCtrl:
  id: mhi_ac
  opdata_poll_intervals:
    current_power: 5s          # CT (amps); also feeds estimated power/energy
    compressor_frequency: 10s  # COMP (Hz)
```

Only named fields change behaviour. All other enabled OPDATA fields continue in
round-robin order; fields with an explicit interval are removed from that
rotation. Explicit intervals also enable those OPDATA requests, even without a
corresponding sensor. Leaving `opdata_poll_intervals` out preserves the existing
wire behaviour and round-robin cadence.

Timed requests occupy otherwise idle eligible background TX frames. Legacy
round-robin slots, user commands, error OPDATA and one-shot confirmation polls
have precedence. These are **minimum intervals between requests**, not a
promise of an AC response at that rate: transport congestion, command
confirmation or unavailable AC data can delay readings. No extra TX frames are
created and the default `tx_background_interval_ms: 250` need not change.

Valid keys correspond to the available OPDATA fields: `mode`,
`target_temperature`, `return_air_temperature`, `indoor_unit_thi_r1`,
`indoor_unit_thi_r2`, `indoor_unit_thi_r3`, `indoor_unit_fan_speed`,
`indoor_unit_total_run_time`, `outdoor_temperature`, `outdoor_unit_tho_r1`,
`compressor_frequency`, `outdoor_unit_discharge_pipe`, `current_power`,
`outdoor_unit_discharge_pipe_super_heat`, `protection_state_number`,
`outdoor_unit_fan_speed`, `defrost`, `compressor_total_run_time`,
`outdoor_unit_expansion_valve`, `energy_used`, and `silent_mode`. Each configured
interval must be between `1s` and `24h`. Avoid configuring many very short intervals:
there is only one OPDATA request slot per eligible TX frame.

Polling CT more often improves how promptly the estimator sees compressor load
changes, but does not improve the CT sensor's own measurement resolution.
`power_estimation.max_sample_interval` is an energy-integration stale-data guard,
not the CT polling rate.

## External room temperature

An external ESPHome temperature sensor can be attached:

```yaml
MhiAcCtrl:
  id: mhi_ac
  external_temperature_sensor: external_room_temp
```

Invalid or unavailable (`NaN`) external sensor states clear the override and return the AC to its indoor-unit temperature sensor. A later valid external temperature automatically restores the override.

For a sensor imported from Home Assistant, the component can also fail safe when the Home Assistant native-API connection disappears:

```yaml
MhiAcCtrl:
  id: mhi_ac
  external_temperature_sensor: external_room_temp
  external_temperature_fallback_on_homeassistant_disconnect: true
  external_temperature_disconnect_grace_period: 60s

sensor:
  - platform: homeassistant
    id: external_room_temp
    entity_id: sensor.room_temperature
```

This option is deliberately disabled by default because `external_temperature_sensor` can also refer to a local ESPHome sensor that does not depend on Home Assistant. When enabled, the component identifies the Home Assistant native-API client. A disconnect starts `external_temperature_disconnect_grace_period`, which defaults to `60s`. If Home Assistant reconnects during that grace period, the current external override is retained. If it remains disconnected until the grace period expires, the component clears the override and returns to the indoor-unit sensor. After fallback, Home Assistant reconnecting does not reuse the cached value; the next valid imported state restores the external override. Other API clients, including ESPHome log viewers, do not keep the external override alive.

An explicit invalid or unavailable (`NaN`) sensor state still falls back immediately and does not wait for the disconnect grace period. The grace period applies specifically to loss of the Home Assistant API connection.

The component does not treat an unchanged temperature as stale. A stable room can legitimately publish the same value for a long period, so a generic value-age timeout would create false fallbacks. `room_temp_timeout` remains the timeout for values supplied through the manual `climate.mhi.set_external_room_temperature` action.

Manual action support also exists:

```yaml
on_...:
  then:
    - climate.mhi.set_external_room_temperature:
        mhi_ac_ctrl_id: mhi_ac
        temperature: 22.5
```

## Vane actions

Automation actions are available for direct vane control:

```yaml
on_...:
  then:
    - climate.mhi.set_vertical_vanes:
        mhi_ac_ctrl_id: mhi_ac
        position: 5
```

```yaml
on_...:
  then:
    - climate.mhi.set_horizontal_vanes:
        mhi_ac_ctrl_id: mhi_ac
        position: 8
```

## Runtime diagnostics

Use DEBUG logging during initial hardware validation and soak testing. At minimum, confirm that valid frames continue to increase, protocol errors remain at zero, commands confirm, and opdata continues to publish.

See [`DIAGNOSTICS.md`](DIAGNOSTICS.md) for:

- Common protocol-health counters
- Driver-specific SPI, RMT, buffering, and TX counters
- Command-confirmation interpretation
- Loop and command-worker measurements
- Soak-test recording requirements
- Troubleshooting workflows

## Command confirmation and duplicate suppression

Commands are staged, transmitted by the selected transport, and confirmed against authoritative decoded MOSI feedback.

Implemented command safety behaviour:

- confirmed decoded state remains authoritative;
- confirmation starts only after actual transport completion;
- duplicate or already-confirmed requests are suppressed;
- confirmation failures use bounded retry and expose timeout/retry counters;
- swing confirmation checks the semantic swing bit and ignores retained fixed-position bits;
- horizontal confirmation validates the requested horizontal state and its preserved 3D companion state;
- 3D Auto confirmation checks `DB17` bit `0x04` independently;
- Outdoor Unit Silent Mode confirms from decoded `0xDD/0x80` operation-data feedback rather than normal status bytes;
- newer horizontal or 3D intent supersedes a stale pending confirmation and coalesces the latest composite state.

This prevents repeated Home Assistant input from generating redundant commands and prevents an obsolete horizontal/3D confirmation generation from leaking into a later request.

## Validation checklist

Before submitting a merge request:

```bash
./scripts/lint.sh fix
./scripts/test.sh
./scripts/compile-tests.sh
```

Hardware and soak-test validation criteria are documented in [`DIAGNOSTICS.md`](DIAGNOSTICS.md#hardware-validation-checklist).

## Troubleshooting

Driver, protocol, command-confirmation, opdata, loop-time, and command-worker troubleshooting has moved to [`DIAGNOSTICS.md`](DIAGNOSTICS.md#troubleshooting).

## Development notes

The code is split into protocol, transport, state, publish, diagnostics, and platform glue:

```text
mhi_checksum.*
mhi_frame_sync.*
mhi_frame_classifier.*
mhi_frame_catalog.*
mhi_status_decoder.*
mhi_opdata_decoder.*
mhi_tx_builder.*
mhi_tx_contract.h
mhi_command_coordinator.*
mhi_command_confirmation.*
mhi_publish_bridge.*
mhi_transport_manager.*
mhi_duplex_transport.*
mhi_fast_gpio_rx_driver.*
mhi_fast_gpio_tx_driver.*
mhi_external_clock_rx_driver.*
mhi_rmt_spi_rx_driver.*
mhi_rmt_cs_spi_transport.*
mhi_null_tx_driver.*
mhi_diag.*
mhi_stats.*
```

Tests live under:

```text
tests/unit/
tests/fixtures/
tests/components/
```

The default ESPHome compile gate uses four representative configurations for the current material transport boundaries. An optional extended gate also compiles the portable RX-only path across ESP32, S2, S3, C2, C3, C5, C6, C61, and S31:

```bash
./scripts/compile-tests.sh compile representative
./scripts/compile-tests.sh compile extended
```

See [`TRANSPORT_COMPILE_MATRIX.md`](TRANSPORT_COMPILE_MATRIX.md) for the target and driver breakdown. Extended success proves source/toolchain compatibility only; hardware timing and transport stability still require physical validation.

Run host tests:

```bash
./scripts/test.sh
```

Run ESPHome compile tests:

```bash
./scripts/compile-tests.sh
```

Run lint:

```bash
./scripts/lint.sh fix
```

## Current design rules

- Keep sensor and opdata fields validity-gated.
- Keep confirmed decoded state authoritative.
- Keep `fast_gpio_rx` as the conservative default and internal recovery transport.
- Treat `rx_driver` as the primary selector and auto-resolve TX for split drivers.
- Preserve explicit `tx_driver` support for valid split configurations and RX-only diagnostics.
- Give full-duplex transports exclusive ownership of RX and TX.
- Keep `rmt_cs_spi` FIFO-backed on ESP32 and ESP32-S3; the supported frame sizes do not require DMA.
- Keep `command_worker` optional and default-off for compatibility, while maintaining full test coverage for the worker-backed lifecycle.
- Use bounded latest-state catalogue slots rather than general status FIFOs.
- Keep protocol, decoder, entity, and transport responsibilities isolated.
- Require returned MOSI state before publishing a requested command as confirmed.

## Project completion and remaining work

The planned architecture and primary feature roadmap are substantially complete:

- protocol, transport, state, publication, and diagnostics are separated;
- 20-byte and 33-byte frame paths are supported;
- FastGPIO, external-clock, split RMT/SPI, and full-duplex RMT-CS SPI transports are implemented;
- DMA has been removed from the full-duplex SPI path;
- command confirmation, retry, duplicate suppression, and latest-intent coalescing are implemented;
- four-speed and three-speed fan profiles are supported;
- vertical vane, horizontal vane, and 3D Auto mappings are hardware-validated;
- Outdoor Unit Silent Mode protocol support is implemented as an opt-in model-dependent feature and still requires wider hardware validation;
- host unit tests, sanitizers, lint checks, and representative cross-chip compile tests are integrated into CI.

Remaining work is incremental:

- validate ESP32-C3 at runtime;
- collect compatibility evidence from additional boards and MHI air-conditioner models;
- document model-specific opdata and any newly discovered protocol fields;
- continue soak testing after material transport or coordinator changes;
- handle bug fixes, ESPHome compatibility updates, and routine maintenance.

## Contributing

Contributions are welcome for bug fixes, documentation, protocol findings, additional hardware validation, and carefully scoped feature work.

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before changing transport, command, worker, state, or publication behaviour. Pull requests should pass:

```bash
./scripts/lint.sh fix
./scripts/test.sh
./scripts/compile-tests.sh
```

Hardware-dependent changes should include the board, ESP chip, air-conditioner model, frame size, selected transport, worker setting, test duration, focused logs, and final diagnostic counters. Use the repository issue forms for bugs, hardware-validation reports, and protocol findings.

## Credits

This project builds on prior MHI reverse-engineering and ESPHome integration work.

- Upstream ESPHome project base: [ginkage/MHI-AC-Ctrl-ESPHome](https://github.com/ginkage/MHI-AC-Ctrl-ESPHome)
- Bus capture and trace reference: [absalom-muc/MHI-AC-Trace](https://github.com/absalom-muc/MHI-AC-Trace)
- FastGPIO inspiration/reference work: [RobertJansen1/MHI-AC-Ctrl-ESPHome esp32_errors branch](https://github.com/RobertJansen1/MHI-AC-Ctrl-ESPHome/tree/esp32_errors)
- RMT-derived chip-select and ESP32 SPI slave approach : [hberntsen/mhi-ac-ctrl-esp32](https://github.com/hberntsen/mhi-ac-ctrl-esp32)
- Outdoor Unit Silent Mode protocol discovery and implementation reference: [hberntsen/mhi-ac-ctrl-esp32 commit 538b341](https://github.com/hberntsen/mhi-ac-ctrl-esp32/commit/538b3412b4e013d6f732f47736f8446a211ec737)
- Original reverse-engineering lineage and MHI protocol work from the wider `MHI-AC-Ctrl` community

## License

See `LICENSE`.

### Operation-data freshness diagnostics

Operation-data values are requested in a rotating cycle, so a previously valid
sensor can remain unchanged even when its individual response has stopped. The
component tracks the last accepted response for every enabled operation-data
request and exposes optional diagnostic entities:

```yaml
MhiAcCtrl:
  id: mhi_ac
  opdata_freshness_timeout: 120s

binary_sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    opdata_fresh:
      name: MHI Operation Data Fresh

sensor:
  - platform: MhiAcCtrl
    mhi_ac_ctrl_id: mhi_ac
    opdata_oldest_age:
      name: MHI Oldest Operation Data Age
    opdata_stale_count:
      name: MHI Stale Operation Data Requests
    opdata_timeout_events:
      name: MHI Operation Data Timeout Events
```

`opdata_fresh` becomes true only after every enabled request has produced an
accepted response and none has exceeded the configured timeout. Timeout events
count transitions into a stale state rather than every polling cycle. Existing
operation-data values are retained; this phase adds observability without
changing the current scheduler or publication behaviour.

## Refactor validation

The modular transport architecture, recovery path, Active Mode, operation-data freshness, and optional estimated-energy features have a consolidated software gate:

```bash
./scripts/release-gate.sh validate
```

Use `./scripts/release-gate.sh compile` for the representative ESP-IDF compile matrix and footprint report. Hardware validation remains a separate requirement for timing-sensitive transports. See [`TRANSPORT_REFACTOR_VALIDATION.md`](TRANSPORT_REFACTOR_VALIDATION.md).
