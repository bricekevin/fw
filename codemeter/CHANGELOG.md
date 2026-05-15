# Changelog

All notable changes to M5Clawd are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); this project is pre-MVP and
not yet versioned for release.

## [Unreleased] — Phase 2 (code-complete, hardware validation pending)

### Added
- Anthropic HTTPS poller (`poller.ino`): TLS POST to `api.anthropic.com`,
  rate-limit header parsing into `UsageData`, ArduinoJson error-body
  classification, NTP time sync.
- Pinned TLS root CA (`certs.h`): self-signed GTS Root R4, probed from the
  live certificate chain.
- Poll loop with exponential backoff (60→120→240→300 s) and WiFi
  auto-reconnect.
- Usage screen (`ui.ino`): SESSION (5h) and WEEKLY (7d) cards with font-7
  percentages, a status-badge bar, reset countdowns, and flicker-free
  partial redraw.
- Three-screen button cycle (Splash / Usage / Status); button B forces an
  immediate poll.
- Last-known-good `UsageData` persisted to NVS (`usage_store.ino`) and
  restored on boot so a cold start shows the last numbers, flagged stale.
- Pure-logic modules with a host `g++` unit-test harness (`m5clawd/test/`):
  header parsing, formatters, poll-state machine — 100 checks.
- GitHub Actions CI: firmware compile + host tests + API-key secret scan.

### Notes
- Header names/formats confirmed against the Clawdmeter daemon: the windows
  are `5h` / `7d`, utilization is a 0–1 fraction, reset is a Unix timestamp.
- Not yet run on hardware — the on-device TLS handshake, a live poll, and the
  rendered Usage screen are unverified (smoke tests, Phase 2 Task 5.2).

## [0.1.0] — 2026-05-15 — Phase 1 (hardware-validated)

### Added
- Firmware bootstrap, copy-stripped from the `Crypto_Coin_TickerUS_Stock`
  reference (ADR 006).
- WiFiManager captive-portal onboarding with a single Anthropic API-key field.
- NVS-persisted credentials; two-stage long-press-C reset gesture.
- WiFi station mode; splash / connecting / status / provisioning screens.
- `arduino-cli` build profile (`sketch.yaml`) pinning ESP32 core 1.0.4,
  M5Stack 0.3.1, ArduinoJson 6.17.3.
