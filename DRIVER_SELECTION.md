# Driver Selection Guide

This guide explains how to choose and tune the MHI transport drivers. The README contains the short operational recommendation; this document records the technical selection rules and validation criteria.

## Configuration model

`rx_driver` is the primary transport selector.

For split backends, the component automatically selects the normal TX driver. `tx_driver` remains available as an optional compatibility and diagnostic override.

| `rx_driver` | Automatic effective TX | Transport model | Maturity |
|---|---|---|---|
| `fast_gpio_rx` | `fast_gpio_tx` | Split RX/TX | **Stable** |
| `external_clock_rx` | `fast_gpio_tx` | Split RX/TX | **Stable** |
| `rmt_spi_rx` | `fast_gpio_tx` | Split RX/TX | **Stable** |
| `rmt_cs_spi` | Integrated `rmt_cs_spi` TX | Shared full-duplex transport | **In testing** |

The default configuration is equivalent to:

```yaml
rx_driver: fast_gpio_rx
```

which resolves to:

```yaml
rx_driver: fast_gpio_rx
tx_driver: fast_gpio_tx
```

Existing explicit split configurations remain supported.

## Selection rules

### Normal split selection

```yaml
rx_driver: rmt_spi_rx
```

This automatically selects `fast_gpio_tx`.

An explicit equivalent remains valid:

```yaml
rx_driver: rmt_spi_rx
tx_driver: fast_gpio_tx
```

### RX-only diagnostic selection

```yaml
rx_driver: rmt_spi_rx
tx_driver: none
```

Use `tx_driver: none` only when validating receive behaviour without controlling the air conditioner. Climate commands and opdata requests requiring TX will not work.

### Full-duplex selection

```yaml
rx_driver: rmt_cs_spi
```

`rmt_cs_spi` owns RMT, SPI2, SCK, MOSI, MISO, RX buffers, TX buffers, and the SPI owner task. It therefore rejects a separate TX override.

Invalid:

```yaml
rx_driver: rmt_cs_spi
tx_driver: fast_gpio_tx
```

Also invalid:

```yaml
rx_driver: rmt_cs_spi
tx_driver: none
```

### Command-worker compatibility

All transport selections support the same command-facing contract. The selected backend remains responsible for real-time TX:

```yaml
command_worker: false
```

is the synchronous fallback, while:

```yaml
command_worker: true
```

enables the first-stage event-driven command coordinator. For split drivers, completion is reported after `fast_gpio_tx` returns. For `rmt_cs_spi`, completion is reported after the SPI owner task receives the completed transaction result.

The command worker does not drive MISO, wait on SCK edges, or own SPI. Classified RX processing will be added to this same worker later. The legacy `rx_worker` and `tx_worker` settings have been removed.

## Driver summary

### `fast_gpio_rx`

Software GPIO receive path.

Strengths:

- Conservative and established baseline.
- Works with the existing FastGPIO TX path.
- Available on more ESP32 targets than the S3-specific hardware SPI backends.
- Useful fallback when hardware-assisted drivers are unavailable.

Trade-offs:

- CPU-intensive and timing-sensitive.
- More exposed to Wi-Fi, logging, API, publishing, and scheduler load.
- Can contribute to long transport sections in the ESPHome main loop.

Use when:

- Bringing up a new installation.
- Running on hardware without a validated hardware-assisted backend.
- Diagnosing whether a problem is specific to an experimental driver.

### `external_clock_rx`

Software external-clock receive path that samples MOSI from the AC-provided SCK timing and emits signature-anchored frame chunks.

Current maturity: **Stable** on the original ESP32 configuration validated with an M5Stack Atom.

Strengths:

- Reduces RX-side pressure compared with the original synchronous FastGPIO path in tested runs.
- Supports split operation with `fast_gpio_tx`.
- Provides the current validated hardware-assisted direction for the original ESP32/non-S3 target.

Trade-offs:

- TX remains FastGPIO and can remain the dominant loop-time cost.
- Validation is hardware-specific; behaviour must not be assumed across every ESP32 family.

Use when:

