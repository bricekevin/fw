# Phase 2 - Anthropic Poller + Usage UI Tasks

**Status:** 18/20 complete — code-complete; only hardware smoke + soak (5.2, 5.3) remain
**Updated:** 2026-05-15

> Task count: 20 numbered tasks across 5 epics (5/4/4/2/5). The PRD and
> DEPENDENCIES docs say "16" — a planning miscount; this checklist is authoritative.

> See PHASE_PRD.md for requirements and PHASE_IMP.md for code snippets and commands.
> See HANDOFF_NOTES.md for Phase 1 context (note: all builds use `--profile m5clawd`).
> Checkbox legend: `[ ]` pending, `[~]` in-progress, `[x]` done.

---

## Epic 1: Pure-logic foundation (host-testable)

> Build the Arduino-header-free modules first — they have no hardware dependency,
> they are the unit-test surface, and the poller + UI both consume them. Per
> docs/4_QUALITY_ASSURANCE.md, these stay free of `M5Stack.h`/`WiFi`/`Arduino.h`
> so host `g++` can compile them.

- [x] **1.1 Extract `UsageData` into a pure header** (`usage_data.h`)
  - [x] Moved the `UsageData` struct + `Status` enum out of `config.h` into a new `usage_data.h` that includes only `<stdint.h>`
  - [x] `config.h` `#include "usage_data.h"`; device build confirmed clean (`arduino-cli compile --profile m5clawd`)
  - [x] Added a `bool stale` field so the UI can flag last-known-good data; struct comments updated

- [x] **1.2 Rate-limit header parser** (`parse_headers.{h,cpp}`)
  - [x] `parse_anthropic_headers(...)` — takes the four header values as `const char*`, populates `UsageData` utilization + reset fields
  - [x] `parse_utilization()` — 0..100, clamped; fraction-vs-percent heuristic via the decimal-point guard
  - [x] `parse_reset()` — relative seconds, or absolute Unix timestamp resolved against `now_unix`; missing/bad -> 0, status -> `UNKNOWN`
  - [x] No Arduino headers — `<cstdint>`/`<cstdlib>`/`<cstring>`/`<cmath>` only
  - Note: header value FORMATS are assumptions documented in `parse_headers.h` — confirm against a live response in Task 2.3

- [x] **1.3 Formatting helpers** (`format_helpers.{h,cpp}`)
  - [x] `format_reset_countdown(uint32_t seconds)` -> `"2h 14m"` / `"4d 7h"` / `"<1m"`
  - [x] `format_relative_time(uint32_t seconds_ago)` -> `"42s ago"` / `"3m ago"` / `"now"` for the last-poll stamp
  - [x] Arduino-header-free; returns `std::string`

- [x] **1.4 Poll-state machine** (`state_machine.{h,cpp}`)
  - [x] `sm_advance()` maps a `PollOutcome` to the next `UsageData::Status`
  - [x] `sm_next_delay_s(st, base, cap)` — exponential backoff (60 -> 120 -> 240 -> 300 cap); base on success. Backoff bounds passed in, not hardcoded, so the module stays config-free and host-testable
  - [x] Tracks a saturating consecutive-failure count; `sm_data_is_stale()` exposes the stale flag

- [x] **1.5 Host unit-test harness** (`test/run.sh` + `test/*_test.cpp`)
  - [x] `test/run.sh` host-compiles each `*_test.cpp` with `g++ -std=c++17 -Wall -Wextra` against the pure sources and runs it
  - [x] Suites for `parse_headers` (52 checks), `format_helpers` (22), `state_machine` (28) — 102 checks, all passing
  - [x] `run.sh` exits non-zero on any suite failure
  - Note: `secret_redactor()` is NOT host-tested — it lives in `secrets_store.ino` and takes an Arduino `String`, so it cannot compile under host `g++` without refactoring Phase 1 code. It is a one-line check (`startsWith("sk-ant-")`) and was verified on-device during the Phase 1 portal smoke test. Covered by hardware tests, not the host harness.
  - Coverage: the three pure modules are exercised across all branches (fraction/percent/clamp/invalid paths, every countdown bucket, every state transition + backoff + saturation + null-safety) — well above the 60% target

---

## Epic 2: Anthropic HTTPS poller

> Per docs/2_ARCHITECTURE.md "Poller". TLS to `api.anthropic.com` is the
> headline risk — do Task 2.1 before writing poll logic.

- [x] **2.1 Validate TLS to `api.anthropic.com` + bundle root CA** (`certs.h`)
  - [x] Probed the served chain (`openssl s_client`): `leaf -> WE1 -> GTS Root R4` — **Google Trust Services**, not the Amazon/ISRG roots the PRD assumed
  - [x] Bundled the self-signed GTS Root R4 (valid until 2036) as a PEM literal in `certs.h` — the server sends a cross-signed R4 (expires 2028); pinning the self-signed root validates the same chain but lasts far longer
  - [x] Verified end-to-end on the host: `openssl s_client ... -CAfile gtsr4.pem` against the live host returns `Verify return code: 0 (ok)`
  - [x] No `setInsecure()` — the poller always validates against the pinned root
  - Note: the PRD's two-root plan was insurance against *guessing*; the live probe removed the guesswork, so one correctly identified root replaces two. On-device handshake confirmation happens when the firmware is flashed (smoke Task 5.2).

