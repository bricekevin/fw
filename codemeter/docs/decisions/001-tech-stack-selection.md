# ADR 001: Tech Stack Selection

**Date:** 2026-05-14 (revised 2026-05-15)
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [002-wifi-vs-ble-daemon](002-wifi-vs-ble-daemon.md), [004-port-vs-fork](004-port-vs-fork.md), [006-start-from-crypto-ticker](006-start-from-crypto-ticker.md)

---

## Revision note (2026-05-15)

The original version of this ADR (2026-05-14) selected **PlatformIO + M5Unified + ArduinoJson 7**. That was the right call *in the abstract* — but on 2026-05-15 the user pointed us at an existing, working project on the exact target hardware: `Crypto_Coin_TickerUS_Stock`, an M5Stack Core ticker that already uses WiFiManager exactly the way the user wants.

That project is a **proven toolchain on the user's actual device**. The user chose to start M5Clawd by copy-and-stripping it (see [ADR 006](006-start-from-crypto-ticker.md)). Continuing to spec a *different* toolchain than the thing we're copying would be self-defeating. This ADR is therefore revised to match the reference project. The PlatformIO plan is preserved in this revision note for the record.

---

## Context

We are building M5Clawd — a standalone Claude Code usage display — for an **M5Stack Core Basic (Gray, ESP32)**. We have two reference codebases:

1. **`Clawdmeter/`** — the conceptual origin ([HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter)). Different hardware (Waveshare ESP32-S3 AMOLED), different transport (BLE). Read-only reference for the *idea* and the Anthropic poll trick.
2. **`Crypto_Coin_TickerUS_Stock`** (`~/GIT/Crypto_Coin_TickerUS_Stock`) — a working M5Stack Core ticker the user already runs. Same MCU family as our target, classic `M5Stack.h` API, WiFiManager-based onboarding the user is happy with. **This is the codebase M5Clawd is copy-and-stripped from.**

The constraint: M5Clawd should be the crypto ticker's scaffolding (WiFiManager onboarding, M5Stack display, `Preferences` storage) with the crypto/stock/chart logic replaced by an Anthropic usage poller. So the stack should *match the crypto ticker*, not diverge from it.

---

## Decision

**We will build M5Clawd on the crypto ticker's proven stack:**

| Layer            | Choice                                          | Why                                                                                          |
| ---------------- | ----------------------------------------------- | --------------------------------------------------------------------------------------------- |
| MCU framework    | **Arduino-ESP32**, board manager **1.0.4**      | What the crypto ticker uses. Pinned 1.0.4 — the crypto ticker's comments note newer versions caused boot problems. |
| Build / flash    | **`arduino-cli`** (sketch stays Arduino-IDE-compatible) | Scriptable one-command build/flash without restructuring to PlatformIO layout. The crypto ticker is an Arduino IDE project; `arduino-cli` compiles the same `.ino` tree headlessly and is CI-friendly. The user can still open it in Arduino IDE. |
| Board HAL        | **classic `M5Stack.h`** (M5Stack lib v0.3.1)    | The crypto ticker uses it; proven on Core Basic. `M5.Lcd.*`, `M5.update()`, `M5.BtnA/B/C`.    |
| Graphics         | `M5.Lcd.*` (TFT_eSPI-backed, built into `M5Stack.h`) | No separate graphics lib. `drawString`, `fillRect`, `drawPngFile` from SPIFFS.            |
| WiFi provisioning | **tzapu/WiFiManager**, vendored into the sketch folder | Exactly the crypto ticker's setup — `WiFiManager.cpp/.h` + `wm_*` files copied next to the `.ino`. See [ADR 003](003-onboarding-via-wifimanager.md). |
| HTTP / TLS       | **`HTTPClient` + `WiFiClientSecure`** (Arduino-ESP32 stdlib) | For HTTPS POST to `api.anthropic.com`. (The crypto ticker uses `WebSocketsClient` for Binance — we do *not* need WebSockets; a plain TLS POST is enough.) |
| JSON             | **ArduinoJson 6.17.3**                          | The exact version the crypto ticker pins. v6 API differs from v7 — match the reference to avoid surprises. |
| Storage          | **`Preferences.h`** (NVS), namespace `m5clawd`  | The crypto ticker stores all settings in `Preferences` (namespace `settings`). We use the same library, our own namespace. See [ADR 005](005-secrets-storage-nvs.md). |
| Splash assets    | **SPIFFS PNGs** via `M5.Lcd.drawPngFile`        | The crypto ticker ships PNGs in `data/` uploaded to SPIFFS. We can do the same for splash/status screens. |
| Lang             | C++ (Arduino dialect, GNU++11 as the 1.0.4 core defaults) | Whatever the crypto ticker compiles under.                                              |

