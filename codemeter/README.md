# M5Clawd

A standalone, WiFi-connected desk-side display for Claude Code usage, built for the **M5Stack Core Basic (Gray, ESP32)**.

Polls `api.anthropic.com` directly every 60 seconds and renders your current 5-hour session and weekly utilization on the 320×240 LCD. No host computer, no daemon, no companion app — just the device, your WiFi, and an Anthropic API key.

---

## Why this exists

M5Clawd takes its **concept** from [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) (a Claude usage display, originally for Waveshare ESP32-S3 hardware with a host laptop daemon) and rearchitects it to be standalone — WiFi-direct, configured by a one-page captive portal.

Its **codebase** starts as a copy-and-strip of a working M5Stack Core project (`Crypto_Coin_TickerUS_Stock`) that already runs on this exact hardware and already uses WiFiManager for onboarding. See [`docs/decisions/`](docs/decisions/) for the full reasoning.

---

## Quick Start

> Setup target: under 5 minutes from a freshly powered-on device to live usage data on screen.

### Prerequisites

- **M5Stack Core Basic** (Gray or any original ESP32 M5Stack). USB-C cable.
- **`arduino-cli`** — `brew install arduino-cli` on macOS. The project's `setup-toolchain.sh` handles the rest (ESP32 core 1.0.4, libraries).
- **An Anthropic API key** (`sk-ant-...`). Get one at [console.anthropic.com](https://console.anthropic.com/settings/keys). Use a scoped project key with a low spending limit — the device stores it in flash.
- **A WiFi network with WPA2-PSK** (ESP32 original does not support WPA3-only networks).
- **A phone or laptop** to fill in the captive portal form.

### 1. Build & flash

```bash
git clone <this-repo> theClaw
cd theClaw/m5clawd

./setup-toolchain.sh        # one-time: arduino-cli config + ESP32 core 1.0.4 + libraries
./flash.sh                  # compile + upload firmware + SPIFFS assets via USB-C
```

> **Project status:** The `m5clawd/` firmware is created during Phase 1 by copy-and-stripping the `Crypto_Coin_TickerUS_Stock` reference project. Until Phase 1 runs, this repo holds the foundation docs, the ADRs, and a read-only clone of the Clawdmeter concept reference.

### 2. First-boot onboarding

1. Plug the M5Stack into USB-C (computer or charger — either works).
2. The screen shows: `Configure WiFi  →  Connect to "M5Clawd-XXXXXX"`.
3. On your phone: **Settings → WiFi → `M5Clawd-XXXXXX`**.
4. A captive-portal page should auto-open. If not, open any browser and visit `http://192.168.4.1`.
5. Fill in your home WiFi SSID + password, and your Anthropic API key.
6. Press **Save**. The device reboots, joins your WiFi, and starts polling within ~30 seconds.

### 3. Day-to-day use

The device sits there showing your usage. Buttons:

- **A** (left) — cycle screens (Splash ↔ Usage ↔ Status)
- **B** (middle) — force a poll right now
- **C** (right, **long-press 5 s**) — wipe stored credentials, return to onboarding

### 4. Reflash without losing credentials

```bash
cd m5clawd && ./flash.sh    # WiFi creds + API key in NVS survive a reflash
```

### 5. Start over from scratch

```bash
cd m5clawd
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:m5stack-core-esp32 -e .
# or use the M5Burner "erase" — then ./flash.sh again
```

---

## Tech Stack

- **Hardware:** M5Stack Core Basic (ESP32-D0WDQ6, 320×240 ILI9342C LCD, 3 buttons, IP5306 PMU)
- **Framework:** Arduino-ESP32, board manager 1.0.4 (pinned)
- **Build / flash:** `arduino-cli` (the sketch is also openable in the Arduino IDE)
- **Board HAL:** classic `M5Stack.h` library (v0.3.1)
- **WiFi provisioning:** [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager), vendored in-tree
- **HTTP / TLS:** Arduino-ESP32 `HTTPClient` + `WiFiClientSecure`
- **JSON:** ArduinoJson 6.17.3
- **Storage:** ESP32 NVS via `Preferences.h`
- **Assets:** SPIFFS PNGs

Full rationale: [`docs/decisions/001-tech-stack-selection.md`](docs/decisions/001-tech-stack-selection.md).

---

## Documentation Structure

```text
./
├── CLAUDE.md                       AI agent guide (read first if you're Claude)
├── README.md                       (this file)
├── docs/
│   ├── 1_PROJECT_OVERVIEW.md       Vision, goals, MVP scope, risks
│   ├── 2_ARCHITECTURE.md           Components, data flow, project layout, tech stack
│   ├── 3_DESIGN_SYSTEM.md          Screen layouts, colors, fonts
│   ├── 4_QUALITY_ASSURANCE.md      Test strategy, smoke tests, quality gates
│   ├── HANDOFF_NOTES.md            Session-by-session work log
│   └── decisions/                  6 ADRs covering the major design calls
├── Clawdmeter/                     Read-only concept reference (clone of upstream)
└── m5clawd/                        The firmware (created in Phase 1 by copy-strip)
```

**For project context, read in order:**

1. `docs/1_PROJECT_OVERVIEW.md` — what we're building and why
2. `docs/2_ARCHITECTURE.md` — how the pieces fit, the project layout
3. `docs/decisions/` — why each major choice was made

---

## Development Approach

This project uses the [theClaw](.) starterPack workflow — structured, phase-based development with AI assistance.

### Phase-based development

Each phase has `docs/Phase [N]/PHASE_PRD.md` (requirements), `PHASE_TASKS.md` (checklist), `PHASE_IMP.md` (implementation guide).

### Slash commands

| Command           | Purpose                                                        |
| ----------------- | -------------------------------------------------------------- |
| `/1_start`        | Initialize project foundation docs (done)                       |
| `/2_pm <phase>`   | Plan a phase                                                    |
| `/3_dev [phase]`  | Implement phase tasks (in a worktree)                           |
| `/4_test [phase]` | Functional tests                                                |
| `/5_visual [phase]` | UI validation (manual phone-photo for MVP)                    |
| `/6_doc`          | Update docs                                                     |
| `/7_go <task>`    | Ad-hoc work                                                     |
| `/8_done`         | Session wrap-up                                                 |
| `/9_deploy`       | N/A — the device is the deployment target                       |

### Typical workflow

```text
/1_start                                                  # done
/2_pm  "Phase 1: copy-strip the crypto ticker + captive portal"
/3_dev                                                    # implement
/4_test                                                   # validate
/6_doc                                                    # tidy up
/2_pm  "Phase 2: Anthropic poller + error UI"
... etc.
```

---

## Architecture (one-screen view)

```text
        Anthropic API (HTTPS)
                ^
                |
+---------------|-----------------+
|  M5Stack      v                 |
|  +-------+ +-----+ +---------+ |
|  | WiFi  |-|Poll |-|  UI     | |
|  +-------+ +-----+ +---------+ |
|       ^      ^         |       |
|       |      |         v       |
|       |      |     +---------+ |
|       |  +---+---+ | M5.Lcd  | |
|       |  |Prefs  | | 320x240 | |
|       |  | (NVS) | +---------+ |
|       |  +-------+             |
|       |                        |
|  +----+------+  +------------+ |
|  |WiFiMgr    |  |Buttons     | |
|  |(portal)   |  | A / B / C  | |
|  +-----------+  +------------+ |
+--------------------------------+
        ^
        | (first boot)
        v
   Phone browser
   captive portal
```

**Two modes:**

- **Provisioning** (first boot or post-reset): soft-AP `M5Clawd-XXXXXX`, captive-portal config page in a phone browser.
- **Station** (normal): joined to user's WiFi, polls Anthropic every 60 s, renders to the LCD.

Full diagram + per-component details in [`docs/2_ARCHITECTURE.md`](docs/2_ARCHITECTURE.md).

---

## Quality Standards

- **First-boot to live data:** < 5 minutes
- **Single-poll latency (TLS + parse):** < 5 s p95
- **24h uptime test:** ≥ 99% poll success rate, > 100 KB free heap
- **Build:** `arduino-cli compile` clean, no warnings
- **Secret hygiene:** no `sk-ant-` strings in any committed file (pre-commit check)

Full strategy: [`docs/4_QUALITY_ASSURANCE.md`](docs/4_QUALITY_ASSURANCE.md).

---

## Project Structure (post-Phase 1)

```text
theClaw/
├── CLAUDE.md, README.md, WORKFLOW.md
├── docs/                       (foundation docs + ADRs + per-phase plans)
├── Clawdmeter/                 (read-only concept reference)
└── m5clawd/                    (our firmware — copy-stripped from the crypto ticker)
    ├── m5clawd.ino             main sketch — setup(), loop()
    ├── wifi_portal.ino         WiFiManager + captive portal
    ├── poller.ino              Anthropic HTTPS poll + header parsing
    ├── ui.ino                  three screens, M5.Lcd drawing
    ├── secrets_store.ino       Preferences + reset gesture
    ├── config.h                compile-time constants
    ├── WiFiManager.cpp/.h      vendored tzapu WiFiManager
    ├── wm_*.h, strings_en.h    vendored WiFiManager support
    ├── data/                   SPIFFS PNG assets
    ├── setup-toolchain.sh
    └── flash.sh
```

---

## Getting Started (for contributors)

### Prerequisites

- M5Stack Core Basic hardware + USB-C cable
- `arduino-cli`: `brew install arduino-cli`
- Anthropic API key with a low spend limit

### Setup

```bash
# 1. Clone
git clone <this-repo> theClaw
cd theClaw

# 2. Pull the Clawdmeter concept reference (read-only)
git clone https://github.com/HermannBjorgvin/Clawdmeter.git Clawdmeter/

# 3. Read the docs
cat docs/HANDOFF_NOTES.md
cat docs/1_PROJECT_OVERVIEW.md
cat CLAUDE.md   # if you're an AI agent

# 4. After Phase 1 creates m5clawd/:
cd m5clawd && ./setup-toolchain.sh && ./flash.sh
```

### Verify a successful flash

```bash
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
# Expected: boot banner, then "M5Clawd v0.0.1" + WiFi state
```

---

## Troubleshooting

**Device not on `/dev/cu.usbserial-*` (macOS):**

- Try a different cable (some USB-C cables are charge-only)
- `ls /dev/cu.*` before and after plugging in
- M5Stack uses CP2104 — usually plug-and-play; else install the [SiLabs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

**Captive portal doesn't auto-pop:**

- Open any browser and go to `http://192.168.4.1`
- iOS with strict DNS-over-HTTPS won't auto-redirect; manual works

**`arduino-cli compile` fails:**

- `arduino-cli core list` — confirm ESP32 1.0.4
- `arduino-cli lib list` — confirm M5Stack 0.3.1, ArduinoJson 6.17.3
- Re-run `./setup-toolchain.sh`

**Device shows "API error" repeatedly:**

- Confirm the key starts with `sk-ant-`
- Long-press button C → re-onboard with a fresh key
- Check the Anthropic dashboard for billing / spending-limit issues

**Want recent dev context:**

- `docs/HANDOFF_NOTES.md` (newest session on top)
- `git log --oneline -20`

---

## Security & Privacy Notes

- The Anthropic API key is stored **in plaintext** in the device's NVS partition. If you lose physical control of the device, rotate the key.
- Use a **scoped project key** with a low spending limit — see [Anthropic API key best practices](https://docs.anthropic.com/en/api/getting-started#api-keys).
- The captive-portal AP is **unencrypted** during the ~2-minute onboarding window. Anyone in WiFi range can sniff the form submission. WPA2 on the AP is planned for a hardening phase.
- The poll request goes over TLS to `api.anthropic.com` — that part is secure.

Full threat model: [`docs/2_ARCHITECTURE.md`](docs/2_ARCHITECTURE.md#threat-model-summary), [`docs/decisions/005-secrets-storage-nvs.md`](docs/decisions/005-secrets-storage-nvs.md).

---

## License

Pending. M5Clawd is copy-and-stripped from the user's `Crypto_Coin_TickerUS_Stock` project (no explicit license) and vendors tzapu/WiFiManager (MIT). The Clawdmeter concept reference [notes its own licensing gray areas](Clawdmeter/README.md#licensing-gray-area-warning) around Anthropic fonts and the Clawd mascot — M5Clawd does not ship those assets. Licensing should be settled before any public release.

---

## Credits

- **Concept:** [Hermann Björgvin](https://github.com/HermannBjorgvin) — [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter)
- **Scaffolding source:** the user's own `Crypto_Coin_TickerUS_Stock` M5Stack project
- **WiFiManager:** [tzapu](https://github.com/tzapu/WiFiManager) (MIT)

---

**Built with the [theClaw](.) starterPack — a structured, phase-based development workflow.**
