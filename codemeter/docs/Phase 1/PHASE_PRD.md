# Phase 1 - Bootstrap (Copy-Strip + Captive Portal + WiFi) PRD

**Duration:** ~1 week (evening/weekend pace)
**Status:** PLANNING
**Owner:** Kevin Brice

---

## Problem Statement

**Problem:** M5Clawd has a complete doc set and a scaffolded `m5clawd/` folder (toolchain scripts, `config.h`, vendored WiFiManager) — but no actual firmware. There is no `m5clawd.ino`, the toolchain has never been exercised, and nothing has been flashed to the device. We cannot build, flash, or run anything yet.

**Why Now:** This is the first implementation phase. Everything downstream — the Anthropic poller (Phase 2), the polished UI (Phase 3) — depends on a device that boots, onboards over WiFi, and has a clean-compiling sketch. Until that exists, no feature work can start.

**Impact:** The developer (Kevin) — Phase 1 is what turns the project from "docs + scaffold" into "a device on my desk that connects to my WiFi."

---

## Objectives

**Primary Goal:** A flashable M5Clawd firmware that boots, runs a WiFiManager captive portal on first boot, collects WiFi credentials plus an Anthropic API key, persists them to NVS, and connects to WiFi in station mode on subsequent boots.

**Success Metrics:**
- `arduino-cli compile` of `m5clawd/` succeeds with zero errors
- Fresh-flashed device raises the `M5Clawd-XXXXXX` soft-AP and serves the captive portal within 30 s of power-on
- After submitting WiFi creds + an API key, the device reboots and reaches `WL_CONNECTED` within 30 s
- Credentials survive a reflash (NVS persistence verified)
- Long-press button C wipes NVS and returns the device to portal mode

---

## User Stories

### Story 1: Build and flash from a clean clone
**As a** developer
**I want** to run two scripts and have firmware on my device
**So that** the "easy to setup once cloned" promise holds from the very first phase.

**Acceptance:**
- `m5clawd/setup-toolchain.sh` installs the ESP32 core and libraries without manual steps
- `m5clawd/flash.sh` compiles and uploads to the M5Stack Core over USB-C

### Story 2: Onboard the device with no recompile
**As a** user setting up the device
**I want** to enter my WiFi credentials and Anthropic API key in a phone browser
**So that** I never have to edit code or hardcode secrets before flashing.

**Acceptance:**
- On first boot the device shows "Configure WiFi" with the AP SSID and a QR code
- Connecting a phone to `M5Clawd-XXXXXX` opens (or makes reachable at `http://192.168.4.1`) a config page
- The page has the standard WiFi fields plus one custom field: "Anthropic API Key"
- Pressing Save persists everything and reboots the device

### Story 3: Recover from a wrong entry
**As a** user who fat-fingered the API key or changed WiFi networks
**I want** a way to wipe the device's stored config
**So that** I can re-onboard without reflashing.

**Acceptance:**
- Holding button C for 5 s shows an on-screen confirmation, then wipes NVS and reboots into portal mode
- After the wipe, the device behaves exactly as a fresh flash

---

## Requirements