- [x] **2.2 Build and send the poll request** (`poller.ino`)
  - [x] New `poller.ino` tab; `poller_poll()` opens a stack-scoped `WiFiClientSecure` + `HTTPClient` (freed every poll — heap discipline)
  - [x] POSTs `ANTHROPIC_MESSAGES_URL` with `x-api-key` / `anthropic-version` / `content-type` headers
  - [x] Body is the minimal `claude-haiku-4-5`, `max_tokens:1` request
  - [x] `http.end()` + `client.stop()` on every path; logs `[poll] code=.. t=..ms heap=.. key=<redacted>` via `secret_redactor()`

- [x] **2.3 Extract headers + detect body errors -> `UsageData`** (`poller.ino`)
  - [x] `collectHeaders()` for the four rate-limit headers; first-poll dump logs their resolved values to confirm names on hardware
  - [x] Feeds header values into `parse_anthropic_headers()` with an NTP-derived epoch
  - [x] On non-2xx, parses the body with ArduinoJson 6 (`StaticJsonDocument<512>`) -> maps HTTP code + `error.type` to `AUTH_FAIL` / `RATE_LIMITED` / `API_UNREACHABLE`
  - [x] `last_poll_unix` set from `poller_time_now()` on success
  - **Header names + formats CONFIRMED** against the Clawdmeter daemon source (`Clawdmeter/daemon/claude_usage_daemon.py`): the windows are named **`7d`** (not `week` — the architecture doc + PRD were wrong); utilization is a **0.0..1.0 fraction**; reset is an **absolute Unix timestamp**. `parse_headers.{h,cpp}` + tests were revised to match (Epic 1 had assumed otherwise). The epoch-timestamp reset means the device needs wall-clock time — see NTP below.

- [x] **2.4 Periodic poll loop + backoff** (`m5clawd.ino`)
  - [x] First poll fires immediately once `loop()` runs (`nextPollAtMs` set in `setup()`)
  - [x] `do_poll()` polls when the backoff-adjusted interval elapses; the poll itself blocks for its TLS round-trip, but `loop()`/buttons run normally between polls
  - [x] Each outcome goes through the Epic 1 state machine; `sm_next_delay_s()` drives the backoff
  - [x] WiFi loss -> `POLL_WIFI_DOWN`; `WiFi.setAutoReconnect(true)` + the retry loop restore the link
  - [x] **NTP added** — `poller_begin()` calls `configTime()`; `poller_time_now()` returns the epoch (0 until synced). Required because reset headers are absolute timestamps. (PRD had NTP as an open question — the confirmed header format makes it mandatory.)

**Build:** `arduino-cli compile --profile m5clawd` clean, no warnings (sketch 85% of flash — TLS/mbedTLS is heavy; ~189 KB headroom left for Epic 3).

---

## Epic 3: Usage screen + status overlays

> Per docs/3_DESIGN_SYSTEM.md sections "2. Usage" and "State Indicators".

- [x] **3.1 Usage screen layout** (`ui.ino`)
  - [x] `ui_show_usage(const UsageData&)` — status bar + SESSION (5h) + WEEKLY (7d) cards
  - [x] Big % values in font 7 (`%` sign in `FSSB18` beside it); titles in `FSS9`; countdowns in `FSS9`; all colors from `config.h`
  - [x] Status bar: status badge, "updated Xs ago" (from `format_relative_time`), free-heap KB

- [x] **3.2 Partial redraw of changing fields** (`ui.ino`)
  - [x] `ui_update_usage(const UsageData&)` redraws only the % numbers, countdowns, and badge — titles/background stay put
  - [x] Per-field static cache; a field whose value is unchanged is skipped (a status change forces value/reset redraw, since it flips stale/UNKNOWN rendering)
  - [x] Full repaint only via `ui_show_usage()` on a screen switch into Usage

- [x] **3.3 Status overlays / badges** (`ui.ino`)
  - [x] Distinct badge per `Status` — `connecting` / `wifi down` / `api error` / `auth failed` / `rate limited` / `poll ok`
  - [x] Stale data keeps the last-good numbers (dimmed to `COLOR_TEXT_DIM`); `UNKNOWN` (no poll yet) shows `--` placeholders, never blanks
  - [x] Badge is text + color, never color alone (accessibility rule)
  - Note: badge wording uses readable phrases (`auth failed`) rather than the design doc's terse `auth!` glyphs — same intent, clearer on the 320 px panel. Minor, deliberate deviation.

