#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

BUILD_DIR=".test-build"
mkdir -p "${BUILD_DIR}"

CXX="${CXX:-g++}"

SANITIZER_FLAGS=()

if [[ "${SANITIZERS:-0}" == "1" ]]; then
  SANITIZER_FLAGS=(
    -O1
    -g
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
  )
fi

TEST_SOURCES=(
  tests/unit/mhi_unit_test_main.cpp
  tests/unit/test_checksum.cpp
  tests/unit/test_frame_sync.cpp
  tests/unit/test_frame_queue.cpp
  tests/unit/test_duplex_tx_mailbox.cpp
  tests/unit/test_rmt_cs_spi_mode.cpp
  tests/unit/test_transport_wrappers.cpp
  tests/unit/test_rx_runtime.cpp
  tests/unit/test_command_coordinator.cpp
  tests/unit/test_command_coordinator_extended_supersession.cpp
  tests/unit/test_worker_decoded_store.cpp
  tests/unit/test_frame_catalog.cpp
  tests/unit/test_fan_profile.cpp
  tests/unit/test_status_decoder.cpp
  tests/unit/test_swing_restore.cpp
  tests/unit/test_opdata_decoder.cpp
  tests/unit/test_opdata_freshness.cpp
  tests/unit/test_power_estimator.cpp
  tests/unit/test_publish_bridge.cpp
  tests/unit/test_tx_builder.cpp
  tests/unit/test_tx_builder_3d_auto_command_bits.cpp
  tests/unit/test_silent_mode.cpp
  tests/unit/test_command_confirmation.cpp
  tests/unit/test_diag.cpp
  tests/unit/test_fixtures.cpp
)

COMPONENT_SOURCES=(
  components/MhiAcCtrl/mhi_checksum.cpp
  components/MhiAcCtrl/mhi_diag.cpp
  components/MhiAcCtrl/mhi_command.cpp
  components/MhiAcCtrl/mhi_command_confirmation.cpp
  components/MhiAcCtrl/mhi_command_coordinator.cpp
  components/MhiAcCtrl/mhi_frame_sync.cpp
  components/MhiAcCtrl/mhi_frame_catalog.cpp
  components/MhiAcCtrl/mhi_frame_classifier.cpp
  components/MhiAcCtrl/mhi_opdata_decoder.cpp
  components/MhiAcCtrl/mhi_opdata_freshness.cpp
  components/MhiAcCtrl/mhi_opdata_freshness_publisher.cpp
  components/MhiAcCtrl/mhi_power_estimator.cpp
  components/MhiAcCtrl/mhi_publish_bridge.cpp
  components/MhiAcCtrl/mhi_rx_runtime.cpp
  components/MhiAcCtrl/mhi_status_decoder.cpp
  components/MhiAcCtrl/mhi_stats.cpp
  components/MhiAcCtrl/mhi_split_transport.cpp
  components/MhiAcCtrl/mhi_duplex_transport_adapter.cpp
  components/MhiAcCtrl/mhi_transport_manager.cpp
  components/MhiAcCtrl/mhi_transport_diagnostics_publisher.cpp
  components/MhiAcCtrl/mhi_tx_builder.cpp
  components/MhiAcCtrl/mhi_worker_decoded_store.cpp
)

check_sources_exist() {
  local source
  local missing=0

  for source in "${TEST_SOURCES[@]}" "${COMPONENT_SOURCES[@]}"; do
    if [[ ! -f "${source}" ]]; then
      echo "Missing host-test source: ${source}" >&2
      missing=1
    fi
  done

  if [[ "${missing}" -ne 0 ]]; then
    echo "Host-test source manifest is stale; update scripts/test.sh before compiling." >&2
    exit 1
  fi
}

check_sources_exist

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "${SANITIZER_FLAGS[@]}" \
  -Itests/stubs \
  -Icomponents/MhiAcCtrl \
  "${TEST_SOURCES[@]}" \
  "${COMPONENT_SOURCES[@]}" \
  -o "${BUILD_DIR}/mhi_protocol_tests"
"${BUILD_DIR}/mhi_protocol_tests"

python3 tests/unit/test_climate_setpoint_config.py
python3 tests/unit/test_driver_selection.py
python3 tests/unit/test_transport_configuration.py
python3 tests/unit/test_transport_compile_selection.py
python3 tests/unit/test_log_severity_policy.py
python3 tests/unit/test_esphome_component_dependencies.py
python3 tests/unit/test_repository_hygiene.py
