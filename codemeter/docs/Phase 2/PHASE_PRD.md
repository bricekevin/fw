# Phase 2 - Anthropic Poller + Usage UI PRD

**Duration:** ~1 week (evening/weekend pace)
**Status:** PLANNING
**Owner:** Kevin Brice

---

## Problem Statement

**Problem:** After Phase 1, M5Clawd boots, onboards over a captive portal, persists an Anthropic API key to NVS, and connects to WiFi in station mode — but it does nothing with that connection. It shows a diagnostic status screen and no usage data. The `UsageData` struct exists in `config.h` but is never populated; `poller.ino` does not exist; the Usage screen from the design system is unbuilt.

**Why Now:** Polling Anthropic and rendering utilization *is* the product. Phase 1 delivered the plumbing; Phase 2 delivers the reason the device sits on the desk. This phase completes the MVP — per `1_PROJECT_OVERVIEW.md`, MVP is "done at the end of Phase 2."

**Impact:** The developer (Kevin) and any future user — Phase 2 turns "a device on my desk that connects to my WiFi" into "a glanceable Claude usage fuel gauge."

---

## Objectives

**Primary Goal:** The device polls `api.anthropic.com` every 60 s over TLS, parses the `anthropic-ratelimit-*` response headers into `UsageData`, and renders session % / weekly % / reset countdowns on a Usage screen — resiliently, with clear error states and last-known-good persistence.

**Success Metrics:**
- The device completes a successful TLS POST to `api.anthropic.com/v1/messages` and the Usage screen shows real session/weekly percentages within 90 s of boot.
- Single poll latency (TLS handshake + request + parse) < 5 s p95 (serial-timestamped).
- Over a 24 h window on stable WiFi, poll-success rate >= 99%.
- Pulling WiFi shifts the UI to a `wifi-` state within one poll interval; the last good numbers remain on screen; recovery restores `wifi` within two intervals.
- Free heap stays > 100 KB after 24 h uptime (no leak across poll cycles).
- A reboot while WiFi is down still shows the last-known-good numbers (restored from NVS).
- Host unit tests cover the pure-logic modules at >= 60% line coverage and pass in `m5clawd/test/run.sh`.

---

## User Stories

### Story 1: See my Claude usage at a glance
**As a** Claude Code power user
**I want** the device to show my current 5-hour and weekly rate-limit utilization
**So that** I can tell at a glance whether I'm about to hit a limit, without reading API headers.

**Acceptance:**
- The Usage screen shows session %, weekly %, and a "resets in Xh Ym" countdown for each
- A status bar shows WiFi state, the last-poll time, and a free-heap indicator
- Numbers refresh automatically every poll interval (default 60 s)

### Story 2: Trust the display when something is wrong
**As a** user
**I want** the device to clearly tell me when it can't get fresh data
**So that** I never mistake stale numbers for current ones.

**Acceptance:**
- Distinct on-screen states for: no API key, WiFi down, API unreachable, auth failed (key rejected), rate-limited
- When data is stale, the status badge says so; the last good numbers stay visible rather than blanking
- Error states use the design-system color + a text glyph (not color alone)

### Story 3: Survive WiFi blips and reboots
**As a** user who leaves the device running unattended
**I want** it to recover from brief WiFi drops and to show something meaningful right after a reboot
**So that** the device is trustworthy as an always-on desk display.

**Acceptance:**
- A brief WiFi outage triggers exponential backoff, not a hammering retry loop, and the device reconnects automatically when WiFi returns
- The last successful `UsageData` is persisted to NVS and restored on boot, so a cold boot while WiFi is down shows the last known numbers with a "stale" badge
- The poll loop runs for 24 h without a heap leak or hard reset

### Story 4: Force a refresh
**As a** user who just changed something and wants fresh numbers now
**I want** a button to trigger an immediate poll
**So that** I don't have to wait out the 60 s interval.

**Acceptance:**
- Button B triggers an immediate poll
- Button A cycles Splash <-> Usage <-> Status

---

## Requirements

### Functional Requirements
1. The poller must open a TLS 1.2/1.3 connection to `api.anthropic.com:443`, validate the server certificate against a bundled root CA, and POST the minimal Messages request defined in `2_ARCHITECTURE.md`.
2. The poller must parse the four `anthropic-ratelimit-unified-*` response headers (5h + weekly utilization and reset) into a `UsageData` struct.
3. The poller must parse the response body (ArduinoJson 6) only enough to detect API errors (`authentication_error`, `rate_limit_error`, etc.) and map them to a `UsageData::Status`.
4. The poll loop must run every `poll_interval_s` (default 60) and apply exponential backoff (60 -> 120 -> 240 -> 300 s cap) on consecutive failures, resetting to the base interval on the first success.
5. The Usage screen must render session %, weekly %, both reset countdowns, and a status bar (WiFi state, last-poll time, heap).
6. The UI must show a distinct, labelled state for each non-OK `UsageData::Status` value, and must display last-known-good numbers (flagged stale) rather than blanking when a poll fails.
7. The firmware must persist the last successful `UsageData` to NVS and restore it on boot.
8. Button B must trigger an immediate poll; button A must cycle Splash <-> Usage <-> Status.
9. Pure-logic modules (header parsing, countdown/percent formatting, the poll-state machine) must be implemented as Arduino-header-free `.h`/`.cpp` pairs so they compile and run under host `g++`.

