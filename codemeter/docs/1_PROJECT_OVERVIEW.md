# Project Overview

**Project:** M5Clawd
**Version:** 0.0.1 (pre-MVP)
**Status:** Planning → Phase 1 imminent

---

## Vision

M5Clawd is a desk-side Claude Code usage display that runs on an **M5Stack Core Basic (Gray, ESP32)** and shows your current 5-hour and weekly Anthropic API utilization at a glance. Unlike the [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) project it is derived from, M5Clawd is **standalone**: it connects to your home WiFi, polls `api.anthropic.com` directly, and needs no companion laptop or daemon. Onboarding is a single captive-portal page on first boot — flash it, connect your phone, fill in two fields, done.

---

## Problem & Solution

### Problem

Claude Code's rate-limit utilization (`session %`, `weekly %`) is only visible from the API response headers — there's no glanceable "fuel gauge" surface. Upstream Clawdmeter solves this with a desk-display + paired laptop daemon, but that has high setup friction (Bluetooth pairing, daemon install, OS-specific scripts) and is hardware-locked to a $40 Waveshare AMOLED board that isn't widely available.

### Solution

Port the concept to the M5Stack Core Basic — a common, off-the-shelf ESP32 dev board — and re-architect the data path:

- **Onboarding:** Captive-portal Wi-Fi setup (tzapu/WiFiManager) in a phone browser. No app install, no recompile. *(Note — 2026-05: Phase 2 hardware testing found the usage figures require a Claude Code **OAuth token**, not a plain API key; the onboarding flow for that credential is being redesigned as its own phase. See `docs/HANDOFF_NOTES.md` Session 7.)*
- **Operation:** Device polls `api.anthropic.com` directly every 60 seconds, reads usage headers, renders on a 320×240 IPS LCD.
- **No host daemon. No BLE. No companion software of any kind.**

Result: clone → `./flash.sh` → power on → portal-pop → 5-minute total setup, then unattended forever.

### Target Users

- **Primary:** Claude Code power users who already own an M5Stack Core Basic and want a glanceable usage display without running yet another background daemon on their laptop.
- **Secondary:** Hardware tinkerers looking for a small, focused ESP32 project that demonstrates WiFi captive portals + TLS API polling + simple UI on a constrained device.

---

## Objectives

### Goals

1. **Make first-boot setup take under 5 minutes** including connecting the phone to the AP and filling the form.
2. **Eliminate host-side software entirely** — no daemon, no app, no install script for the operator.
3. **Run unattended for 7+ days** on a USB charger with reasonable resilience to brief WiFi drops.
4. **Reuse a proven scaffold** — start from a copy-and-strip of the user's working `Crypto_Coin_TickerUS_Stock` project so onboarding, display, and persistence are known-good from day one (see ADR 006).
5. **Keep the codebase small enough that a single contributor can hold the whole thing in head** — M5Clawd should end up *smaller* than the crypto ticker it's stripped from.

### Success Metrics

| Metric                        | Target                                          |
| ----------------------------- | ----------------------------------------------- |
| First-boot to live data       | < 5 min (typical user, never seen the device)   |
| Reflash → resume normal op    | < 30 s (creds survive in NVS)                   |
| Unattended uptime             | 7 days without hard reset                       |
| Poll-loop reliability         | ≥ 99% poll success rate over a 24h window on a stable WiFi |
| Binary size                   | < 1.5 MB (well under partition limit)           |
| Firmware LOC                  | < 2,000 lines C++                               |

---

## Core Features

> Phase breakdown note: the copy-and-strip decision (ADR 006) arrived after this
> overview's first draft. Phase 1 is now scoped to **bootstrap only** — the
> copy-strip is itself substantial work, so Anthropic polling moved to Phase 2.
> Phase 1's plan lives in `docs/Phase 1/`.

### Phase 1 — Bootstrap (copy-strip + captive portal + WiFi)

1. **Toolchain validation** — confirm `arduino-cli` builds against the pinned ESP32 core.
2. **Copy-and-strip** — `m5clawd.ino` derived from the crypto ticker, stripped to a clean-compiling skeleton.
3. **Captive-portal onboarding** — Soft-AP `M5Clawd-XXXXXX`; phone-browser config page with WiFi fields + one Anthropic API-key field.
4. **NVS-persisted secrets** — Creds survive reboot and reflash; reset gesture (long-press button C, 5s) wipes them.
5. **WiFi station mode + basic screens** — connects with stored creds; splash / connecting / diagnostic-status screens.

