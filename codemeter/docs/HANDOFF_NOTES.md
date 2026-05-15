# Handoff Notes

**Project:** M5Clawd
**Last Updated:** 2026-05-15

> This document tracks work sessions, changes, and context for continuity between work sessions or AI agent handoffs.

---

## Session 4 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1.5 h
**Focus:** `/3_dev` Phase 2 Epic 2 — the Anthropic HTTPS poller, TLS, and the poll loop.

### What happened

Implemented Epic 2 (4 tasks) in worktree `../theClaw-phase-2-20260515`. The device now polls `api.anthropic.com` over validated TLS, parses the rate-limit headers into `UsageData`, and runs a backoff-driven poll loop. Two live probes during the work overturned assumptions baked into the foundation docs — see Decisions.

### Completed (Phase 2 Epic 2 — 9/16 tasks total)

- [x] **2.1** `certs.h` — TLS root CA. Probed the live chain; bundled the self-signed GTS Root R4.
- [x] **2.2** `poller.ino` — `WiFiClientSecure` + `HTTPClient` POST, stack-scoped for heap safety.
- [x] **2.3** rate-limit header extraction + ArduinoJson 6 error-body classification.
- [x] **2.4** `m5clawd.ino` — `do_poll()` loop, exponential backoff, NTP, WiFi auto-reconnect.

### Decisions made (IMPORTANT — these overturn earlier doc assumptions)

- **TLS chain is Google Trust Services, not Amazon/ISRG.** A live `openssl s_client` probe showed `api.anthropic.com` chains `leaf -> WE1 -> GTS Root R4`. The PRD/risk table assumed Amazon Root CA 1 / ISRG Root X1 — wrong. `certs.h` pins the **self-signed GTS Root R4** (valid to 2036). The server actually sends a *cross-signed* R4 (expires 2028); the self-signed root — same key — validates the identical chain and lasts longer. Verified: `openssl s_client ... -CAfile gtsr4.pem` returns `Verify return code: 0 (ok)`.
- **Rate-limit header names use `7d`, not `week`.** Confirmed against the Clawdmeter daemon source (`Clawdmeter/daemon/claude_usage_daemon.py`): `anthropic-ratelimit-unified-7d-utilization` / `-7d-reset`. **`docs/2_ARCHITECTURE.md` and `docs/Phase 2/PHASE_PRD.md` both say `-week-` — they are now factually wrong** and should be corrected (see Known Issues).
- **Header value formats confirmed -> NTP is mandatory.** The daemon shows utilization is a `0.0..1.0` fraction (`pct = round(value*100)`) and reset is an absolute Unix timestamp (`(reset - now)`). `parse_headers.{h,cpp}` + its tests were revised from the Epic 1 assumptions to match. Because reset is an absolute timestamp, the device must know wall-clock time — `poller_begin()` now starts NTP (`configTime`), and `poller_time_now()` returns 0 until it syncs (reset countdowns read 0 on the very first poll, correct thereafter). The PRD had NTP as an open question; the confirmed format makes it required.

### Files Changed

```text
m5clawd/certs.h            — new: pinned GTS Root R4 PEM + provenance notes
m5clawd/poller.ino         — new: TLS poll, header parse, error classify, NTP
m5clawd/m5clawd.ino        — poll loop (do_poll), backoff, auto-reconnect,
                             g_usage/g_pollState/g_apiKey globals; lib includes
m5clawd/config.h           — includes state_machine.h; poller prototypes;
                             POLL_HTTP_TIMEOUT_MS + NTP_SERVER_* constants
m5clawd/parse_headers.{h,cpp} — revised to confirmed formats (fraction / epoch)
m5clawd/test/parse_headers_test.cpp — tests updated to the confirmed formats
docs/Phase 2/PHASE_TASKS.md — Epic 2 checked off
```

### Verification

