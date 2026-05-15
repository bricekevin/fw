# Project Guide for AI Agents — M5Clawd

This file is the primary briefing document for AI coding assistants working in this repo. Read this first before doing anything else.

---

## Project Overview

**M5Clawd** is a standalone, WiFi-connected desk-side display for Claude Code usage, running on **M5Stack Core Basic (Gray, ESP32)**. The device joins the user's WiFi, polls `api.anthropic.com` directly every 60 s, and shows the current 5-hour session and weekly rate-limit utilization on a 320×240 LCD. It is configured via a captive-portal flow on first boot. No host computer, no daemon, no companion app.

**Two lineages (both read-only references — see [ADR 004](docs/decisions/004-port-vs-fork.md)):**

- **Concept** comes from [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) — a Claude usage display for Waveshare ESP32-S3 AMOLED hardware that uses BLE + a host laptop daemon. We keep the *idea* and the Anthropic poll trick; none of its firmware code.
- **Codebase scaffolding** comes from the user's working `Crypto_Coin_TickerUS_Stock` project (`~/GIT/Crypto_Coin_TickerUS_Stock`) — an M5Stack Core ticker. `m5clawd/` is a copy-and-strip of it ([ADR 006](docs/decisions/006-start-from-crypto-ticker.md)): we keep its WiFiManager onboarding, M5Stack display/button code, and `Preferences` persistence; we strip the crypto/stock/chart/sensor logic.

**Tech Stack:**

- **MCU / framework:** ESP32 + Arduino-ESP32, board manager **1.0.4** (pinned)
- **Build / flash:** `arduino-cli` (the sketch stays Arduino-IDE-compatible)
- **Board HAL:** classic `M5Stack.h` (lib v0.3.1) — `M5.Lcd.*`, `M5.update()`, `M5.BtnA/B/C`
- **WiFi provisioning:** [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager), **vendored in-tree** (copied from the crypto ticker)
- **HTTP / TLS:** Arduino-ESP32 `HTTPClient` + `WiFiClientSecure`
- **JSON:** ArduinoJson **6.17.3** (v6 API — not v7)
- **Storage:** ESP32 NVS via `Preferences.h`, namespace `m5clawd`
- **Assets:** SPIFFS PNGs via `M5.Lcd.drawPngFile`
- **Infrastructure:** None — device runs standalone

**Quick Start (after Phase 1 creates `m5clawd/`):**

```bash
cd m5clawd
./setup-toolchain.sh        # one-time: installs arduino-cli + ESP32 core 1.0.4 + libs
./flash.sh                  # compile + upload firmware + SPIFFS over USB-C
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200   # serial log
```

---

## Documentation Structure

```text
./
├── CLAUDE.md                   (this file)
├── README.md                   (human-facing intro + setup walkthrough)
├── docs/
│   ├── 1_PROJECT_OVERVIEW.md   (vision, goals, MVP scope, risks)
│   ├── 2_ARCHITECTURE.md       (components, data flow, project layout, tech stack)
│   ├── 3_DESIGN_SYSTEM.md      (screen layouts, colors, M5.Lcd fonts)
│   ├── 4_QUALITY_ASSURANCE.md  (test strategy, smoke tests, quality gates)
│   ├── HANDOFF_NOTES.md        (session-by-session log — read for recent context)
│   └── decisions/              (6 ADRs documenting the key decisions)
│       ├── 000-index.md
│       ├── 001-tech-stack-selection.md       (revised: Arduino toolchain)
│       ├── 002-wifi-vs-ble-daemon.md
│       ├── 003-onboarding-via-wifimanager.md
│       ├── 004-port-vs-fork.md               (revised: two read-only references)
│       ├── 005-secrets-storage-nvs.md
│       └── 006-start-from-crypto-ticker.md   (the copy-and-strip decision)
├── Clawdmeter/                 (READ-ONLY concept reference — do not edit)
└── m5clawd/                    (THE FIRMWARE — created in Phase 1 by copy-strip)

~/GIT/Crypto_Coin_TickerUS_Stock/  (SEPARATE repo — READ-ONLY scaffolding source, never edit)
```

**Before writing any code, always check:**

