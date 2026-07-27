# MHI-AC-Ctrl-ESPHome-Redux

ESPHome external component for controlling Mitsubishi Heavy Industries air conditioners over the MHI SPI-style bus using ESP32-class hardware.

This Redux version is an ESP-IDF-focused rewrite/refactor of the existing MHI-AC-Ctrl ESPHome work. It keeps the ESPHome/Home Assistant integration model, but splits the protocol, transport, state, publishing, diagnostics, command confirmation, and transport-driver paths so changes can be made in smaller, safer pieces.

This is not a clean-room protocol project. It builds on the original community MHI-AC-Ctrl work, upstream ESPHome component behaviour, and public MHI trace/capture knowledge.

## Current status

The currently validated runtime targets are the original **ESP32** and **ESP32-S3**, both using ESP-IDF. Keep both workers disabled unless specifically testing worker behaviour.

- `fast_gpio_rx` with `fast_gpio_tx` remains the conservative stable baseline.
- `external_clock_rx` with `fast_gpio_tx` is the stable non-S3 path currently running on an M5Stack Atom based on the original ESP32.
- `rmt_spi_rx` with `fast_gpio_tx` is the stable hardware-assisted split path on ESP32-S3. It completed a roughly 47.5-hour soak with clean RX protocol health.
- `rmt_cs_spi` is the full-duplex ESP32-S3 path currently in testing. Initial hardware results are clean and longer soak testing is in progress.
- ESP32-C3 remains in development with compile coverage only; runtime operation is not validated.

Implemented functionality includes 20-byte and 33-byte frames, command confirmation, climate control, configurable fan profiles, vertical and horizontal vanes, 3D Auto, common status sensors, opdata sensors, and transport diagnostics.

## Driver selection

`rx_driver` is the primary selection. For split transports, TX is selected automatically. Existing configurations may still specify `tx_driver` explicitly.

### Driver combinations

| RX selection | Effective TX | Validation status |
|---|---|---|
| `fast_gpio_rx` | `fast_gpio_tx` | **Stable** |
| `external_clock_rx` | `fast_gpio_tx` | **Stable** |
| `rmt_spi_rx` | `fast_gpio_tx` | **Stable** |
| `rmt_cs_spi` | Integrated full-duplex TX | **In testing** |

`Stable` means the driver combination has completed hardware validation and little to no transport-level change is expected. `In testing` means the implementation is functional but still undergoing soak or compatibility testing. `In development` means it is not ready for normal use.

Workers were disabled for the stable and in-testing configurations above.

### Hardware driver guide

| ESP chip | Validated hardware | Recommended RX selection | Effective TX | Status | Notes |
|---|---|---|---|---|---|
| ESP32 | M5Stack Atom (original ESP32) | `external_clock_rx` | `fast_gpio_tx` | **Stable** | Current running non-S3 configuration. Use `fast_gpio_rx` as the conservative fallback. |
| ESP32-S3 | M5Stack Atom S3 current ESP32-S3 test unit | `rmt_spi_rx`; `rmt_cs_spi` for active testing | `fast_gpio_tx`; integrated TX for `rmt_cs_spi` | **Stable** for `rmt_spi_rx`; **In testing** for `rmt_cs_spi` | `rmt_spi_rx` has completed extended soak testing. `rmt_cs_spi` is in the current longer soak. |
| ESP32-C3 | No runtime-validated board yet | `fast_gpio_rx` | `fast_gpio_tx` | **In development** | Compile coverage only. Single-core runtime behaviour has not been validated. |

Add tested boards or modules to the matching chip row as results become available. Do not add a new row for every board; each ESP chip version should have one consolidated row.

Minimal stable configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: fast_gpio_rx
```

Preferred split ESP32-S3 configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: rmt_spi_rx
  rmt_spi_frame_gap_us: 1000
  rx_worker: false
  tx_worker: false
```

Experimental full-duplex ESP32-S3 configuration:

```yaml
MhiAcCtrl:
  id: mhi_ac
  frame_size: 33
  sck_pin: 8
  mosi_pin: 38
  miso_pin: 39
  rx_driver: rmt_cs_spi
  rmt_spi_frame_gap_us: 1000
  rx_worker: false
  tx_worker: false
```

`rmt_cs_spi` owns RX and TX, so it rejects any `tx_driver` override. For split drivers, the previous explicit configuration remains valid:

```yaml
rx_driver: rmt_spi_rx
tx_driver: fast_gpio_tx
```

