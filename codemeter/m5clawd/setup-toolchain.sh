#!/usr/bin/env bash
#
# setup-toolchain.sh — one-time toolchain install for M5Clawd.
#
# Installs the arduino-cli ESP32 core and libraries pinned by ADR 001.
# Safe to re-run; arduino-cli skips already-installed components.
#
# Prerequisite: arduino-cli on PATH  (macOS: `brew install arduino-cli`).
#
# NOTE (ADR 001 risk): ESP32 core 1.0.4 is old (2020). It is pinned because the
# Crypto_Coin_TickerUS_Stock reference project requires it. If a modern
# arduino-cli refuses to build against 1.0.4, that is a Phase 1 finding — revise
# ADR 001 and this script rather than silently bumping the version.

set -euo pipefail

ESP32_INDEX="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
ESP32_CORE_VERSION="1.0.4"
M5STACK_LIB_VERSION="0.3.1"
ARDUINOJSON_LIB_VERSION="6.17.3"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "ERROR: arduino-cli not found on PATH." >&2
  echo "Install it first:  brew install arduino-cli" >&2
  exit 1
fi

echo "arduino-cli: $(arduino-cli version)"

echo "==> Registering the ESP32 board index"
arduino-cli config init --overwrite >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls "$ESP32_INDEX"
arduino-cli core update-index

echo "==> Installing ESP32 core ${ESP32_CORE_VERSION}"
arduino-cli core install "esp32:esp32@${ESP32_CORE_VERSION}"

echo "==> Installing libraries"
# WiFiManager is NOT installed here — it is vendored in this folder
# (WiFiManager.cpp/.h + wm_*.h), copied from the crypto ticker. See ADR 003.
arduino-cli lib install "M5Stack@${M5STACK_LIB_VERSION}"
arduino-cli lib install "ArduinoJson@${ARDUINOJSON_LIB_VERSION}"

echo
echo "Toolchain ready. Installed cores:"
arduino-cli core list
echo
echo "Next:  ./flash.sh"