- `arduino-cli compile --profile m5clawd` — clean, no warnings. Sketch now **85%** of flash (was 71%; +14% is mbedTLS/WiFiClientSecure/HTTPClient). ~189 KB headroom remains for Epic 3.
- `m5clawd/test/run.sh` — 100 checks across 3 suites, all pass.
- TLS root verified against the live host with `openssl` (see Decisions).
- **Not yet verified on hardware:** the actual on-device TLS handshake, a real poll, and the first-poll header-name dump. Needs a flash (smoke Task 5.2).

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| `2_ARCHITECTURE.md` + `PHASE_PRD.md` say `-week-` rate-limit headers | MED | Docs contradict the shipped code (`-7d-`) | Correct both during `/6_doc`. The code/`PHASE_TASKS.md` are authoritative. |
| On-device TLS/poll unverified | MED | Poller is compile-clean + host-probed but never run on the device | Flash and watch serial for `[poll] code=200` and the first-poll header dump. If headers read `(empty)`, the names need another look. |
| Sketch at 85% of flash | LOW | Headroom shrinking | Fine for Epic 3 (UI draw code is small). Watch it; an OTA partition scheme may need thought before Phase 3 art. |

### Next Session Should

1. Continue `/3_dev` Phase 2 with **Epic 3** — the Usage screen + status overlays (`ui.ino`). It consumes `g_usage`; the Epic 1 formatters (`format_helpers`) are ready and unused so far.
2. Then **Epic 4** — last-known-good `UsageData` persistence to NVS + heap/reconnect hardening.
3. When hardware is available, flash and run smoke Task 5.2 — this is the first real on-device TLS/poll test.

---

## Session 3 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/2_pm` Phase 2 planning, then `/3_dev` Phase 2 Epic 1 — the pure-logic foundation for the Anthropic poller.

### What happened

Planned Phase 2 (Anthropic poller + Usage UI; 16 tasks, 5 epics — see `docs/Phase 2/`), then implemented Epic 1 in worktree `../theClaw-phase-2-20260515` (branch `phase-2-20260515`). Epic 1 is the Arduino-header-free module layer the poller and the Usage UI will both consume; all of it is host-testable without an ESP32 toolchain.

### Completed (Phase 2 Epic 1 — 5/16 tasks)

- [x] **1.1** `UsageData` struct + `Status` enum moved out of `config.h` into a new pure `usage_data.h` (`<stdint.h>` only); added a `bool stale` field for last-known-good display. `config.h` includes it.
- [x] **1.2** `parse_headers.{h,cpp}` — `parse_anthropic_headers()` parses the four `anthropic-ratelimit-unified-*` header values into `UsageData`.
- [x] **1.3** `format_helpers.{h,cpp}` — `format_reset_countdown()` and `format_relative_time()`.
- [x] **1.4** `state_machine.{h,cpp}` — poll-outcome -> `Status` mapping + exponential backoff.
- [x] **1.5** `test/` host unit-test harness — `run.sh` + three suites, 102 checks, all passing.

### Files Changed

```text
m5clawd/usage_data.h        — new: pure UsageData struct (was inline in config.h)
m5clawd/config.h            — UsageData removed; #include "usage_data.h" added
m5clawd/parse_headers.{h,cpp} — new: rate-limit header parser
m5clawd/format_helpers.{h,cpp} — new: countdown / relative-time formatters
m5clawd/state_machine.{h,cpp}  — new: poll-state machine + backoff
m5clawd/test/{run.sh,test_util.h,*_test.cpp} — new: host g++ unit tests
docs/Phase 2/               — new: PRD, TASKS, IMP, DEPENDENCIES (from /2_pm)
```

### Decisions made

- **`UsageData` extracted to its own pure header.** The pure-logic modules cannot include `config.h` (it pulls in `<Arduino.h>`). `usage_data.h` is `<stdint.h>`-only so host `g++` compiles it. `config.h` includes it — no behavior change for the device build.
- **Backoff bounds are function parameters, not hardcoded.** `sm_next_delay_s(st, base_s, cap_s)` takes the bounds so `state_machine.cpp` stays free of `config.h`; the firmware passes `POLL_INTERVAL_S` / `POLL_BACKOFF_MAX_S`, the tests pass literals.
- **Header value FORMATS are documented assumptions, not confirmed.** `parse_headers.h` spells out the assumed utilization (fraction vs percent) and reset (relative seconds vs absolute epoch) formats. These MUST be confirmed against a real `200` response in Task 2.3 — the parser is built to be revised there.
- **`secret_redactor()` is not in the host harness.** It lives in `secrets_store.ino` and takes an Arduino `String`; host-testing it would mean refactoring Phase 1 code for a one-line function already verified on-device. Left device-side. (QA doc lists it as a unit-test target — this is a deliberate, documented deviation.)