See [`DRIVER_SELECTION.md`](DRIVER_SELECTION.md) for backend design, hardware constraints, tuning options, invalid combinations, and the process for adding new hardware validation results. See [`DIAGNOSTICS.md`](DIAGNOSTICS.md) for runtime counters, health interpretation, soak-test evidence, and troubleshooting.

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

## Minimal component configuration

Recommended FastGPIO configuration:

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

Optional fields:

```text
rx_driver
tx_driver
rx_worker
tx_worker
rx_worker_start_delay_ms
rx_worker_stack_size
rx_worker_priority
rx_worker_core_id
tx_worker_start_delay_ms
tx_worker_stack_size
tx_worker_priority
tx_worker_core_id
tx_background_interval_ms
frame_start_idle_ms
rmt_spi_frame_gap_us
room_temp_timeout
external_temperature_sensor
fan_profile
```

## Worker mode

Workers are experimental and disabled by default:

```yaml
rx_worker: false
tx_worker: false
```

All driver combinations listed as validated in this README were tested with workers disabled. `rmt_cs_spi` currently rejects worker enablement. Do not enable workers for a normal installation; worker redesign and validation will resume after the full-duplex transport soak is complete.

See [`DRIVER_SELECTION.md`](DRIVER_SELECTION.md#workers) for the technical background and functional validation criteria.

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
    visual_temperature_step: 1
```

Climate state is published from confirmed decoded MHI state.

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

Redux defaults to the four-speed profile. No `fan_profile` setting is required for normal use:

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
```

3D Auto command state is confirmed from decoded feedback. The switch should settle to the state reported by the AC.

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

## External room temperature

An external ESPHome temperature sensor can be attached:

```yaml
MhiAcCtrl:
  id: mhi_ac
  external_temperature_sensor: external_room_temp
```

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
- Loop and worker measurements
- Soak-test recording requirements
- Troubleshooting workflows

## Command confirmation and duplicate suppression

Commands are staged and then confirmed against decoded MOSI feedback.

Implemented command safety behaviour:

- Confirmed decoded state wins over requested state.
- Duplicate confirmable commands are suppressed while the same command is pending.
- Command timeouts are counted in diagnostics.

This prevents rapid Home Assistant/UI input changes from repeatedly restaging the same pending command.

## Validation checklist

Before submitting a merge request:

```bash
./scripts/lint.sh fix
./scripts/test.sh
./scripts/compile-tests.sh
```

Hardware and soak-test validation criteria are documented in [`DIAGNOSTICS.md`](DIAGNOSTICS.md#hardware-validation-checklist).

## Troubleshooting

Driver, protocol, command-confirmation, opdata, loop-time, and worker troubleshooting has moved to [`DIAGNOSTICS.md`](DIAGNOSTICS.md#troubleshooting).

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
- Keep `fast_gpio_rx` as the conservative default and fallback.
- Treat `rx_driver` as the primary selector and auto-resolve TX for split drivers.
- Preserve explicit `tx_driver` support for existing split configurations and RX-only diagnostics.
- Give full-duplex transports exclusive ownership of RX and TX.
- Keep workers opt-in, experimental, and disabled for validated transport configurations.
- Use latest-state catalogue slots instead of general status FIFOs.
- Keep transport changes separate from protocol, decoder, and sensor-parity changes.

## Roadmap

- Complete the long-duration ESP32-S3 soak for `rmt_cs_spi`.
- Compare full-duplex stability, command confirmation, opdata flow, and loop timing against `rmt_spi_rx` plus `fast_gpio_tx`.
- Keep the stable default unchanged until the full-duplex path completes hardware validation.
- Reopen the worker path as an event-driven design only after the transport result is established.
- Investigate opdata catalogue slot pressure and rejected keys independently of transport work.
- Add hardware rows to the compatibility table only after compile, command, opdata, and soak evidence is available.


## Credits

This project builds on prior MHI reverse-engineering and ESPHome integration work.

- Upstream ESPHome project base: [ginkage/MHI-AC-Ctrl-ESPHome](https://github.com/ginkage/MHI-AC-Ctrl-ESPHome)
- Bus capture and trace reference: [absalom-muc/MHI-AC-Trace](https://github.com/absalom-muc/MHI-AC-Trace)
- FastGPIO inspiration/reference work: [RobertJansen1/MHI-AC-Ctrl-ESPHome esp32_errors branch](https://github.com/RobertJansen1/MHI-AC-Ctrl-ESPHome/tree/esp32_errors)
- Original reverse-engineering lineage and MHI protocol work from the wider `MHI-AC-Ctrl` community

## License

See `LICENSE`.
