# Diagnostics and Hardware Validation

This guide explains the MHI runtime diagnostics, how to interpret transport health, and what evidence is required before a driver or hardware combination is promoted in the compatibility tables.

The driver-selection rules and hardware recommendations are documented in [`DRIVER_SELECTION.md`](DRIVER_SELECTION.md).

## Logging level

Use DEBUG logging for initial bring-up, command testing, and the first part of a soak test:

```yaml
logger:
  level: DEBUG
```

Once the configuration is behaving correctly, use WARN or ERROR for the longer soak. This reduces logging load and provides a more representative runtime result.

Do not treat a clean startup log as proof of stability. Review diagnostics after commands, reconnects, opdata polling, and sustained runtime.

## Common protocol health

A healthy run should show:

```text
valid_frames increases continuously
invalid_frames = 0
checksum_failures = 0
signature_misses = 0
sync_losses = 0
dropped_bytes = 0
command_confirmation_timeouts = 0
```

The following fields are the main protocol indicators.

| Counter | Meaning | Healthy expectation |
|---|---|---|
| `rx_bytes` | Total bytes received by the transport | Increases while the MHI bus is active |
| `rx_chunks` | RX chunks delivered to frame processing | Increases with traffic |
| `candidate_frames` | Potential frames evaluated | Tracks received frame activity |
| `valid_frames` | Frames accepted after structure and checksum validation | Increases continuously |
| `invalid_frames` | Candidate frames rejected | Zero during normal runtime |
| `checksum_failures` | Frames rejected because the checksum was wrong | Zero |
| `signature_misses` | Data discarded while searching for a valid frame signature | Zero on complete-frame hardware transports; occasional recovery activity may occur on software paths |
| `sync_losses` | Loss of frame synchronisation | Zero |
| `dropped_bytes` | RX bytes discarded during recovery or overflow | Zero |
| `last_valid_frame_age_ms` | Time since the last accepted frame | Should remain close to the normal bus cadence |
| `last_rx_byte_age_ms` | Time since the last received byte | Should remain close to the normal bus cadence |

A single startup anomaly can be non-material if the counters stop increasing. A counter that continues increasing indicates an active fault.

## TX and command health

The common TX counters show whether frames are being prepared and transmitted.

| Counter | Meaning | Healthy expectation |
|---|---|---|
| `tx_frames` | Total frames successfully handed to the TX backend | Increases with background and command traffic |
| `tx_failures` | General TX failures | Zero, or fully explained by a known startup event |
| `tx_command_frames` | Command-bearing frames transmitted | Increases when Home Assistant commands are issued |
| `tx_command_failures` | Failed command-frame attempts | Zero |
| `unsupported_commands` | Commands the active transport or protocol path could not represent | Zero during normal use |
| `last_tx_command_mask` | Bitmask of the last command fields transmitted | Used when tracing a specific command |
| `last_tx_command_age_ms` | Time since the last command transmission | Informational |

A background TX failure does not automatically mean a user command failed. Command success must be judged using command-confirmation counters and actual AC feedback.

## Command confirmation

Commands are staged, transmitted, and then confirmed against decoded MOSI feedback from the air conditioner.

Monitor:

```text
command_confirmations
command_confirmation_timeouts
command_retries
command_retry_exhaustions
command_staged_timeouts
pending_confirmation_mask
last_confirmed_mask
last_timeout_mask
last_retry_mask
last_exhausted_mask
last_staged_timeout_mask
```

Healthy behaviour:

- `command_confirmations` increases after confirmable commands.
- `pending_confirmation_mask` returns to zero after the AC reports the requested state.
- Home Assistant settles to the decoded AC state.
- `command_confirmation_timeouts` normally remains zero, but a timeout may now trigger a bounded retry of only the remaining fields.

Runtime logs distinguish:

```text
command: confirmation timeout ... retry=...
command: confirmation exhausted after 3 attempts ...
command: staged but not transmitted ... waiting for AC bus clock
command: superseded pending confirmation ...
```

A retry warning is not the same as final command failure. An exhausted command, repeated retry cycle, or persistent non-zero pending mask requires investigation.

A command test is not successful merely because a TX frame was sent. The physical unit must change and the returned status must confirm the change.

When testing, cover at least:

- Power on and off
- Each supported climate mode
- Several temperature changes
- Every exposed fan level
- Vertical vane positions and swing
- Horizontal vane positions when supported
- 3D Auto when supported
- Commands issued while the unit is off
- Rapid replacement of pending commands

## Frame catalogue and opdata

The frame catalogue separates repeated status traffic, extended status, operation data, and unknown frames.

Monitor:

| Counter | Meaning | Healthy expectation |
|---|---|---|
| `catalog ingested` | Valid frames accepted by the catalogue | Tracks `valid_frames` closely |
| `status` | Standard status frames classified | Depends on the AC and frame layout |
| `extended` | Extended status frames classified | Expected on 33-byte units |
| `opdata` | Operation-data responses classified | Increases when opdata polling is active |
| `unknown` | Valid frames that were not classified | Zero or explained by an unsupported frame type |
| `overwritten` | Latest-value catalogue entries replaced before publication | Normally zero; sustained growth requires review |
| `opdata_slots_full` | Opdata could not be stored because the catalogue was full | Zero |
| `command_candidates` | Frames considered for command confirmation | Increases when relevant feedback is received |

Opdata health must also be verified functionally. Sensors should continue updating across the entire run. Clean frame counters do not compensate for missing opdata publications.

Some opdata fields are model-dependent. A sensor that remains unavailable because the AC never returns that field is not necessarily a transport problem.

## TX scheduling and priority

The TX priority diagnostics distinguish command traffic from routine background requests.

Monitor:

```text
tx_priority command_attempts
tx_priority background_attempts
tx_priority background_failures
interval_deferrals
confirmation_deferrals
```

Expected behaviour:

- Commands bypass the normal background interval.
- Background traffic waits while a command confirmation is pending.
- `confirmation_deferrals` may increase during a command and should stop once the command is confirmed or times out.
- Repeated `background_failures` require investigation, especially if opdata stops updating.

## Main-loop timing

The component reports total loop timing and timing by section.

Example fields:

```text
loop_us last / average / maximum
over_budget
transport
TX
RX
publish
command
```

Use these measurements to locate processing pressure, not as the sole definition of health.

A command worker or hardware backend can improve `loop_us` while introducing lost confirmations, stale data, or missing opdata. Functional behaviour remains the primary acceptance criterion.

For split FastGPIO configurations, long transport or TX sections may occur because software must follow the externally supplied MHI clock. Hardware-assisted backends should materially reduce this pressure.

## Command-worker diagnostics

The first-stage command worker is experimental and disabled by default. It prepares immutable command envelopes and coordinates staging, actual TX completion, and semantic confirmation. Real-time TX remains owned by the selected transport.

Monitor:

```text
command_worker enabled / running / classified_rx
command_worker wakes
command_worker service_runs
command_worker idle_polls
command_worker frames_staged
command_worker completions
command_worker rx_polls
command_worker rx_batches
command_worker rx_chunks
command_worker rx_frames
command_worker rx_max_batch
```

Interpretation:

| Counter | Meaning | Healthy expectation |
|---|---|---|
| `enabled` | Configured worker state | Matches YAML |
| `running` | FreeRTOS task is active | `YES` when enabled |
| `classified_rx` | Worker owns RX draining/synchronisation/classification | `YES` for queue-backed RX drivers; `NO` for `fast_gpio_rx` |
| `wakes` | Explicit notifications consumed by the worker | Increases with command changes and completed command transactions |
| `service_runs` | Combined command/RX service passes | Increases while the worker is active |
| `idle_polls` | Timed polls without an explicit notification | May increase steadily; should not correlate with drops or regressions |
| `frames_staged` | Frames accepted by the selected transport | Increases with commands and background requests |
| `completions` | Command frames reported complete after a real bus transaction | Increases only for command-bearing TX envelopes |
| `rx_polls` | Worker RX polling passes | Increases only when `classified_rx=YES` |
| `rx_batches` | Polls that produced at least one valid frame | Should increase continuously on an active bus |
| `rx_chunks` | Transport chunks drained by the worker | Tracks queue/DMA handoff activity |
| `rx_frames` | Valid frames synchronised and catalogued by the worker | Should broadly track common `valid_frames` |
| `rx_max_batch` | Largest number of valid frames processed in one poll | Normally small; sustained growth indicates worker starvation |
| `worker_decode status/extended` | Decoded status writes and latest-value overwrites | Overwrites are expected when repeated status arrives before main-loop apply |
| `worker_decode candidates` | Generation-sensitive command confirmation snapshots | Should increase while commands are awaiting confirmation |
| `worker_decode opdata_merges` | Decoded opdata frames merged into the pending snapshot | Should track active opdata traffic |
| `worker_decode opdata_field_overwrites` | Repeated semantic opdata fields replaced before main-loop apply | Acceptable when bounded; different fields remain preserved |
| `worker_decode unknown` | Bounded unknown-frame ring writes/overwrites | Overwrites are diagnostic only and must not affect status or opdata |
| `worker_decode publish_batches` | Main-loop decoded snapshot batches applied | Should increase while the bus is active |