### Verification

- `m5clawd/test/run.sh` — 102 checks across 3 suites, all pass.
- `arduino-cli compile --profile m5clawd` — clean, exit 0 (sketch 71% of flash). The pure `.cpp` modules compile into the firmware; the `test/` subdir is not picked up by arduino-cli, as intended.

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| Rate-limit header value formats unconfirmed | MED | `parse_headers` may need adjustment | Names + formats are assumptions. Task 2.3 dumps all headers from a live response; revise `parse_headers.{h,cpp}` + its tests then. |

### Next Session Should

1. Continue `/3_dev` Phase 2 with **Epic 2** — the Anthropic HTTPS poller.
2. **Do Task 2.1 first** (TLS root CA validation) — it is the phase's highest-risk node. Probe `api.anthropic.com`'s served chain with `openssl s_client` before writing poll code.
3. Epic 3 (Usage UI) can be worked in parallel with Epic 2 — it only needs Epic 1's `UsageData` + formatters, not a live poll.

---

## Session 2 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1.5 h
**Focus:** `/3_dev` Phase 1 — toolchain validation, copy-strip, captive portal, screens, reset gesture. Firmware compiles clean and is validated on hardware; Phase 1 complete.

### What happened

Worked Phase 1 Epics 1–4 in worktree `../theClaw-phase-1-20260515`. The crypto ticker (4296-line `.ino`) was copied in and reduced to a 549-line, 4-tab skeleton that compiles clean against the pinned toolchain.

### Toolchain finding (important — read before next build)

`arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32` **fails** on this Mac. The global sketchbook (`~/Documents/Arduino/libraries`) carries a generic Arduino `SD` library (`architectures=*`) that shadows the ESP32 core's own `SD(esp32)`. Because `M5Stack.h` force-includes `<SD.h>`, the broken SD is picked and the build dies with "Architecture or board not supported".

**Fix:** added `m5clawd/sketch.yaml` — an arduino-cli build profile named `m5clawd`. Building with `--profile m5clawd` resolves libraries only from the profile + platform-bundled set, ignoring the global sketchbook. `flash.sh` was updated to use `--profile`. **All builds must use `arduino-cli compile --profile m5clawd`** (or `./flash.sh`), not a bare `--fqbn` invocation. This is *not* an ADR 001 change — ESP32 core 1.0.4 itself installs and compiles fine; the issue was library shadowing. ESP32 core 1.0.4 + M5Stack 0.3.1 + ArduinoJson 6.17.3 all confirmed working.

Note: `setup-toolchain.sh` downgraded the globally-installed M5Stack (0.4.6 -> 0.3.1) and ArduinoJson (7.4.2 -> 6.17.3) to the pinned versions. These are global; the build profile pins them anyway, but other sketches on this Mac now see the older versions.

### Decisions made

- **Build profile over bare FQBN** — `sketch.yaml` profile `m5clawd`; isolates from the global sketchbook and pins exact versions. `flash.sh` updated.
- **`startConfigPortal()` not `autoConnect()`** for provisioning — so the portal opens even when WiFi creds already exist (needed for re-onboarding after a rejected key). `wifi_portal_begin()` loops the portal until a valid key is saved, then `ESP.restart()`s into station mode.
- **Two-stage reset gesture** — hold C 5 s -> confirm screen; +2 s more -> wipe. Release early aborts.

### Completed

- [x] **1.1** toolchain validated; throwaway `M5.begin()` sketch compiles
- [x] **2.1–2.4** copy-strip: 4296-line crypto ticker -> 549-line skeleton across `m5clawd.ino` / `wifi_portal.ino` / `secrets_store.ino` / `ui.ino`; compiles clean
- [x] **3.1–3.3** captive portal with one Anthropic API-key field; NVS persistence; redactor; provisioning screen with QR
- [x] **4.1–4.3** boot-mode selection, splash/connecting/status/error screens, button-A cycling, button-C reset gesture + backlight toggle
- [x] **1.2** flashed to the M5Stack Core; serial round-trip confirmed at 115200
- [x] **5.1** all four hardware smoke scenarios pass (see Hardware validation below)
- [x] **5.2** docs updated (this entry, PHASE_TASKS)