- Running the validated original ESP32/M5Stack Atom configuration.
- Testing non-S3 hardware where the S3-only SPI backends are unavailable.
- Comparing software RX strategies using recorded diagnostics.

### `rmt_spi_rx`

ESP32-S3 receive backend using RMT to detect the inter-frame clock gap and the SPI slave peripheral with DMA to capture complete frames.

Strengths:

- Hardware-assisted RX.
- Clean 20-byte and 33-byte complete-frame capture on the tested ESP32-S3.
- Completed a roughly 47.5-hour soak with no checksum failures, signature misses, synchronisation losses, SPI queue errors, or completed-frame drops.
- Considerably lowers RX-side timing pressure.

Trade-offs:

- ESP32-S3 and ESP-IDF only.
- TX remains `fast_gpio_tx`, which can still block the main loop and miss occasional background response windows.
- Requires an inferred internal chip-select boundary because the MHI bus has no physical CS line.

Recommended ESP32-S3 split configuration:

```yaml
rx_driver: rmt_spi_rx
rmt_spi_frame_gap_us: 1000
command_worker: false
```

### `rmt_cs_spi`

ESP32-S3 full-duplex backend using RMT-derived internal chip select and a mode-3, LSB-first SPI slave transaction for simultaneous RX and TX.

Strengths:

- Hardware owns bit-level RX and TX timing.
- Removes FastGPIO TX from the normal transaction path.
- Uses DMA-aligned buffers for 20-byte and 33-byte frames.
- Initial hardware results show clean frame capture, clean TX completion, command operation, and low buffering pressure.

Trade-offs:

- Experimental and currently under long-duration soak testing.
- ESP32-S3 and ESP-IDF only.
- Owns the complete bus transport and cannot be mixed with another TX driver.
- The new command worker is optional and does not change SPI ownership or bus timing.

Configuration:

```yaml
rx_driver: rmt_cs_spi
rmt_spi_frame_gap_us: 1000
command_worker: false
```

### `fast_gpio_tx`

Software GPIO transmit path used by all split RX drivers.

Strengths:

- Established command path.
- Compatible with FastGPIO, external-clock, and RMT/SPI RX backends.
- Command confirmation has remained reliable in the validated S3 tests.

Trade-offs:

- Follows the externally supplied clock in software.
- Can create 40-60 ms class main-loop transport stalls.
- Long-duration testing with `rmt_spi_rx` showed occasional background TX failures even though tested commands still confirmed.

### `none`

Diagnostic RX-only TX selection.

Use only to isolate receive behaviour. It is not a usable Home Assistant control configuration.

## Hardware guidance

Keep one consolidated row per ESP chip version. Add boards or modules to the `Validated hardware` cell as testing expands.

| ESP chip | Validated hardware | Recommended RX selection | Effective TX | Maturity | Technical position |
|---|---|---|---|---|---|
| ESP32 | M5Stack Atom (original ESP32) | `external_clock_rx` | `fast_gpio_tx` | **Stable** | Current running non-S3 configuration. `fast_gpio_rx` remains the conservative fallback. |
| ESP32-S3 | Current ESP32-S3 test unit; board/module model still to be recorded | `rmt_spi_rx`; `rmt_cs_spi` for active testing | `fast_gpio_tx`; integrated TX for `rmt_cs_spi` | **Stable** for `rmt_spi_rx`; **In testing** for `rmt_cs_spi` | Strongest runtime coverage. The split path has completed extended soak testing; the duplex path is in soak testing. |
| ESP32-C3 | No runtime-validated board yet | `fast_gpio_rx` | `fast_gpio_tx` | **In development** | Compile coverage only. Single-core runtime reliability has not been established. |

Do not infer compatibility from the general ESP32 family name. GPIO registers, core count, RMT revisions, SPI routing, DMA behaviour, and available peripherals differ by target.

## Technical tuning

### `frame_size`

```yaml
frame_size: 20
```

Use for older or shorter-frame units.

```yaml
frame_size: 33
```

Use for units providing the extended frame, including horizontal vane and 3D Auto feedback.

