# Command Worker v2 Migration Plan

## Target state

The transport remains responsible for all timing-sensitive bus activity. A single optional `command_worker` owns command preparation and, in a later stage, classified RX processing.

```yaml
MhiAcCtrl:
  rx_driver: rmt_cs_spi
  command_worker: true
```

The old `rx_worker` and `tx_worker` settings are removed.

```text
Home Assistant command
        ↓
CommandCoordinator
        ↓
immutable TX envelope
        ↓
selected transport
        ↓
actual bus transaction
        ↓
TX completion record
        ↓
command confirmation
```

## Non-negotiable boundaries

- The worker never drives MISO or waits on SCK edges.
- The selected transport owns real-time transmission.
- A command is not awaiting confirmation until the transport reports a completed TX transaction.
- ESPHome entities are published only from the main loop.
- Notifications wake a task; command state and frames remain in owned storage.
- Shared command state is protected by one short-duration mutex.

## Stage 1 — first testing point

This stage establishes the command lifecycle without moving RX decoding out of the main loop.

### Changes

- Remove the legacy RX and TX worker configuration and implementation.
- Add `command_worker` and its task settings.
- Add `MhiTxEnvelope` containing an immutable frame, generation, command mask, and intent.
- Add `MhiTxCompletion`, emitted only after a command frame was actually clocked onto the bus.
- Add `MhiCommandCoordinator` to:
  - allocate command generations;
  - stage command frames;
  - start confirmation after TX completion;
  - restore command fields after staging or TX failure without overwriting newer requests.
- Add an atomic command-patch API so one Home Assistant climate action:
  - merges power, mode, target temperature, fan, and swing fields under one lock;
  - produces one worker notification;
  - cannot be split into partial command frames by a concurrently running worker.
- Extend both transport models:
  - split RX/TX drivers report completion after `fast_gpio_tx` returns;
  - `rmt_cs_spi` reports completion after the SPI transaction result is received.
- Keep background/opdata frames out of the command-completion queue.
- Keep RX synchronisation, classification, decoding, and publication in the main loop.
- Preserve `command_worker: false` as the synchronous fallback.
- Preserve the existing 250 ms default background TX interval in both modes.

### First hardware test matrix

Run both configurations against the same firmware and AC:

```yaml
command_worker: false
```

```yaml
command_worker: true
command_worker_start_delay_ms: 0
```

Test at least:

1. Power on and off.
2. Mode changes.
3. Temperature changes.
4. Every configured fan speed.
5. Vertical and horizontal vane commands.
6. Rapid combined changes, such as power, mode, temperature, and fan.
7. Wi-Fi and Home Assistant API reconnects.

### Stage 1 acceptance criteria

- The firmware compiles for every existing compile-test target.
- Existing split-driver configurations still work.
- `rmt_cs_spi` still owns its SPI task and real-time TX path.
- `tx_command_frames` increases only after completed bus transactions.
- `pending_confirmation_mask` remains zero while a command is merely staged.
- Command confirmations and physical AC behaviour match.
- `command_confirmation_timeouts` remains zero during the functional test.
- No transport queue, RMT, SPI, overwrite, or drop regressions appear.
- `command_worker: false` remains behaviourally equivalent to the prior main-loop path.
- A combined climate call is staged as one command mask rather than several worker wake-ups.
- A failed older command never overwrites a newer request for the same field.

## Stage 2 — command lifecycle hardening

Implemented hardening:

- confirmation timeout restores only the still-unconfirmed fields;
- commands are capped at three total transmission attempts;
- a newer value for the same field supersedes the older pending confirmation immediately;
- feedback for an old in-flight value cannot block a newer queued value;
- staged-but-not-transmitted commands produce a separate one-shot warning while continuing to wait for the AC-owned clock;
- 3D Auto confirmation uses the returned 3D Auto feedback bit and does not require the pre-command horizontal-louver context to remain unchanged;
- tests cover partial retry, retry exhaustion, same-field supersession, stale in-flight completion, and staged timeout reporting.

Production hardening now implemented:

- command-worker stack high-water and service-duration reporting;
- notification-batch high-water reporting;
- transport RX queue depth, high-water, and overwrite reporting;
- TX completion queue depth, high-water, and drop reporting;
- completion queues reject overflow instead of overwriting lifecycle events;
- decoded snapshot backlog and unknown-ring high-water reporting;
- ESPHome shutdown stops the worker before releasing transport resources.

## Stage 3 — classified RX in the same worker (decoded snapshot handoff complete)

Do not add a second RX worker. Extend `command_worker` to consume complete RX frames from a bounded transport handoff.

The Stage 3 implementation now:

- drains queue-backed RX drivers from the same worker;
- performs frame synchronisation and checksum validation there;
- classifies valid frames into the bounded latest-slot catalog;
- decodes status and opdata in the worker;
- commits decoded status, command-candidate, opdata, and unknown-frame results to bounded stores;
- wakes the ESPHome main loop after each non-empty decoded batch;
- keeps `fast_gpio_rx` in the main loop because its read path is synchronous and timing-critical.

The main loop continues to:

- apply decoded snapshots to the public state store;
- update generation-aware confirmation state;
- publish climate and sensor state;
- drain completed command outcomes;
- perform ESPHome-facing callbacks.

Repeated status and repeated semantic opdata fields use latest-value replacement. Different opdata fields remain merged in the same pending decoded snapshot. Unknown frames use a small diagnostic-only ring and cannot block status, confirmation, or opdata processing.

## Stage 4 — lifecycle and backpressure hardening

The Stage 4 implementation makes queue pressure and worker resource usage explicit and adds a safe shutdown boundary. No transport timing or command semantics change in this stage.

Acceptance criteria:

- `completion_dropped` remains zero;
- `rx_overwritten` remains zero;
- worker `stack_free_min` retains usable headroom;
- worker `runtime_us` stays well below the 10 ms classified-RX poll period under normal load;
- decoded `pending_high_water` remains bounded;
- OTA/reboot shutdown does not leave the worker or SPI transport active;
- completion ordering remains FIFO and queue overflow never overwrites an earlier completion.

## Stage 5 — validation and promotion

Run the full matrix:

- stable split transport with `command_worker: false`;
- stable split transport with `command_worker: true`;
- `rmt_cs_spi` with `command_worker: false`;
- `rmt_cs_spi` with `command_worker: true`.

Promotion requires a mixed-command test, a DEBUG soak, and a 24–48 hour reduced-logging soak with clean protocol, confirmation, opdata, and transport diagnostics.

## Configuration migration

Previous experimental configuration:

```yaml
rx_worker: false
tx_worker: false
```

New configuration:

```yaml
command_worker: false
```

Enable the first test point explicitly:

```yaml
command_worker: true
```

The setting controls command preparation and coordination. It does not move real-time TX out of the selected transport.