### Files Changed

```text
m5clawd/m5clawd.ino        — REPLACED: crypto ticker -> stripped skeleton (globals, setup, loop, buttons)
m5clawd/wifi_portal.ino    — new: WiFiManager glue, one API-key param
m5clawd/secrets_store.ino  — new: NVS persistence + secret_redactor
m5clawd/ui.ino             — new: text-only screens (splash/connecting/status/provisioning/error/reset)
m5clawd/config.h           — added cross-tab function prototypes + <Arduino.h>
m5clawd/sketch.yaml        — new: arduino-cli build profile (the SD-shadowing fix)
m5clawd/flash.sh           — updated to build with --profile m5clawd
docs/Phase 1/PHASE_TASKS.md — checkboxes updated
```

### Hardware validation (same session)

Flashed and smoke-tested on the physical M5Stack Core. Device port: `/dev/cu.usbserial-02132522` (CP2104, plug-and-play on macOS — no driver step). All four Task 5.1 scenarios pass:

- **Fresh flash -> portal**: AP `M5Clawd-0D0A10` raised, provisioning screen renders (CONFIGURE WIFI + SSID + QR).
- **Valid onboarding**: a malformed key was correctly rejected and the portal reopened; a valid key was accepted, persisted, and the device auto-rebooted into station mode — serial showed `[boot] configured -> station mode` / `[wifi] connected, IP 192.168.0.154`; status screen renders.
- **Reflash -> creds survive**: a normal (non-erase) reupload kept NVS; device booted straight to the status screen.
- **Reset gesture**: long-press C wiped NVS and returned to the provisioning screen.

Serial-capture note: `arduino-cli monitor` exits immediately when run as a detached background process (no stdin/TTY). The pattern that works is a foreground call — `arduino-cli monitor ... >log & MPID=$!; sleep N; kill $MPID` — and it only captures while the device is actively emitting (i.e. catch a reset/boot inside the window).

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| First post-provisioning boot once needed a manual power-cycle to connect | LOW | One-off; did not reproduce on retest | Likely a flaky first WiFi connect right after the AP/STA teardown. If it recurs, revisit `station_connect()` — the 30 s timeout + single retry may need more retries or a short pre-connect delay. |
| `M5.Lcd.qrcode()` scannability not formally verified | LOW | QR may need version/width tuning | QR renders on the provisioning screen; not test-scanned. Params: text, x, y, width=100, version=4. |
| Global libs downgraded | LOW | Other sketches on this Mac see M5Stack 0.3.1 / ArduinoJson 6.17.3 | Intended (matches ADR 001 pins); the crypto ticker also wants these versions. |

### Next Session Should

1. Phase 1 is **complete and hardware-validated** -> run `/2_pm` for Phase 2 (the Anthropic HTTPS poller + the Usage screen).
2. Optionally photograph the device screens for the PR record.
3. If the one-off power-cycle issue recurs, harden `station_connect()`.

---

## Session 1 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~40 min
**Focus:** Continuation of `/1_start` — user surfaced a second reference project; revised the tech-stack decisions and doc set to match it.

### What happened

The user pointed at `~/GIT/Crypto_Coin_TickerUS_Stock` — a **working M5Stack Core ticker that already uses WiFiManager the way the user wants**. They chose to start M5Clawd as a **copy-and-strip of that project**, and left the build-toolchain choice to the agent.

This invalidated the original (Session 0) tech-stack plan, which had assumed PlatformIO + M5Unified + ArduinoJson 7. The crypto ticker is Arduino IDE + classic `M5Stack.h` + ArduinoJson 6.17.3 — a proven toolchain on the exact hardware.

### Decisions made