A valid first-stage test must demonstrate all of the following:

- The configured worker starts on the intended core.
- A staged command does not create a pending confirmation until `completions` increases.
- Commands continue to confirm.
- `command_confirmation_timeouts` remains zero.
- Opdata continues to publish.
- Home Assistant state remains synchronised with the physical unit.
- Transport overwrite, queue, and drop counters remain clean.
- `command_worker: false` remains a working synchronous fallback.

For queue-backed RX drivers, RX draining, synchronisation, classification, and protocol decoding run in the same worker. The main loop consumes bounded decoded snapshots and remains the only context that mutates published state or calls ESPHome publication APIs. `fast_gpio_rx` remains entirely main-loop driven.

## `rmt_spi_rx` diagnostics

The split hardware-assisted RX path uses RMT-derived frame boundaries and the SPI slave peripheral for receive capture. TX remains `fast_gpio_tx`.

Monitor:

```text
invalid transaction lengths
SPI result errors
SPI queue errors
RMT re-arm errors
completed-frame overwrites
completed-frame drops
maximum buffered frames
```

Healthy expectation:

- Invalid transaction lengths remain zero after startup.
- SPI result and queue errors remain zero.
- RMT re-arm errors remain zero.
- Completed-frame overwrites and drops remain zero.
- Maximum buffering remains low and bounded.

If RX remains clean but background TX fails or the main loop stalls, investigate the `fast_gpio_tx` side separately.

## `rmt_cs_spi` diagnostics

The full-duplex backend uses the same SPI transaction for RX and TX and owns the full transport.

Example runtime line:

```text
boundaries=602 completed=601 tx_completed=117 tx_failures=0
frame20=0 frame33=601 invalid_len=1 result_errors=0
queue_errors=0 rmt_rearm_errors=0 buffered_frames=0
max_buffered=1 rx_overwritten=0 tx_overwritten=0 dropped=0
task_running=YES
```

Monitor:

| Counter | Meaning | Healthy expectation |
|---|---|---|
| `boundaries` | Frame gaps detected by RMT | Increases with bus traffic |
| `completed` | SPI transactions returned as completed | Closely tracks boundaries after startup |
| `tx_completed` | Transactions that carried prepared TX data | Increases with background and command traffic |
| `tx_failures` | TX-side transaction failures | Zero |
| `frame20` | Completed 20-byte transactions | Matches the configured frame type |
| `frame33` | Completed 33-byte transactions | Matches the configured frame type |
| `invalid_len` | Transactions with an unexpected bit length | Zero after startup |
| `result_errors` | Errors retrieving completed SPI transactions | Zero |
| `queue_errors` | Errors queuing SPI transactions | Zero |
| `rmt_rearm_errors` | Failures restarting RMT boundary detection | Zero |
| `buffered_frames` | Completed RX frames waiting for the main path | Normally zero or low |
| `max_buffered` | High-water mark for completed-frame buffering | Low and bounded |
| `rx_overwritten` | Completed RX frames replaced before consumption | Zero |
| `tx_overwritten` | Pending TX snapshots replaced before use | Zero during normal command operation; investigate sustained growth |
| `dropped` | Frames discarded by the transport | Zero |
| `task_running` | State of the SPI owner task | `YES` |

One initial `invalid_len` can occur while attaching to an already active bus. It is acceptable only when the counter remains fixed and all subsequent traffic is clean.

## Hardware validation checklist