- `docs/HANDOFF_NOTES.md` — what was just done, what's known broken
- `docs/2_ARCHITECTURE.md` — data flow, component boundaries, project layout
- `docs/decisions/` — the "why" behind tech choices; if a change conflicts with an ADR, write a new ADR

---

## Critical: Two Read-Only References

**Never edit `Clawdmeter/` or `Crypto_Coin_TickerUS_Stock`.** Both are references. The crypto ticker is also the user's *live device firmware* — editing it could break a device they use.

| Reference                       | Role                  | What we take                                                       |
| -------------------------------- | --------------------- | ------------------------------------------------------------------ |
| `Crypto_Coin_TickerUS_Stock`     | Scaffolding source    | Copy-and-strip into `m5clawd/`: WiFiManager flow, M5 display/buttons, `Preferences`, SPIFFS loading |
| `Clawdmeter/`                    | Concept reference     | The poll trick + exact rate-limit header names. No code reuse.     |

The crypto ticker's WiFiManager pattern — reused in `m5clawd/wifi_portal.ino`:

- `WiFiManager wifiManager;` global + `WiFiManagerParameter` custom fields
- `getParam(name)` — reads a field from `wifiManager.server->arg(name)`
- `saveParamCallback()` — on Save, pulls fields via `getParam()`, writes to `Preferences`
- `onConfigModeCallback()` — on portal open, paints the LCD
- `WiFiEvent()` — swaps LCD art on AP connect/disconnect
- `autoConnect(...)`, `setClass("invert")` dark theme, custom menu

The crypto ticker registered ~52 parameters (25 trading pairs + names + timezone + language). **M5Clawd registers exactly one: the Anthropic API key.** WiFi SSID/password are WiFiManager built-ins.

**Do not assume Clawdmeter's code applies.** Its CO5300/AXP2101/NimBLE/LVGL specifics are for different hardware. M5Clawd uses none of it.

---

## Development Workflow

This project uses the structured `/N_*` commands from the starterPack toolkit. See `WORKFLOW.md` at the repo root for the canonical command reference.

### Phase-based development

Each phase has `docs/Phase [N]/PHASE_PRD.md` (requirements), `PHASE_TASKS.md` (checklist), `PHASE_IMP.md` (implementation guide).

### Commands

| Command           | Purpose                                                              |
| ----------------- | -------------------------------------------------------------------- |
| `/1_start`        | (done) Initialize project foundation docs                            |
| `/2_pm <phase>`   | Break a phase into tasks — **next step** (Phase 1: copy-strip + portal) |
| `/3_dev [phase]`  | Implement phase tasks in a git worktree                              |
| `/4_test [phase]` | Functional tests (unit + integration smoke)                          |
| `/5_visual [phase]` | UI validation — see embedded note below                            |
| `/6_doc`          | Update docs                                                          |
| `/7_go <task>`    | Ad-hoc work                                                          |
| `/8_done`         | Session wrap-up + worktree cleanup                                   |
| `/9_deploy`       | **N/A** — no prod server; the device is the deployment target        |

### `/5_visual` for embedded — important

There is no browser; the UI is on a 320×240 LCD. The starterPack's Playwright-based `/5_visual` does not apply. For MVP, "visual validation" means: flash the device, photograph the screen, attach the photo to the PR. A serial-framebuffer-dump screenshot path (like Clawdmeter's) could be wired up in Phase 3 — until then, manual photo.

---

## Typical Workflow

```text
/1_start                                          [DONE]
/2_pm  "Phase 1: copy-strip the crypto ticker + captive portal"
/3_dev          (one task at a time, in a worktree)
/4_test
/6_doc
/8_done
```

---

## Parallel Sessions & Worktrees

Code-modifying commands (`/3_dev`, `/7_go`) run in a git worktree by default. Once we `git init` at `/Users/kevinbrice/GIT/theClaw/` (HANDOFF_NOTES.md flags this HIGH-priority), worktrees go in `../theClaw-<branch>/`.