- **Toolchain:** Arduino sketch built via **`arduino-cli`** (scriptable, but stays Arduino-IDE-compatible). Overrides ADR 001's PlatformIO choice.
- **HAL:** classic `M5Stack.h` (lib 0.3.1), not M5Unified. `M5.Lcd.*` drawing, no LVGL/M5GFX.
- **JSON:** ArduinoJson 6.17.3 (match the reference; v6 API).
- **Starting point:** `m5clawd/` will be a copy-and-strip of the crypto ticker's canonical sketch — keep WiFiManager onboarding / M5 display / buttons / `Preferences`; strip Binance WebSockets, charts, 25-pair management, SHT3X sensor, NeoPixel, SD-card config, M5StackUpdater, Timezone.
- **References:** there are now **two** read-only reference projects — `Clawdmeter/` (concept) and `Crypto_Coin_TickerUS_Stock` (scaffolding source). Neither is ever edited.

### Completed

- [x] Explored the crypto ticker; extracted its WiFiManager pattern (`getParam`, `saveParamCallback`, `onConfigModeCallback`, `WiFiEvent`, `autoConnect`, dark theme)
- [x] **Revised ADR 001** — Arduino / `arduino-cli` / `M5Stack.h` / ArduinoJson 6 (original PlatformIO plan preserved in a revision note)
- [x] **Added ADR 006** — copy-and-strip the crypto ticker
- [x] **Revised ADR 004** — now covers both read-only reference projects
- [x] **Rewrote** `2_ARCHITECTURE.md`, `CLAUDE.md`, `README.md` for the Arduino toolchain + project layout (`.ino` tabs, not `src/*.cpp`)
- [x] **Patched** `3_DESIGN_SYSTEM.md` (M5.Lcd fonts), `4_QUALITY_ASSURANCE.md` (arduino-cli + host-`g++` tests), `1_PROJECT_OVERVIEW.md` (tech mentions)
- [x] **`git init`** at `/Users/kevinbrice/GIT/theClaw/`; added `.gitignore`; committed the foundation docs
- [x] **Scaffolded `m5clawd/`** — copied the vendored WiFiManager files from the crypto ticker (`WiFiManager.cpp/.h`, `wm_*.h`, `strings_en.h`); wrote `config.h` (constants + `UsageData` struct), `setup-toolchain.sh` (arduino-cli core/lib install), `flash.sh` (compile + upload). Committed.

### Files Changed

```text
Docs (revised this session)
  ├── docs/decisions/001-tech-stack-selection.md   — full rewrite (Arduino toolchain)
  ├── docs/decisions/004-port-vs-fork.md           — revised (two references)
  ├── docs/decisions/006-start-from-crypto-ticker.md — new
  ├── docs/decisions/000-index.md                  — added ADR 006, revision dates
  ├── docs/2_ARCHITECTURE.md                       — full rewrite
  ├── docs/3_DESIGN_SYSTEM.md                      — typography + asset pipeline
  ├── docs/4_QUALITY_ASSURANCE.md                  — build/test commands
  ├── docs/1_PROJECT_OVERVIEW.md                   — tech mentions, risk row
  ├── CLAUDE.md                                    — full rewrite
  └── README.md                                    — full rewrite

Scaffolding (new this session)
  ├── .gitignore                                   — new
  ├── m5clawd/config.h                             — new (constants + UsageData)
  ├── m5clawd/setup-toolchain.sh                    — new (arduino-cli install)
  ├── m5clawd/flash.sh                              — new (compile + upload)
  └── m5clawd/{WiFiManager.cpp,.h,wm_*.h,strings_en.h} — copied from the crypto ticker
```

### Known Issues

| Issue                                              | Severity | Impact                                                              | Notes                                                                       |
| --------------------------------------------------- | -------- | ------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `m5clawd/m5clawd.ino` does not exist yet           | n/a      | The folder is not a valid Arduino sketch until the copy-strip runs   | Expected — scaffolding only. The sketch is Phase 1 task 1 (copy-strip, ADR 006). `flash.sh` errors clearly until then. |
| `arduino-cli` 1.2.2 + ESP32 core 1.0.4 unverified  | MED      | A modern arduino-cli may refuse to build against the 2020-era core 1.0.4 | **Validate first thing in Phase 1.** If it fails: revise ADR 001 + `setup-toolchain.sh` rather than silently bumping the core. |
| Crypto ticker has no license file                 | LOW      | M5Clawd's license can't be finalized until provenance is clear      | The reused glue is the user's own code, so likely fine; confirm pre-release  |

### Follow-Up Required

