# Handoff Notes

**Project:** M5Clawd
**Last Updated:** 2026-05-15

> This document tracks work sessions, changes, and context for continuity between work sessions or AI agent handoffs.

---

## Session 2 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/3_dev` Phase 1 — toolchain validation, copy-strip, captive portal, screens, reset gesture. Firmware now compiles clean.

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

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| Not yet flashed to hardware | n/a | Tasks 1.2 + 5.1 (smoke tests) are unverified | Firmware compiles clean; needs the physical M5Stack + USB-C. |
| `M5.Lcd.qrcode()` render not visually verified | LOW | QR may need version/width tuning on the real LCD | API confirmed present in M5Stack 0.3.1; params: text, x, y, width=100, version=4. |
| Global libs downgraded | LOW | Other sketches on this Mac see M5Stack 0.3.1 / ArduinoJson 6.17.3 | Intended (matches ADR 001 pins); the crypto ticker also wants these versions. |

### Next Session Should

1. **Flash to hardware** (`cd m5clawd && ./flash.sh`) — completes Task 1.2.
2. Run the Task 5.1 smoke scenarios; photograph the screens for the PR.
3. Tune the provisioning-screen QR if it scans poorly.
4. If hardware is good, Phase 1 is done -> `/2_pm` for Phase 2 (the Anthropic poller).

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