- [x] **3.4 Wire buttons into the 3-screen cycle** (`m5clawd.ino`)
  - [x] `Screen` is `SCREEN_SPLASH` / `SCREEN_USAGE` / `SCREEN_STATUS`; button A cycles all three (`% SCREEN_COUNT`)
  - [x] Button B triggers an immediate poll (`nextPollAtMs = millis()`)
  - [x] Boot defaults to `SCREEN_USAGE`; first good poll after a failed boot-connect surfaces it; auto-jumps to `SCREEN_STATUS` after 3 consecutive failures

**Build:** `arduino-cli compile --profile m5clawd` clean, no warnings (sketch 86% of flash). Host tests still green (100 checks).
**Not verified:** the rendered screen — no hardware in this session. Needs a flash + photo (`/5_visual` / smoke Task 5.2). Layout y-offsets in `ui.ino` (`USG_*`) are hand-tuned and may need adjustment against a photo.

---

## Epic 4: Resilience & persistence

- [x] **4.1 Last-known-good `UsageData` in NVS** (`usage_store.ino`)
  - [x] On a successful poll, the whole `UsageData` is written as one `putBytes()` blob — but only when a percentage changed since the last save, to avoid needless NVS write wear
  - [x] On boot, `usage_store_load()` restores it; `setup()` flags it stale + `status=UNKNOWN` so the screen shows the last numbers (dimmed) instead of "--"
  - [x] New NVS key `NVS_KEY_LKG_USAGE` in `config.h`; `getBytes` size-check discards a stale blob if `UsageData` ever changes
  - Note: put in a new `usage_store.ino` tab (not `secrets_store.ino`) — the usage snapshot is not a secret. `secrets_reset()`'s `prefs.clear()` still wipes it, as intended.

- [x] **4.2 Heap watchdog + WiFi reconnect hardening** (`m5clawd.ino`)
  - [x] Each poll logs `heap=` and warns (`[warn] low heap`) below `POLL_HEAP_FLOOR` (90 KB)
  - [x] On `POLL_WIFI_DOWN` the poll loop nudges `WiFi.reconnect()` (non-blocking) on top of the existing `setAutoReconnect(true)`
  - [ ] No-leak confirmation across ~30 poll cycles — **hardware item**, rolls into the 24 h uptime run (Task 5.3); cannot be checked without a device

**Build:** `arduino-cli compile --profile m5clawd` clean, no warnings (sketch 86% of flash). Host tests green (100 checks).

---

## Epic 5: Testing & documentation

- [x] **5.1 Host unit tests pass** — `m5clawd/test/run.sh` green: 100 checks across `parse_headers` / `format_helpers` / `state_machine`, all branches exercised (well above the 60% target)
- [ ] **5.2 Hardware smoke tests** (docs/4_QUALITY_ASSURANCE.md scenarios 2, 3) — **BLOCKED: needs hardware**
  - [ ] Scenario 2 (happy path): valid key -> Usage screen shows real numbers within 90 s
  - [ ] Scenario 3 (WiFi drop): kill the router -> stale badge within one interval, numbers persist; restore -> recovers within two intervals
  - [ ] Bad-key path: confirm the `auth failed` badge on a rejected key
  - [ ] Capture serial logs (`[poll] code=200 ...`, the first-poll header dump) and a photo of the Usage screen for the PR
- [ ] **5.3 Performance checks** — single-poll latency < 5 s p95; 24 h uptime/heap run (covers the Task 4.2 no-leak check) — **BLOCKED: needs hardware**
- [x] **5.4 CI workflow** — `.github/workflows/ci.yml` added: `arduino-cli compile --profile m5clawd` + `test/run.sh` + API-key secret scan, on push/PR. (A GitHub remote exists — `bricekevin/theClaw` — so this task was in scope.)
- [x] **5.5 Update documentation** — HANDOFF_NOTES session entry; this checklist; `CHANGELOG.md` created (Phase 1 + Phase 2); `2_ARCHITECTURE.md` rate-limit header names corrected `-week-` -> `-7d-`. No new ADR: the TLS-root and header-name findings are confirmations/corrections of implementation detail, fully documented in `certs.h` / `parse_headers.h` / HANDOFF — not architecture-decision reversals. `PHASE_PRD.md` still references the assumed Amazon/ISRG roots and is left for `/6_doc`.

**Phase 2 status: code-complete (18/20).** Every task that does not require the physical device is done — firmware compiles clean on the pinned toolchain and the host test suite passes. Tasks 5.2 and 5.3 are the remaining work and need an M5Stack on USB: flash, watch serial, photograph the screen, run the 24 h soak.

---

## Notes

**Blockers:**
- Tasks 5.2 (hardware smoke tests) and 5.3 (latency + 24 h soak) need a
  physical M5Stack Core on USB. All code-side work is done; these are the only
  remaining tasks. Flash with `cd m5clawd && ./flash.sh`, watch serial at
  115200, and confirm `[poll] code=200` + the first-poll header dump.

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