- [x] `git init` at `/Users/kevinbrice/GIT/theClaw/` and commit the foundation docs — done this session
- [ ] Run `/2_pm "Phase 1: copy-strip the crypto ticker + captive portal"` — Priority: HIGH (next step)
- [ ] Phase 1 task 1: copy the crypto ticker's canonical sketch into `m5clawd/m5clawd.ino`, split into `.ino` tabs, strip per ADR 006, get a clean `arduino-cli compile` — Priority: HIGH
- [ ] Validate `arduino-cli` + ESP32 core 1.0.4 by running `m5clawd/setup-toolchain.sh` on the dev Mac — Priority: HIGH
- [ ] Confirm exact M5Stack Core revision (Micro-USB vs USB-C) — Priority: LOW (M5Stack.h handles it)

### Context Notes

**Why the toolchain flipped:** Session 0 designed in the abstract (PlatformIO is "cleaner"). Session 1 had a concrete, working, on-hardware reference. Matching the thing we're copying beats an abstractly-nicer toolchain — copying a project and *then* converting its build system would be churn for no gain. `arduino-cli` recovers the scriptability that made PlatformIO attractive.

**The crypto ticker's WiFiManager is the asset.** The user explicitly likes its onboarding UX. The single most valuable thing we're inheriting is `wifi_portal.ino`'s pattern. Don't re-derive it.

### Next Session Should

1. Run `/2_pm "Phase 1: copy-strip the crypto ticker + captive portal"` to break Phase 1 into tasks.
2. Begin `/3_dev`: validate the toolchain (`m5clawd/setup-toolchain.sh`), then copy the crypto ticker's canonical sketch into `m5clawd/m5clawd.ino`, split into `.ino` tabs, strip per ADR 006, get a clean `arduino-cli compile`.

---

## Session 0 — 2026-05-14

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~45 min
**Focus:** Project kickoff (`/1_start`) — discovery, architecture decisions, foundation docs.

### Completed

- [x] Cloned upstream reference `HermannBjorgvin/Clawdmeter` into `Clawdmeter/`
- [x] Identified two major mismatches with the user's stated goal:
      (1) no `wifimanager` branch in upstream — turned out to mean **the tzapu/WiFiManager library**, not a branch;
      (2) upstream targets **Waveshare ESP32-S3-Touch-AMOLED-2.16**, not M5Stack Core Basic
- [x] Confirmed direction with user: full port to M5Stack Core Basic + WiFi-direct (no host daemon) + captive portal
- [x] Created 5 ADRs documenting the decisions
- [x] Generated full doc set: `CLAUDE.md`, `README.md`, `docs/1_PROJECT_OVERVIEW.md`, `docs/2_ARCHITECTURE.md`, `docs/3_DESIGN_SYSTEM.md`, `docs/4_QUALITY_ASSURANCE.md`, this file

### Files Changed

```text
Docs
  ├── /Users/kevinbrice/GIT/theClaw/CLAUDE.md                       — new (replaces starterPack's own)
  ├── /Users/kevinbrice/GIT/theClaw/README.md                        — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/1_PROJECT_OVERVIEW.md       — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/2_ARCHITECTURE.md           — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/3_DESIGN_SYSTEM.md          — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/4_QUALITY_ASSURANCE.md      — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/HANDOFF_NOTES.md            — new (this file)
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/000-index.md      — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/001-tech-stack-selection.md       — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/002-wifi-vs-ble-daemon.md          — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/003-onboarding-via-wifimanager.md  — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/004-port-vs-fork.md                — new
  └── /Users/kevinbrice/GIT/theClaw/docs/decisions/005-secrets-storage-nvs.md         — new

Reference
  └── /Users/kevinbrice/GIT/theClaw/Clawdmeter/                       — read-only clone (do not edit)
```

### Upstream Reference Pin

We cloned `HermannBjorgvin/Clawdmeter` at whatever HEAD was current on 2026-05-14. To check what we have:

```bash
git -C /Users/kevinbrice/GIT/theClaw/Clawdmeter log -1 --oneline
```

If upstream meaningfully evolves and we want to re-sync the reference:

```bash
git -C /Users/kevinbrice/GIT/theClaw/Clawdmeter pull
```

### Known Issues