**We will _not_ use:**

- **PlatformIO** — superseded. The reference is an Arduino sketch; copying it and *then* converting to PlatformIO layout would be churn for no gain. `arduino-cli` gives us scriptable builds without the conversion.
- **M5Unified / M5GFX** — superseded. The crypto ticker uses classic `M5Stack.h` and it works on Core Basic. Forward-portability to Core2/CoreS3 is no longer a goal (the user has a Core Basic and that is the target).
- **LVGL** — never planned; the crypto ticker draws directly with `M5.Lcd.*` and so will we.
- **NimBLE / BLE** — no BLE in MVP. See [ADR 002](002-wifi-vs-ble-daemon.md).
- **WebSocketsClient** — the crypto ticker uses it for Binance streaming; M5Clawd polls Anthropic with a one-shot HTTPS POST every 60 s, so no WebSocket dependency.
- **SD card config** — the crypto ticker reads `ccticker.cfg` off an SD card *and* uses `Preferences`. M5Clawd drops SD entirely: all config comes through the captive portal into `Preferences`. Simpler, one fewer thing for the user to prepare.

---

## Alternatives Considered

### Option 1: PlatformIO + M5Unified (the original 2026-05-14 plan)

**Description:** A clean PlatformIO project, `m5stack/M5Unified` HAL, ArduinoJson 7, version-pinned `lib_deps`.

**Pros:**
- One-command flash, CI-friendly, reproducible dependency versions.
- M5Unified would make the firmware portable to Core2 / CoreS3.
- Cleaner project layout (`src/`, `lib/`, `platformio.ini`).

**Cons:**
- Diverges from the reference project we are copying. We'd copy crypto-ticker code written against `M5Stack.h` and then have to port every `M5.Lcd.*` call to M5GFX — extra work, new bugs.
- The crypto ticker's WiFiManager integration is written against the vendored tzapu lib + classic toolchain; reusing it verbatim is the whole point of [ADR 006](006-start-from-crypto-ticker.md).
- A new toolchain is one more thing to validate against the user's hardware before we can flash anything.

**Rejected because:** Once we decided to copy-and-strip the crypto ticker (ADR 006), matching its toolchain is strictly lower-risk. `arduino-cli` recovers most of PlatformIO's scriptability without the conversion cost.

### Option 2: Arduino IDE GUI only (no `arduino-cli`)

**Description:** Build and flash purely through the Arduino IDE GUI, like the crypto ticker's documented workflow.

**Pros:**
- Zero new tooling — exactly the reference project's flow.

**Cons:**
- Not scriptable; no clean CI path; an AI agent can't drive a GUI to flash.
- The crypto ticker already accreted ~29 ad-hoc shell scripts trying to script around the GUI — evidence the GUI-only flow is painful.

**Rejected because:** `arduino-cli` compiles the identical sketch headlessly and scriptably. We get the reference's compatibility *and* automation. The sketch stays openable in the Arduino IDE for anyone who prefers it.

### Option 3: ESP-IDF

**Description:** Espressif's native framework.

**Pros:** Smaller binary, more control.

**Cons:** The reference project, `M5Stack.h`, WiFiManager, ArduinoJson are all Arduino-ecosystem. Reimplementing them is multi-week work for no benefit here.

**Rejected because:** Same reasoning as the original ADR — no upside, large cost, and now doubly so since we're copying an Arduino project.