### Functional Requirements
1. The firmware must compile under `arduino-cli` with the ESP32 core and libraries pinned in ADR 001.
2. On boot, the firmware must select **provisioning mode** (no stored creds) or **station mode** (creds present) based on an NVS flag.
3. In provisioning mode, the firmware must run the vendored WiFiManager captive portal with exactly one custom parameter (the Anthropic API key) alongside the built-in WiFi fields.
4. On portal save, the firmware must persist the API key to NVS namespace `m5clawd` and set the `wifi_configured` flag.
5. In station mode, the firmware must connect to WiFi using the stored credentials.
6. The firmware must render at least three screens: a boot splash, a "connecting" screen, and a diagnostic status screen.
7. A 5-second long-press of button C must clear NVS (both WiFiManager's slot and our `m5clawd` namespace) and restart.

### Non-Functional Requirements
- **Performance:** Cold boot to first on-screen content < 3 s. Portal-save to `WL_CONNECTED` < 30 s.
- **Security:** The API key must never be printed to serial. The captive-portal key field uses `type="password"`. No `sk-ant-` literal in any committed file.
- **Reliability:** The device must reach a stable state (portal up, or WiFi connected) on every boot — no boot loops.
- **Code size:** The stripped `m5clawd.ino` plus tabs must be meaningfully smaller than the crypto ticker's ~190 KB sketch (we delete more than we add).

### System Constraints
- ESP32 original (no S3) — WPA2 only, ~520 KB SRAM.
- ESP32 Arduino core **1.0.4** (pinned, 2020-era) — see ADR 001. Compatibility with modern `arduino-cli` is the headline risk.
- Classic `M5Stack.h` (v0.3.1) — `M5.Lcd.*` drawing only, no M5GFX/LVGL.
- WiFiManager is vendored in-tree, not package-managed.

---

## Scope

### In Scope
- Toolchain validation (`setup-toolchain.sh` actually works on the dev Mac).
- Copy the crypto ticker's canonical sketch into `m5clawd/m5clawd.ino`; split into `.ino` tabs.
- Strip all crypto/stock/chart/sensor/SD code down to a clean-compiling skeleton.
- Captive portal with one API-key field; NVS persistence; provisioning vs station mode selection.
- WiFi station-mode connection with stored credentials.
- Boot splash, connecting screen, diagnostic status screen; button A screen-cycling.
- NVS reset gesture (long-press button C).

### Out of Scope (deferred)
- **Anthropic API polling** — no HTTPS calls to `api.anthropic.com` this phase. Phase 2.
- **Usage screen with real data** — the Usage screen is Phase 2 (it needs the poller).
- **Splash animations / Clawd pixel-art** — Phase 3.
- **Battery indicator, audio chime, NeoPixel** — Phase 3.
- **SPIFFS PNG assets** — Phase 1 screens are text-only; PNG upload is Phase 3.
- **OTA updates, mDNS, NVS encryption** — future phases.

---

## Dependencies

### External Dependencies
- `arduino-cli` (installed — v1.2.2 confirmed on the dev Mac).
- ESP32 Arduino core 1.0.4 and libraries M5Stack 0.3.1, ArduinoJson 6.17.3 (installed by `setup-toolchain.sh`).
- The `Crypto_Coin_TickerUS_Stock` reference project at `~/GIT/Crypto_Coin_TickerUS_Stock` (read-only copy source).
- M5Stack Core Basic hardware + USB-C cable.

### Prerequisites
- `/1_start` complete — done (docs, ADRs, `m5clawd/` scaffold all committed).

### Assumptions
- The crypto ticker's WiFiManager integration compiles and runs on the target device (it is the user's working firmware, so this is well-founded).
- `M5.Lcd.qrcode()` exists in M5Stack library 0.3.1 (it is a long-standing API; confirm during Task 3.3).
- The dev Mac can talk to the device over USB-C (CP2104; confirm in Task 1.2).

---

## User Experience

### Key User Flows
1. **First boot:** Power on -> "Configure WiFi" screen with AP SSID + QR -> user joins the AP from a phone -> captive page -> fill WiFi + API key -> Save -> device reboots.
2. **Normal boot:** Power on -> splash -> "Connecting to <ssid>" -> diagnostic status screen once connected.
3. **Reset:** Hold button C 5 s -> on-screen "Reset? keep holding…" confirmation -> NVS wiped -> reboots to first-boot flow.

### UI/UX Requirements
- Provisioning screen must show the exact AP SSID and the `http://192.168.4.1` fallback URL (in case the captive pop-up doesn't fire).
- Screens use the color constants in `config.h` — no raw hex.
- Status screen is a plain diagnostic dump (version, SSID, RSSI, IP, heap, uptime) — utilitarian, no animation.

---

## Risks & Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| `arduino-cli` 1.2.2 cannot build against ESP32 core 1.0.4 | MED | HIGH | Task 1.1 validates this **first**, before any other work. If it fails, revise ADR 001 (try a newer core; the crypto ticker's "1.0.4 required" note may not bind a fresh strip) rather than blocking the phase. |
| The strip breaks compilation in tangled, hard-to-trace ways | MED | MED | Strip incrementally (Tasks 2.2, 2.3) and recompile after every removal so breakage stays localized. |
| The 190 KB single `.ino` is hard to navigate during the strip | MED | LOW | Split into tabs early (Task 2.1) before stripping. |
| `M5.Lcd.qrcode()` absent in M5Stack 0.3.1 | LOW | LOW | If absent, fall back to showing the AP SSID + URL as text only; QR is a convenience, not a blocker. |
| Device won't enumerate over USB on the dev Mac | LOW | MED | Task 1.2 isolates this early; CP210x driver install is the documented fix. |

---

## Open Questions

- [ ] Does ESP32 core 1.0.4 install and compile cleanly under `arduino-cli` 1.2.2? (Resolved by Task 1.1.)
- [ ] Is the user's board the plain Core Basic or a FIRE/Gray variant? (Low impact — `M5Stack.h` handles all; confirm at first flash.)
- [ ] Should the API-key field do a live "test the key" call in the portal? (Deferred — that needs the Phase 2 HTTPS client. Phase 1 only does a format check.)

---

**Approved By:** Kevin Brice
**Updated:** 2026-05-15