### Non-Functional Requirements
- **Performance:** Single poll (TLS + request + parse) < 5 s p95. Boot-to-first-Usage-render < 90 s.
- **Reliability:** >= 99% poll success over 24 h on stable WiFi; no heap leak (free heap > 100 KB after 24 h); no boot loops; automatic WiFi reconnect within 30 s of an AP returning.
- **Security:** TLS certificate validation is mandatory — no `setInsecure()` fallback. The API key goes only into the `x-api-key` header and is never printed to serial (use `secret_redactor()`). No `sk-ant-` literal in any committed file.
- **Resilience:** A failed poll never crashes the loop; it transitions the state machine and the UI. Last-known-good values survive reboot.
- **Testability:** >= 60% line coverage on the pure-logic modules via host `g++` tests.

### System Constraints
- ESP32 original — WPA2 only, ~520 KB SRAM. TLS handshakes are heap-hungry; the poller must free `WiFiClientSecure` / `HTTPClient` after every poll.
- ESP32 Arduino core 1.0.4, classic `M5Stack.h` 0.3.1, ArduinoJson **6.17.3** (v6 API — `StaticJsonDocument`/`DynamicJsonDocument`, not v7).
- All builds use `arduino-cli compile --profile m5clawd` (the `sketch.yaml` profile — see HANDOFF_NOTES Session 2). A bare `--fqbn` invocation fails on this Mac due to global-sketchbook SD-library shadowing.
- The `anthropic-ratelimit-*` header names are not a documented contract; parsing must degrade gracefully to an `UNKNOWN` state if a header is missing or malformed.

---

## Scope

### In Scope
- `poller.ino` — TLS POST to Anthropic, header extraction, body error detection.
- Bundled root CA(s) for `api.anthropic.com` certificate validation.
- Pure-logic modules: `parse_headers.{h,cpp}`, `format_helpers.{h,cpp}`, `state_machine.{h,cpp}`, plus a pure `usage_data.h` (the `UsageData` struct extracted free of Arduino headers).
- Periodic poll loop with exponential backoff, integrated into `loop()`.
- The Usage screen (`ui.ino`) — session/weekly cards, status bar, partial-redraw of changing fields.
- Status overlays / badges for every non-OK `UsageData::Status`.
- Last-known-good `UsageData` persistence to NVS and restore on boot.
- Button B = force poll; button A cycle extended to three screens.
- Host unit tests in `m5clawd/test/` with a `run.sh` harness.
- Optional: a minimal GitHub Actions CI workflow (compile + host tests + secret scan) — gated on a GitHub remote existing.

### Out of Scope (deferred)
- **Splash animations / Clawd pixel-art** — Phase 3.
- **Battery indicator, audio chime on threshold crossings, NeoPixel** — Phase 3.
- **SPIFFS PNG assets** — Phase 3 (Phase 2 screens stay text + drawn shapes).
- **Portal-tunable poll interval** — `poll_interval_s` stays a compile-time default; no portal field this phase.
- **OTA cert/firmware updates, mDNS, NVS encryption** — future phases.
- **NTP / wall-clock time** — last-poll time is shown as "Xs/Xm ago" relative to `millis()`, not an absolute clock, unless an NTP sync proves trivial; absolute time is a stretch goal, not a requirement.

---

## Dependencies

### External Dependencies
- Phase 1 firmware — committed, hardware-validated (boot-mode select, NVS, station-mode WiFi, screens).
- A valid Anthropic API key on the test device (entered via the Phase 1 captive portal).
- Live `api.anthropic.com` reachability from the dev WiFi network.
- `arduino-cli` + the `m5clawd` build profile (ESP32 core 1.0.4, M5Stack 0.3.1, ArduinoJson 6.17.3) — installed and validated in Phase 1.
- Host `g++` (C++17) for the pure-logic unit tests.

### Prerequisites
- Phase 1 complete — done (firmware compiles clean, all four smoke scenarios pass on hardware).