---

## Consequences

### Positive

- **Maximum reuse:** The crypto ticker's WiFiManager onboarding, `Preferences` persistence, M5Stack display init, and button handling all transfer with minimal edits.
- **Proven on the user's hardware:** Board manager 1.0.4 + M5Stack lib 0.3.1 already boot and run on this exact device. No toolchain validation gamble.
- **Scriptable:** `arduino-cli compile` / `arduino-cli upload` give us the one-command flash that PlatformIO would have — drivable by a script or an AI agent.
- **Simpler than the reference:** Dropping SD-card config and WebSockets removes two subsystems. M5Clawd is the crypto ticker minus complexity, not plus.

### Negative

- **Pinned-old dependencies:** Board manager 1.0.4 is old (2020-era). It works, but we inherit its quirks and miss newer ESP32-Arduino fixes. Mitigation: it's pinned for a reason (the crypto ticker's own comments cite boot regressions in newer cores); we don't fight that fight now.
- **ArduinoJson 6, not 7:** v6's API (`StaticJsonDocument` / `DynamicJsonDocument`) is older than v7's unified `JsonDocument`. Fine, just match the reference; documented so no one "upgrades" it casually.
- **No automatic Core2/CoreS3 portability:** Classic `M5Stack.h` is Core-Basic-shaped. If the user later wants Core2, that's a port. Accepted — not a goal.
- **Vendored WiFiManager:** The tzapu lib lives in-tree (copied from the crypto ticker), not pulled by a package manager. Updates are manual. Accepted — it's a stable, rarely-updated dependency and vendoring is what the reference does.

### Neutral

- **`arduino-cli` config** (board URLs, the 1.0.4 core) must be captured in a setup script so a fresh clone is reproducible. We will write `m5clawd/setup-toolchain.sh`.
- **The crypto ticker's ~29 build/flash shell scripts are NOT copied** — they're accreted cruft. M5Clawd gets two clean scripts: `setup-toolchain.sh` and `flash.sh`.

---

## Implementation

**Timeline:** Toolchain + copy-strip is Phase 1, task 1.

**Steps:**

1. Install `arduino-cli`; script the board-manager 1.0.4 + library installs (`m5clawd/setup-toolchain.sh`)
2. Copy the crypto ticker into `m5clawd/` and strip per [ADR 006](006-start-from-crypto-ticker.md)
3. Confirm a clean build of the *stripped* sketch with `arduino-cli compile`
4. Add the Anthropic poller (Phase 2)

**Dependencies:**

- `arduino-cli` on the dev Mac
- ESP32 Arduino core 1.0.4
- Libraries: M5Stack 0.3.1, ArduinoJson 6.17.3, plus the vendored WiFiManager (in-tree)
- M5Stack Core Basic hardware + USB-C cable

**Risks:**

- **`arduino-cli` + core 1.0.4 interaction:** Older cores occasionally need specific `arduino-cli` versions. Mitigation: pin `arduino-cli` version in the setup script; test the compile on day 1 of Phase 1.
- **TLS to `api.anthropic.com`:** Same risk the original ADR flagged — ESP32 SSL handshake heap pressure. Mitigation unchanged: pin a single root CA, tear down `WiFiClientSecure` between polls.

---

## Validation

**How will we know this was the right call?**

- A fresh clone runs `m5clawd/setup-toolchain.sh && ./flash.sh` and the device boots — under 10 minutes including toolchain install.
- The stripped sketch compiles without touching a single `M5.Lcd.*` call (proof the classic-API reuse worked).
- We never wish we'd kept PlatformIO — meaning `arduino-cli` scripted builds are good enough.

**Review Date:** End of Phase 1 (post copy-strip + first build)

---

## References

- Reference project: `~/GIT/Crypto_Coin_TickerUS_Stock`
- [arduino-cli docs](https://arduino.github.io/arduino-cli/)
- [classic M5Stack library](https://github.com/m5stack/M5Stack)
- [ArduinoJson 6 docs](https://arduinojson.org/v6/doc/)
- [Upstream Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) (conceptual reference only)

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