### Phase 2 — Anthropic poller + Usage UI

1. **Anthropic HTTPS poll** — TLS call to `api.anthropic.com`, parse rate-limit headers into `UsageData`.
2. **Usage screen** — Session %, weekly %, reset countdowns, last-poll timestamp.
3. **Periodic poll loop** with exponential backoff on failure.
4. **Status overlays** — Clear UI for "no API key", "rate-limit error", "WiFi down", "auth failed".
5. **Resilience** — last-known-good values persisted across reboots; heap watchdog.

### Phase 3 — Polish

1. **Splash screen** with Clawd art — SPIFFS PNGs drawn via `M5.Lcd.drawPngFile`, optionally a short cycled frame sequence.
2. **Button A/B/C interactions** — A = cycle screens; B = force-refresh poll; C = (long-press) reset NVS.
3. **Battery indicator** via `M5.Power` (the classic `M5Stack.h` power API over IP5306).
4. **Audio chime** on rate-limit threshold crossings (built-in speaker).

### Future Phases (not in MVP scope)

- **OTA updates** via WebOTA or ArduinoOTA over WiFi.
- **mDNS discovery** so users can browse to `http://m5clawd.local/` for config.
- **NVS encryption** for hardening API-key storage.
- **BLE HID re-add** — make the M5Stack's buttons send Space / Shift+Tab to a paired laptop (the upstream feature we deferred).
- **Optional MQTT push** for users with a homelab Grafana setup.

---

## Timeline

| Phase  | Focus                                              | Duration (estimate) |
| ------ | -------------------------------------------------- | ------------------- |
| Phase 1 | Bootstrap — copy-strip, captive portal, WiFi connect | ~1 week             |
| Phase 2 | Anthropic poller + Usage UI + reliable loop        | ~1 week             |
| Phase 3 | Splash art + button interactions + battery + audio | 1–2 weeks           |

**Target MVP completion:** End of Phase 2 (~2 weeks from Phase 1 start).

This is a single-contributor side project — timelines are estimates against evening/weekend pace, not full-time.

---

## Key Risks

| Risk                                              | Impact | Mitigation                                                                                                                                                                                                                                              |
| ------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ESP32 TLS handshake fails to `api.anthropic.com`  | HIGH   | Bundle 2+ likely root certs (Amazon Root CA 1, ISRG Root X1). Document OTA path before MVP ships. Test on day 1 of Phase 2.                                                                                                                              |
| Anthropic rate-limit headers change format        | MED    | Defensive JSON / header parsing with fallback to "unknown" state. Anthropic has been stable on these headers for 12+ months but it's not a documented contract.                                                                                          |
| API key extraction from stolen device             | MED    | Recommend a scoped, low-limit Anthropic project key. Document NVS-encryption upgrade path. Reset gesture for fast revocation.                                                                                                                            |
| WiFi-6 / WPA3-only home networks                  | LOW    | ESP32 (original) is WPA2 only. Detect and show clear error. Document in README. (Not a problem for most home networks in 2026 yet.)                                                                                                                      |
| Drift from upstream Clawdmeter conventions        | LOW    | Per ADR 004, upstream is reference-only. We define our own JSON schema, screen layouts, etc. Drift is expected and fine.                                                                                                                                  |
| Arduino core 1.0.4 + `arduino-cli` interaction    | LOW    | The pinned old ESP32 core works with the crypto ticker under the Arduino IDE; `arduino-cli` should compile the same tree. Validate on Phase 1 day 1; pin `arduino-cli` version in `setup-toolchain.sh`.                                                    |

---

## Out-of-Scope (for MVP, explicitly)

- **BLE in any form.** No HID buttons, no daemon. Phase 4+ at earliest.
- **A companion mobile app.** Captive portal is the only configuration surface.
- **User accounts / multi-user.** Single API key per device.
- **Local logging beyond serial.** No SD-card log files, no cloud telemetry.

---

**Last Updated:** 2026-05-14
**Owner:** Kevin Brice
