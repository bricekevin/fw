# Phase 2 - Anthropic Poller + Usage UI Tasks

**Status:** 0/16 complete — planning
**Updated:** 2026-05-15

> See PHASE_PRD.md for requirements and PHASE_IMP.md for code snippets and commands.
> See HANDOFF_NOTES.md for Phase 1 context (note: all builds use `--profile m5clawd`).
> Checkbox legend: `[ ]` pending, `[~]` in-progress, `[x]` done.

---

## Epic 1: Pure-logic foundation (host-testable)

> Build the Arduino-header-free modules first — they have no hardware dependency,
> they are the unit-test surface, and the poller + UI both consume them. Per
> docs/4_QUALITY_ASSURANCE.md, these stay free of `M5Stack.h`/`WiFi`/`Arduino.h`
> so host `g++` can compile them.

- [ ] **1.1 Extract `UsageData` into a pure header** (`usage_data.h`)
  - [ ] Move the `UsageData` struct + `Status` enum out of `config.h` into a new `usage_data.h` that includes only `<stdint.h>`
  - [ ] `config.h` `#include "usage_data.h"`; confirm the device build still compiles clean
  - [ ] Add a `bool stale` (or equivalent) field so the UI can flag last-known-good data — update the struct comment

- [ ] **1.2 Rate-limit header parser** (`parse_headers.{h,cpp}`)
  - [ ] `parse_anthropic_headers(...)` — given header values (as strings/a lookup), populate the `UsageData` utilization + reset fields
  - [ ] Parse utilization as a 0..100 percentage; clamp out-of-range values
  - [ ] Missing or malformed header -> leave the field at a sentinel and let the caller decide `UNKNOWN`
  - [ ] No Arduino headers — `<string>`, `<cstdint>`, `<cstdlib>` only

- [ ] **1.3 Formatting helpers** (`format_helpers.{h,cpp}`)
  - [ ] `format_reset_countdown(uint32_t seconds)` -> `"2h 14m"` / `"4d 7h"` / `"<1m"`
  - [ ] `format_relative_time(uint32_t seconds_ago)` -> `"42s ago"` / `"3m ago"` for the last-poll stamp
  - [ ] Percent-string helper if useful; keep all of it Arduino-header-free

- [ ] **1.4 Poll-state machine** (`state_machine.{h,cpp}`)
  - [ ] Given a poll outcome (OK / network-fail / auth-fail / rate-limited / no-key), compute the next `UsageData::Status`
  - [ ] Compute the next poll delay with exponential backoff (60 -> 120 -> 240 -> 300 cap); reset to base on success
  - [ ] Track a consecutive-failure count; expose whether data should be flagged stale

- [ ] **1.5 Host unit-test harness** (`test/run.sh` + `test/*_test.cpp`)
  - [ ] `test/run.sh` host-compiles each `*_test.cpp` with `g++ -std=c++17` against the pure sources and runs it
  - [ ] Tests for `parse_anthropic_headers()`, `format_reset_countdown()`, the state machine transitions, and `secret_redactor()`
  - [ ] >= 60% line coverage on the pure modules; `run.sh` exits non-zero on any failure

---

## Epic 2: Anthropic HTTPS poller

> Per docs/2_ARCHITECTURE.md "Poller". TLS to `api.anthropic.com` is the
> headline risk — do Task 2.1 before writing poll logic.

- [ ] **2.1 Validate TLS to `api.anthropic.com` + bundle root CA(s)** (`certs.h`)
  - [ ] Probe the served chain: `openssl s_client -connect api.anthropic.com:443 -showcerts`
  - [ ] Bundle the chaining root(s) — Amazon Root CA 1 and ISRG Root X1 — as PEM string literals in `certs.h`
  - [ ] On-device sanity check: `WiFiClientSecure` + `setCACert` connects to `api.anthropic.com:443` and the handshake succeeds
  - [ ] No `setInsecure()` — if validation fails, fix the bundled root, do not bypass

- [ ] **2.2 Build and send the poll request** (`poller.ino`)
  - [ ] New `poller.ino` tab; `poller_poll()` opens `WiFiClientSecure` (stack-scoped) + `HTTPClient`
  - [ ] POST `ANTHROPIC_MESSAGES_URL` with headers `x-api-key`, `anthropic-version`, `content-type`
  - [ ] Body: `{"model":ANTHROPIC_POLL_MODEL,"max_tokens":1,"messages":[{"role":"user","content":"."}]}`
  - [ ] `http.end()` + `client.stop()` on every path; print `[poll]` status + `ESP.getFreeHeap()` (no secret — use `secret_redactor()`)

- [ ] **2.3 Extract headers + detect body errors -> `UsageData`** (`poller.ino`)
  - [ ] `http.collectHeaders()` for the four `anthropic-ratelimit-unified-*` names; on the FIRST real poll, dump *all* headers to serial and confirm the exact names
  - [ ] Feed header values into `parse_anthropic_headers()` (Epic 1)
  - [ ] On non-2xx, parse the body with ArduinoJson 6 (`StaticJsonDocument`) -> map `error.type` to `AUTH_FAILED` / `RATE_LIMITED` / `API_UNREACHABLE`
  - [ ] Set `last_poll_unix` (or a `millis()`-based stamp) on success