```bash
REPO_NAME=$(basename "$(git rev-parse --show-toplevel)")
BRANCH="phase-1-copy-strip"
WORKTREE_PATH="../${REPO_NAME}-${BRANCH}"
if [ ! -d "$WORKTREE_PATH" ]; then
  git fetch origin
  if git show-ref --verify --quiet "refs/heads/$BRANCH"; then
    git worktree add "$WORKTREE_PATH" "$BRANCH"
  else
    git worktree add -b "$BRANCH" "$WORKTREE_PATH" origin/main
  fi
fi
cd "$WORKTREE_PATH"
```

**Project-specific gotchas:**

- **Don't run two `arduino-cli upload` at once** — they fight for the USB serial port.
- `arduino-cli`'s build cache is global (`~/.arduino15`), shared across worktrees — fine, but a stale cache can mask changes; `arduino-cli cache clean` if a build looks wrong.
- The crypto ticker reference lives outside the project tree; worktrees don't affect it.

---

## Architecture (at a glance)

```text
+----------+   +-----------+   +---------+   +-----------+
| WiFi STA |-->|  Poller   |-->|  UI     |-->| M5.Lcd    |
+----------+   +-----------+   +---------+   | 320x240   |
     ^               ^              ^       +-----------+
     |               |              |
     |          +----+----+         |
     |          |Preferences|       |
     |          | (NVS)    |        |
     |          +----------+        |
     |                              |
+----+-----+   +-------------+
|WiFiMgr   |   | Buttons     |
|(portal)  |   | A/B/C       |
+----------+   +-------------+
```

- **WiFiManager:** vendored tzapu lib; soft-AP captive portal for first-boot config.
- **Poller:** every 60 s, TLS POST to `api.anthropic.com/v1/messages`, parse `anthropic-ratelimit-*` headers.
- **UI:** three screens (Splash, Usage, Status), drawn with `M5.Lcd.*`.
- **Preferences:** API key + last-known-good usage, namespace `m5clawd`.

**For full details:** `docs/2_ARCHITECTURE.md`.

---

## Design System

- **Colors:** Anthropic-orange primary (`#DA7756`) on warm dark background (`#1A1815`), warm-white text. Status colors: green / amber / muted red. Defined as constants in `m5clawd/config.h`.
- **Typography:** `M5.Lcd` built-in fonts + `Free_Fonts.h` (bundled with `M5Stack.h`). No custom font conversion in MVP.
- **Layout:** 320×240 fixed panel; three screens hand-laid. See `docs/3_DESIGN_SYSTEM.md`.

---

## Quality Standards

- **Build:** `arduino-cli compile` clean, no warnings
- **Smoke tests:** 5 scenarios on real hardware before each release (`docs/4_QUALITY_ASSURANCE.md`)
- **Secret hygiene:** No `sk-ant-` strings in any committed file — pre-commit `grep` check
- **Performance:** first-boot-to-usable-screen < 3 s; provision-to-first-poll < 30 s; 24h uptime test passes pre-release

**For details:** `docs/4_QUALITY_ASSURANCE.md`.

---

## Project Configuration

### Git commit guidelines

(Inherits from the starterPack project-root rules.)

1. No AI assistant references, attributions, or co-author tags
2. No "Generated with" footers
3. No emojis in commit messages
4. Commits read as normal developer commits by Kevin Brice
5. Concise, focused on what changed

**Commit format:**

```text
[type](scope): concise description

- change 1
- change 2
```

**Types:** feat, fix, refactor, test, docs, chore

### Code style

- Arduino C++. Match the crypto ticker's conventions where ambiguous (it's the code we copied).
- No emojis in code comments or docs unless explicitly requested.
- Sketch is split into `.ino` tabs (`m5clawd.ino`, `wifi_portal.ino`, `poller.ino`, `ui.ino`, `secrets_store.ino`) — Arduino concatenates them. Shared declarations go in `config.h`.
- Naming: `snake_case` functions/variables, `PascalCase` types, `UPPER_SNAKE` constants.

### Logging discipline

- All `Serial.print*` is debug-only — no secrets, ever.
- API key references in logs use a redactor (`sk-ant-***`).

---

## Project-Specific Commands

### Toolchain & flash (after Phase 1 creates `m5clawd/`)