### Assumptions
- The `anthropic-ratelimit-unified-5h-utilization` / `-week-utilization` / `-5h-reset` / `-week-reset` headers are present on a normal `200` response. Confirmed against the Clawdmeter daemon source; re-confirm with a real device response early in Task 2.3.
- A bundled root CA (Amazon Root CA 1 and/or ISRG Root X1) chains to `api.anthropic.com`'s leaf certificate. **This is the headline risk — validate first thing (Task 2.1).**
- `HTTPClient` exposes response headers via `collectHeaders()` / `header(name)` on ESP32 core 1.0.4 (it does — long-standing API; confirm in Task 2.3).
- ArduinoJson 6.17.3 can parse the error-response body within available heap (the body is small; a `StaticJsonDocument` sized ~512 B suffices).

---

## User Experience

### Key User Flows
1. **Normal operation:** Boot -> splash -> connecting -> first poll fires immediately on `WL_CONNECTED` -> Usage screen with live numbers, refreshing every 60 s.
2. **WiFi drop:** Router goes down -> next poll fails -> status badge -> `wifi-`, numbers stay (flagged stale), backoff slows retries -> router returns -> WiFi reconnects, next successful poll restores `wifi` and fresh numbers.
3. **Bad key:** Onboarded with a key Anthropic rejects -> first poll returns `401` -> Usage screen shows the `auth!` state -> user long-presses C to reset and re-onboard (Phase 1 path).
4. **Force refresh:** User presses B -> an immediate poll fires regardless of the interval timer.
5. **Cold boot, WiFi down:** Boot -> can't connect -> Usage screen shows last-known-good numbers from NVS with a stale badge.

### UI/UX Requirements
- Usage screen follows the layout in `docs/3_DESIGN_SYSTEM.md` section "2. Usage": status bar + two stacked cards (SESSION, WEEKLY).
- Only the changing fields (the % numbers, the countdowns, the status badge) are redrawn per poll — titles and background stay put (design-system "Animation Budget").
- Status badges use the design-system mapping (`wifi` / `wifi...` / `wifi-` / `api-` / `auth!` / `rate!`) — a text glyph plus a color, never color alone.
- All colors come from the `config.h` constants — no raw hex in `ui.ino`.
- Reset countdowns use `format_reset_countdown()` (e.g. `7320 s` -> `"2h 2m"`).

---

## Risks & Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Bundled root CA does not chain to `api.anthropic.com`; TLS handshake fails | MED | HIGH | Task 2.1 is sequenced first and bundles **two** roots (Amazon Root CA 1 + ISRG Root X1). Validate the handshake with a bare TLS probe before building the poller. If both fail, capture the served chain (`openssl s_client`) and bundle the correct root — never ship `setInsecure()`. |
| TLS handshakes leak heap; 24 h uptime degrades | MED | HIGH | Stack-scope `WiFiClientSecure`/`HTTPClient` so they free every poll; `http.end()` + `client.stop()` explicitly; print `ESP.getFreeHeap()` each poll and watch the trend during the 24 h test. |
| Anthropic rate-limit header names differ from the assumed names | LOW | MED | Dump **all** response headers to serial on the first real poll (Task 2.3) and confirm the exact names before finalizing `parse_headers`. Parser falls back to `UNKNOWN` on any missing header. |
| Single poll exceeds the 5 s p95 budget on a slow handshake | LOW | LOW | The poll runs from `loop()` between `M5.update()` calls; a slow poll only delays a refresh, it does not block buttons catastrophically. If chronic, revisit (a task queue is Phase 3). |
| `claude-haiku-4-5` model id rejected / renamed | LOW | MED | The model id is a `config.h` constant (`ANTHROPIC_POLL_MODEL`); a `400 invalid model` is detectable in the body parse and surfaced. Easy one-line fix. |
| ArduinoJson v7 API used by mistake | LOW | LOW | CLAUDE.md and this PRD pin v6; code review checks for `StaticJsonDocument`/`DynamicJsonDocument`, not v7's unified `JsonDocument`. |

---

## Open Questions

- [ ] Which single root CA does `api.anthropic.com` actually chain to as of 2026-05? (Resolved by Task 2.1 — probe with `openssl s_client -connect api.anthropic.com:443 -showcerts`.)
- [ ] Are the `anthropic-ratelimit-unified-*` header names exactly as assumed on a live response? (Resolved by the header dump in Task 2.3.)
- [ ] Is an absolute wall-clock "last poll 14:23" worth an NTP sync, or is relative "42s ago" enough for MVP? (Default: relative. Promote to absolute only if NTP is trivial.)
- [ ] Does the project have a GitHub remote yet? (Determines whether the CI task in Epic 5 runs this phase or is deferred.)

---

**Approved By:** Kevin Brice
**Updated:** 2026-05-15