- [ ] **2.4 Periodic poll loop + backoff** (`m5clawd.ino`)
  - [ ] Fire the first poll immediately on `WL_CONNECTED`
  - [ ] In `loop()`, poll when the state machine's next-delay has elapsed; non-blocking w.r.t. `M5.update()`/buttons
  - [ ] Feed each poll outcome through the Epic 1 state machine; apply the backoff delay
  - [ ] Detect WiFi loss (`WiFi.status() != WL_CONNECTED`) -> `WIFI_DOWN`; attempt reconnect; restore on return

---

## Epic 3: Usage screen + status overlays

> Per docs/3_DESIGN_SYSTEM.md sections "2. Usage" and "State Indicators".

- [ ] **3.1 Usage screen layout** (`ui.ino`)
  - [ ] `ui_show_usage(const UsageData&)` — status bar + SESSION card + WEEKLY card per the design system
  - [ ] Big % values in font 7; titles in the header font; countdowns in body font; all colors from `config.h`
  - [ ] Status bar: WiFi glyph, last-poll relative time, free-heap indicator

- [ ] **3.2 Partial redraw of changing fields** (`ui.ino`)
  - [ ] `ui_update_usage(const UsageData&)` redraws only the % numbers, countdowns, and status badge — not titles/background
  - [ ] Track the last-rendered values; skip a field whose value is unchanged to cut flicker
  - [ ] Full repaint only on a screen switch into Usage

- [ ] **3.3 Status overlays / badges** (`ui.ino`)
  - [ ] Render a distinct badge for each non-OK `Status`: `wifi...` / `wifi-` / `api-` / `auth!` / `rate!`, plus a "no API key" state
  - [ ] When data is stale, keep the last good numbers visible and show the stale badge (do not blank the cards)
  - [ ] Glyph + color together, never color alone (accessibility rule)

- [ ] **3.4 Wire buttons into the 3-screen cycle** (`m5clawd.ino`)
  - [ ] Extend `Screen` to `SCREEN_SPLASH` / `SCREEN_USAGE` / `SCREEN_STATUS`; button A cycles all three
  - [ ] Button B triggers an immediate poll (force refresh)
  - [ ] Default to `SCREEN_USAGE` once the first poll succeeds; auto-jump to `SCREEN_STATUS` if `Status != OK` for > 2 polls (design-system note)

---

## Epic 4: Resilience & persistence

- [ ] **4.1 Last-known-good `UsageData` in NVS** (`secrets_store.ino` or a new `usage_store` helper)
  - [ ] On every successful poll, write the `UsageData` fields to `Preferences` namespace `NVS_NAMESPACE`
  - [ ] On boot, restore them so a cold boot (even with WiFi down) shows last-known numbers flagged stale
  - [ ] Add the NVS key names to `config.h`

- [ ] **4.2 Heap watchdog + WiFi reconnect hardening** (`m5clawd.ino`)
  - [ ] Log free heap each poll; if it trends toward a floor, log a warning
  - [ ] On `WIFI_DOWN`, attempt a bounded reconnect each loop tick without blocking buttons
  - [ ] Confirm no leak across ~30 simulated poll cycles before the formal 24 h test

---

## Epic 5: Testing & documentation

- [ ] **5.1 Host unit tests pass** — `m5clawd/test/run.sh` green, >= 60% coverage on pure modules
- [ ] **5.2 Hardware smoke tests** (docs/4_QUALITY_ASSURANCE.md scenarios 2, 3)
  - [ ] Scenario 2 (happy path): valid key -> Usage screen shows real numbers within 90 s
  - [ ] Scenario 3 (WiFi drop): kill the router -> `wifi-` within one interval, numbers persist; restore -> `wifi` within two intervals
  - [ ] Bad-key path: confirm the `auth!` state on a rejected key
  - [ ] Capture serial logs (`[poll ok] session=..% weekly=..% heap=..`) and a photo of the Usage screen for the PR
- [ ] **5.3 Performance checks** — single-poll latency < 5 s p95; kick off the 24 h uptime/heap run
- [ ] **5.4 Optional CI workflow** — add `.github/workflows/ci.yml` (compile via the profile + `test/run.sh` + `sk-ant-` secret scan). Only if a GitHub remote exists; otherwise defer and note it.
- [ ] **5.5 Update documentation** — HANDOFF_NOTES session entry, this file's checkboxes, `CHANGELOG.md` (created this phase), and a new ADR if TLS/cert handling deviates from plan

---

## Notes

**Blockers:**
- None at planning time. Phase 1 is complete and hardware-validated.

**Dependencies (intra-phase):**
- Epic 2 and Epic 3 both depend on Epic 1 (the pure modules — `UsageData`, parser, formatters, state machine).
- Epic 3 (Usage UI) consumes the `UsageData` the poller (Epic 2) produces, but the screen can be built against a hand-built `UsageData` in parallel with Epic 2.
- Epic 4 depends on Epic 2 (something must produce a `UsageData` worth persisting).
- Epic 5 depends on all of the above.

**Watch items:**
- TLS root CA (Task 2.1) is the highest-risk node — do it first.
- Heap discipline around `WiFiClientSecure` — free it every poll; the 24 h test (5.3) is the proof.
- ArduinoJson is **v6** — `StaticJsonDocument`, not v7's `JsonDocument`.
- All builds: `arduino-cli compile --profile m5clawd` (or `./flash.sh`).

**Carried from Phase 1 known issues:**
- If the one-off post-provisioning power-cycle issue recurs, harden `station_connect()` (more retries / a pre-connect delay) — fits naturally in Task 4.2.