| Issue                                                | Severity | Impact                                                                         | Notes                                                                                                |
| ---------------------------------------------------- | -------- | ------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| `m5clawd/` directory does not yet exist              | n/a      | Phase 1 must create it                                                          | This is expected — `/1_start` only creates docs                                                       |
| User has not validated hardware revision specifics   | LOW      | If they have a `Gray` variant (with IMU) vs vanilla Basic, MPU6886 detection logic might branch | M5Unified auto-detects, so likely fine. Confirm at first flash. |
| No git repo at project root yet                      | LOW      | Can't track changes across sessions                                             | Recommend `git init` at `/Users/kevinbrice/GIT/theClaw/` before Phase 1 starts                       |

### Follow-Up Required

- [ ] `git init` at `/Users/kevinbrice/GIT/theClaw/` and commit foundation docs — Priority: HIGH
- [ ] Add `Clawdmeter/` to `.gitignore` (or accept it as a vendored reference subdir — either is fine, document the choice) — Priority: MEDIUM
- [ ] Confirm exact M5Stack Core Basic revision the user owns (USB-C vs Micro-USB, FIRE variant, etc.) before Phase 1 — Priority: MEDIUM. M5Unified handles all of these but knowing prevents surprises.
- [ ] Run `/2_pm "Phase 1: bootstrap + captive portal + first poll"` to plan Phase 1 — Priority: HIGH (next step)

### Tech Debt Created

- **Plaintext NVS storage of API key.** Known limitation — see ADR 005. Plan to harden in a future phase via NVS encryption.
- **Bundled root CA may go stale.** We'll pick a CA at Phase 2 implementation time. Plan to add an OTA path before MVP ships so we have a recovery option if Anthropic rotates roots.

### Context Notes

**Hardware mismatch surfacing:** When the user said "check out wifimanager" they meant the **library**, not a branch — language ambiguity. Upstream has no WiFi at all; everything goes over BLE to a host daemon. Caught this early, saved a wasted session.

**Original CLAUDE.md was replaced:** The previous `/Users/kevinbrice/GIT/theClaw/CLAUDE.md` described the starterPack itself (the toolkit). It is fully preserved in git history once we `git init`. The new one is for the M5Clawd project.

**Design philosophy decided:** "Easy to setup once cloned" is the cardinal UX rule. Every design call should be re-checked against that — if it adds friction at first boot, push back.

### Next Session Should

1. Confirm the M5Stack Core Basic revision (Micro-USB vs USB-C; FIRE vs Gray vs Basic).
2. Run `/2_pm "Phase 1: bootstrap + captive portal + first poll"` to break Phase 1 into tasks.
3. Then `/3_dev` to start implementing: create `m5clawd/platformio.ini`, scaffold `src/main.cpp`, etc.

---

## Template for New Sessions

```markdown
## Session [N] — [DATE]

**Agent / Developer:** [NAME]
**Duration:** [TIME_SPENT]
**Focus:** [BRIEF_SUMMARY]

### Completed
- [x] [TASK]

### Files Changed
```text
[CATEGORY]
  ├── [filepath] — [WHAT_CHANGED]
```

### Known Issues
| Issue | Severity | Impact | Notes |
|-------|----------|--------|-------|

### Follow-Up Required
- [ ] [ACTION] — Priority: [LEVEL]

### Tech Debt Created
- [DEBT] — [REASON] — [WHEN_TO_ADDRESS]

### Context Notes
[IMPORTANT_INFO]

### Next Session Should
1. [NEXT_STEP]
```

---

## Quick Reference

### File categories

- **Firmware:** `m5clawd/*.ino`, `m5clawd/*.h`
- **Build / tooling:** `m5clawd/setup-toolchain.sh`, `m5clawd/flash.sh`, `m5clawd/test/*`
- **Reference:** `Clawdmeter/*` (read-only)
- **Docs:** `docs/*`, `CLAUDE.md`, `README.md`

### Severity levels

- **CRITICAL:** Device bricks, secrets leak, can't onboard
- **HIGH:** Blocks Phase progress or causes data loss
- **MEDIUM:** Impacts functionality but has workaround
- **LOW:** Cosmetic or edge-case only

### Priority levels

- **HIGH:** Must be done next session
- **MEDIUM:** Should be done within current phase
- **LOW:** Backlog
