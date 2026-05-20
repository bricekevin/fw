#!/usr/bin/env bash
#
# flash.sh — compile and upload the M5Clawd firmware to an M5Stack Core Basic.
#
# Usage:
#   ./flash.sh                       # auto-detect the USB serial port
#   ./flash.sh /dev/cu.usbserial-XX  # or pass an explicit port
#
# Prerequisite: run ./setup-toolchain.sh once first.
#
# NVS note: a plain upload does NOT erase NVS, so stored WiFi creds + the
# Anthropic API key survive a reflash. To wipe them, the device's reset gesture
# (long-press button C) is the supported path; a full-chip erase also works.
#
# SPIFFS note: this script flashes firmware only. Uploading the data/ folder as
# a SPIFFS image is a Phase 3 concern (splash PNGs) and is not wired up yet.
#
# Build profile: this script builds with the `m5clawd` profile defined in
# sketch.yaml. The profile isolates library resolution from the dev Mac's
# global sketchbook, which carries a generic `SD` library that otherwise
# shadows the ESP32 core's SD and breaks the build. See sketch.yaml.

set -euo pipefail

PROFILE="m5clawd"
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Resolve the upload port ------------------------------------------------
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT="$(ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART 2>/dev/null | head -n1 || true)"
fi
if [ -z "$PORT" ]; then
  echo "ERROR: no USB serial port found." >&2
  echo "Plug in the M5Stack (USB-C), then:  ls /dev/cu.*" >&2
  echo "Pass the port explicitly:  ./flash.sh /dev/cu.usbserial-XXXX" >&2
  exit 1
fi
echo "Port: $PORT"

# --- Sanity check: this folder must be a valid Arduino sketch ---------------
if [ ! -f "$SKETCH_DIR/m5clawd.ino" ]; then
  echo "ERROR: m5clawd.ino not found in $SKETCH_DIR" >&2
  echo "The sketch is created in Phase 1 (copy-and-strip the crypto ticker, ADR 006)." >&2
  exit 1
fi

# --- Compile ----------------------------------------------------------------
# Build to a known output dir so the upload step can run a different FQBN
# (with an overridden UploadSpeed) without rebuilding.
BUILD_DIR="$SKETCH_DIR/build"
echo "==> Compiling"
arduino-cli compile --profile "$PROFILE" --output-dir "$BUILD_DIR" "$SKETCH_DIR"

# --- Upload -----------------------------------------------------------------
# Default to 115200 baud. The board default (921600) is faster (~10 s vs ~64 s)
# but fails intermittently on marginal USB cables ("Timed out waiting for
# packet header") and has eaten 3+ sessions worth of debugging. 115200 always
# works; users with a known-good cable can bump it via FLASH_BAUD=921600.
BAUD="${FLASH_BAUD:-115200}"
FQBN="esp32:esp32:m5stack-core-esp32:UploadSpeed=$BAUD"
echo "==> Uploading to $PORT @ $BAUD baud"
arduino-cli upload --fqbn "$FQBN" --input-dir "$BUILD_DIR" --port "$PORT" "$SKETCH_DIR"

echo
echo "Done. Watch serial output with:"
echo "  arduino-cli monitor -p $PORT -c baudrate=115200"