Incorrect frame size normally presents as repeated invalid frames, checksum failures, or missing extended features.

### `rmt_spi_frame_gap_us`

Default:

```yaml
rmt_spi_frame_gap_us: 1000
```

Valid range: 500-5000 microseconds.

This value must be:

- Greater than the normal inter-byte idle period.
- Lower than the inter-frame idle period.

The tested bus has an approximately 250 microsecond inter-byte pause and a roughly 40 millisecond inter-frame pause, making 1000 microseconds a practical starting point.

Only tune this value when diagnostics show regular invalid transaction lengths, missed boundaries, or re-arm errors. Do not use it to mask checksum or signature problems caused by the wrong frame size or pin mapping.

### `frame_start_idle_ms`

Default:

```yaml
frame_start_idle_ms: 10
```

This setting applies to the software FastGPIO timing path. Leave it at the default unless oscilloscope or logic-analyser data shows a materially different frame-start idle requirement.

### `tx_background_interval_ms`

Default for both synchronous and command-worker modes:

```yaml
tx_background_interval_ms: 250
```

This controls background TX attempts. Command frames bypass the normal background interval.

Increase it when background requests create unnecessary TX pressure. Reducing it increases bus activity and can expose FastGPIO TX timing limitations.

### Command worker

Default:

```yaml
command_worker: false
```

First-stage test configuration:

```yaml
command_worker: true
```

The worker prepares command envelopes and coordinates staging, actual TX completion, and semantic confirmation. Background and command frames remain transmitted by the selected transport. The first-stage implementation leaves RX decoding in the main loop.

Judge the command-worker path by functional outcomes:

- Commands are not marked pending confirmation before actual TX completion.
- Commands confirm and `command_confirmation_timeouts` remains zero.
- Opdata continues to publish.
- Home Assistant state matches the physical unit.
- Transport overwrite, queue, and drop counters remain clean.
- `command_worker: false` remains a reliable fallback.

The staged rollout and classified-RX integration are documented in [`COMMAND_WORKER_V2_PLAN.md`](COMMAND_WORKER_V2_PLAN.md). When `command_worker: true`, queue-backed RX drivers (`external_clock_rx`, `rmt_spi_rx`, and `rmt_cs_spi`) are drained and classified by that worker; `fast_gpio_rx` remains in the main loop.

## Diagnostics

Runtime counters and health interpretation are documented separately in [`DIAGNOSTICS.md`](DIAGNOSTICS.md).

Use that guide for:

- Common protocol-health counters
- `rmt_spi_rx` and `rmt_cs_spi` transport counters
- Command-confirmation and opdata checks
- Loop and command-worker measurements
- Soak-test evidence and troubleshooting

## Driver maturity statuses

Use only these three labels in the README and driver-selection tables:

| Status | Meaning |
|---|---|
| **Stable** | Hardware validated and suitable for normal use on the listed hardware. Little to no transport-level change is expected; changes should normally be fixes, compatibility additions, or diagnostics. |
| **In testing** | Functional on hardware and undergoing soak, compatibility, or regression testing. Configuration or internals may still change before promotion to Stable. |
| **In development** | Incomplete, compile-only, or not yet validated sufficiently for normal installations. |

A driver can have different maturity on different ESP chips. The status must therefore be tied to the hardware row, not inferred globally from the driver name.

## Adding a new hardware result

Record at least:

```text
ESP chip and revision
board/module
ESPHome version
ESP-IDF version
AC model
frame size
SCK/MOSI/MISO pins
RX selection and effective TX
command-worker settings
tuning overrides
test duration
commands tested
opdata behaviour
end-of-test diagnostics
known failures or limitations
```

A hardware/driver combination should not be promoted from **In development** to **In testing** without clean runtime logs and successful command operation. Promotion to **Stable** requires extended runtime evidence and no planned material transport changes.

## Related findings

- [`notes/FINDINGS_RMT_SPI_RX.md`](notes/FINDINGS_RMT_SPI_RX.md)
- [`notes/FINDINGS_fastGpio&ExternalClock.md`](notes/FINDINGS_fastGpio&ExternalClock.md)