Before submitting a merge request:

```bash
./scripts/lint.sh fix
./scripts/test.sh
./scripts/compile-tests.sh
```

For initial hardware validation, record:

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
```

Then verify:

```text
valid_frames climbs
invalid_frames = 0
checksum_failures = 0
signature_misses = 0
sync_losses = 0
dropped_bytes = 0
commands confirm
command_confirmation_timeouts = 0
opdata sensors continue to publish
Home Assistant state matches confirmed AC feedback
driver-specific queue, overwrite, and drop counters remain zero
```

A full command test should be completed before starting a long soak.

## Soak-test evidence

A useful soak record should include:

```text
start and end time
total duration
logging level
Wi-Fi and API reconnect events
commands issued during the run
opdata fields observed
end-of-test common diagnostics
end-of-test driver-specific diagnostics
crashes, watchdog resets, or safe-mode boots
known anomalies and whether their counters continued increasing
```

Recommended sequence:

1. Initial DEBUG validation with command testing.
2. Several hours at DEBUG while reviewing detailed counters.
3. A 24- to 48-hour soak at WARN or ERROR.
4. Final DEBUG reconnect to capture the complete end-of-test diagnostics.

Promotion guidance:

- **In development** to **In testing** requires clean runtime logs and successful command operation.
- **In testing** to **Stable** requires extended runtime evidence and no planned material transport changes.

## Troubleshooting

### No valid frames

Check, in order:

1. SCK, MOSI, and MISO GPIO mapping.
2. Ground reference and electrical connection.
3. `frame_size`.
4. Selected RX backend and chip support.
5. Whether SCK activity is visible with a logic analyser.

Do not tune frame-gap values before confirming the pins and frame size.

### Repeated checksum or signature failures

Likely causes:

- Wrong `frame_size`
- Incorrect GPIO mapping
- Timing instability on a software backend
- Electrical noise or level problems
- Incorrect frame-gap boundary detection

Compare against the conservative `fast_gpio_rx` path to separate protocol and wiring problems from an experimental backend issue.

### Repeated invalid transaction lengths

For RMT/SPI backends:

- Confirm `rmt_spi_frame_gap_us` is greater than the inter-byte gap and lower than the inter-frame gap.
- Confirm the configured frame size matches the unit.
- Check whether the counter increases only once during startup or continuously.
- Review RMT re-arm and SPI queue/result errors.

### Commands transmit but do not confirm

Check:

- `tx_command_frames`
- `tx_command_failures`
- `pending_confirmation_mask`
- `command_confirmation_timeouts`
- `last_confirmed_mask`
- Physical AC behaviour

A clean TX count without returned confirmation is a failed command path.

### Opdata stops updating

Check:

- `opdata` catalogue count
- `opdata_slots_full`
- Catalogue overwrites
- Background TX attempts and failures
- Confirmation deferrals
- Command-worker state

If status frames remain healthy but opdata stops, investigate TX request scheduling and catalogue handling rather than RX synchronisation alone.

### Long ESPHome loop warnings

Synchronous FastGPIO RX or TX can block the ESPHome loop while following the external MHI frame cadence.

Identify whether the time is concentrated in `transport`, `TX`, `RX`, or `publish`. Clean protocol counters are necessary but not sufficient; commands and opdata must remain functional.

### Command-worker mode regresses behaviour

Disable the experimental path:

```yaml
command_worker: false
```

Compare the same command sequence against the synchronous fallback. The worker path is not successful unless actual TX completion, command confirmation, opdata flow, and Home Assistant state remain correct. Lower loop timing alone is not an acceptance result.

### Sensor remains unavailable

Many opdata fields are model-dependent. A sensor only publishes after the AC returns a valid response for that field. Unavailable is preferred over a fabricated zero.

### Fan or vane selection bounces back

Confirmed AC feedback is authoritative. The requested state will be replaced by the decoded state if the AC rejects, clamps, or remaps the command.

Review command masks and confirmation diagnostics to determine whether the AC accepted the selection.

## Reporting a hardware result

Include the hardware-validation record and both the common and driver-specific end-of-test diagnostic lines. State whether `command_worker` was enabled and identify every non-default tuning value.

The hardware compatibility table should only be updated after the evidence is repeatable and the maturity status is clear.
