#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

BUILD_DIR=".test-build"
mkdir -p "${BUILD_DIR}"

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Itests/stubs \
  -Icomponents/MhiAcCtrl \
  tests/unit/mhi_unit_test_main.cpp \
  tests/unit/test_checksum.cpp \
  tests/unit/test_frame_sync.cpp \
  tests/unit/test_frame_queue.cpp \
  tests/unit/test_duplex_tx_mailbox.cpp \
  tests/unit/test_command_coordinator.cpp \
  tests/unit/test_frame_catalog.cpp \
  tests/unit/test_fan_profile.cpp \
  tests/unit/test_status_decoder.cpp \
  tests/unit/test_opdata_decoder.cpp \
  tests/unit/test_publish_bridge.cpp \
  tests/unit/test_tx_builder.cpp \
  tests/unit/test_tx_builder_3d_auto_command_bits.cpp \
  tests/unit/test_command_confirmation.cpp \
  tests/unit/test_diag.cpp \
  tests/unit/test_fixtures.cpp \
  components/MhiAcCtrl/mhi_checksum.cpp \
  components/MhiAcCtrl/mhi_diag.cpp \
  components/MhiAcCtrl/mhi_command.cpp \
  components/MhiAcCtrl/mhi_command_confirmation.cpp \
  components/MhiAcCtrl/mhi_command_coordinator.cpp \
  components/MhiAcCtrl/mhi_frame_sync.cpp \
  components/MhiAcCtrl/mhi_frame_catalog.cpp \
  components/MhiAcCtrl/mhi_frame_classifier.cpp \
  components/MhiAcCtrl/mhi_opdata_decoder.cpp \
  components/MhiAcCtrl/mhi_publish_bridge.cpp \
  components/MhiAcCtrl/mhi_status_decoder.cpp \
  components/MhiAcCtrl/mhi_stats.cpp \
  components/MhiAcCtrl/mhi_tx_builder.cpp \
  -o "${BUILD_DIR}/mhi_protocol_tests"

"${BUILD_DIR}/mhi_protocol_tests"

python3 tests/unit/test_driver_selection.py