```bash
cd m5clawd
./setup-toolchain.sh                          # one-time: arduino-cli + ESP32 core 1.0.4 + libs
./flash.sh                                    # compile + upload firmware + SPIFFS
arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 .   # build only
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:m5stack-core-esp32 .
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200   # serial @ 115200
```

> The exact FQBN string is finalized in Phase 1. M5Stack Core Basic is `esp32:esp32:m5stack-core-esp32` (or `m5stack-core2` for Core2 — not our target).

### Reference projects (read-only)

```bash
ls Clawdmeter/                                       # concept reference
git -C Clawdmeter log -1 --oneline                   # check the cloned SHA
ls ~/GIT/Crypto_Coin_TickerUS_Stock/                 # scaffolding source — DO NOT EDIT
```

---

## Configuration

**No `.env`, no SD card.** The crypto ticker read `ccticker.cfg` off an SD card; M5Clawd drops that entirely — all runtime config flows through the captive portal into `Preferences`.

**Compile-time options** (in `m5clawd/config.h`):

- `POLL_INTERVAL_S` — default 60
- `WIFI_AP_SSID_PREFIX` — `"M5Clawd"` (last 6 of MAC appended at runtime)
- `FW_VERSION` — firmware version string
- Color constants, font selections

**No ports** — we are not a server.

---

## Troubleshooting

**Device not on `/dev/cu.usbserial-*`:**

- Try a different cable (some USB-C cables are charge-only)
- `ls /dev/cu.*` before/after plugging in
- M5Stack Core Basic uses CP2104 — usually plug-and-play on macOS; else install the [SiLabs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

**`arduino-cli compile` fails:**

- Confirm ESP32 core 1.0.4 is installed: `arduino-cli core list`
- Confirm libraries: `arduino-cli lib list` (M5Stack 0.3.1, ArduinoJson 6.17.3)
- The vendored WiFiManager must be inside `m5clawd/` next to the `.ino`
- Re-run `./setup-toolchain.sh`

**Captive portal doesn't pop on phone:**

- iOS DNS-over-HTTPS can suppress auto-pop — open `http://192.168.4.1` manually
- AP SSID should match `M5Clawd-XXXXXX` shown on screen

**Need recent context:**

- `docs/HANDOFF_NOTES.md` — newest session at top
- `git log --oneline -10`
- Latest phase: `ls "docs/Phase "*/`, check `PHASE_TASKS.md`

---

## MCP Servers Available

- **Sequential Thinking** — useful for the captive-portal state machine and TLS-failure recovery logic
- **Playwright / Puppeteer** — **not applicable** to firmware; skip
- **Fetch / WebFetch** — useful for Anthropic API docs, M5Stack library docs

---

## Notes for AI Agents

- **Both `Clawdmeter/` and `Crypto_Coin_TickerUS_Stock` are read-only.** Copy *from* the crypto ticker once (Phase 1 copy-strip); never write to either.
- **Don't suggest BLE in MVP.** [ADR 002](docs/decisions/002-wifi-vs-ble-daemon.md) deferred it entirely.
- **Don't suggest PlatformIO or M5Unified.** [ADR 001](docs/decisions/001-tech-stack-selection.md) was revised to the Arduino/`arduino-cli`/`M5Stack.h` stack to match the crypto ticker. If you think PlatformIO is better, write a new ADR — don't silently switch.
- **Don't hardcode the API key in the sketch.** Ever. It goes through the captive portal into `Preferences` ([ADR 003](docs/decisions/003-onboarding-via-wifimanager.md), [ADR 005](docs/decisions/005-secrets-storage-nvs.md)).
- **ArduinoJson is v6, not v7.** Use `StaticJsonDocument` / `DynamicJsonDocument`, not v7's unified `JsonDocument`.
- **You cannot flash a device or read its screen.** When you finish a UI change, ask the user to flash + photograph. Don't claim "tested" otherwise.
- **When a change conflicts with an ADR, write a new ADR** rather than silently overriding it.

---

**Remember:**

- Read `docs/HANDOFF_NOTES.md` before starting work
- Read the relevant ADR before changing architecture
- Update `HANDOFF_NOTES.md` at the end of every session
- The constraint to optimize for is **easy setup once cloned** — re-check every design decision against it
