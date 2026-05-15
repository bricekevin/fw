# Phase 1 - Bootstrap Tasks

**Status:** 14/14 complete — Phase 1 done, validated on hardware
**Updated:** 2026-05-15

> See HANDOFF_NOTES.md for detailed context. See PHASE_IMP.md for code snippets and commands.
> Checkbox legend: `[ ]` pending, `[~]` in-progress, `[x]` done.

---

## Epic 1: Toolchain validation & hardware bring-up

> Do this epic FIRST. It de-risks the headline unknown (ADR 001: arduino-cli + ESP32 core 1.0.4).

- [x] **1.1 Validate the arduino-cli toolchain**
  - [x] Run `m5clawd/setup-toolchain.sh`; confirm ESP32 core 1.0.4 installs
  - [x] Confirm libraries M5Stack 0.3.1 and ArduinoJson 6.17.3 install
  - [x] Write a throwaway 10-line `M5.begin()` + `Serial.println` sketch; `arduino-cli compile` it
  - [x] Toolchain finding: a global sketchbook `SD` library shadows the ESP32 SD and breaks the build. Fixed with a `sketch.yaml` build profile (not an ADR 001 core change). See HANDOFF_NOTES.

- [x] **1.2 Confirm flash + serial round-trip on the device**
  - [x] Device enumerates as `/dev/cu.usbserial-02132522` — plug-and-play, no CP210x driver step needed
  - [x] Flashed the m5clawd firmware; it boots and runs (provisioning + status screens render)
  - [x] `arduino-cli monitor` shows serial at 115200 (boot banner + `[boot]`/`[wifi]` logs captured)

---

## Epic 2: Copy-and-strip the sketch

> Per ADR 006. Compile after every removal so breakage stays localized.

- [x] **2.1 Copy the crypto ticker sketch and split into tabs**
  - [x] Copy `~/GIT/Crypto_Coin_TickerUS_Stock/Crypto_Coin_TickerUS_Stock.ino/Crypto_Coin_TickerUS_Stock.ino.ino` to `m5clawd/m5clawd.ino`
  - [x] Create tab files: `wifi_portal.ino`, `ui.ino`, `secrets_store.ino`
  - [~] Baseline compile of the unmodified copy was not possible: it depends on libraries (SDConfig, WebSocketsClient, NeoPixel, M5StackUpdater, Timezone, SHT3X) deliberately excluded from the build profile. Stripped incrementally instead, compiling against the profile. See HANDOFF_NOTES.

- [x] **2.2 Strip the data-feed layer**
  - [x] Removed `WebSocketsClient` + all Binance.US streaming code
  - [x] Removed candlestick / chart rendering code
  - [x] Removed the 25-pair data model and pair-cycling logic

- [x] **2.3 Strip peripherals and extras**
  - [x] Removed `SHT3X` sensor, `Adafruit_NeoPixel`, `M5StackUpdater`, `Timezone`
  - [x] Removed `SDConfig` / SD-card config (`ccticker.cfg`) — all config now comes via the portal
  - [x] Removed orphaned helpers, globals, and includes

- [x] **2.4 Reach a minimal clean-compiling skeleton**
  - [x] `setup()` reduced to: `M5.begin()`, SPIFFS init, boot-mode select, WiFiManager / station entry
  - [x] `loop()` reduced to: `M5.update()` + button poll + periodic status refresh
  - [x] `arduino-cli compile` zero errors; only warning is a benign `#warning` inside the vendored WiFiManager
  - [x] Sketch is 549 lines across tabs vs the 4296-line original

---

## Epic 3: Captive portal - one API-key field

- [x] **3.1 Replace the WiFiManager parameter set** (`wifi_portal.ino`)
  - [x] Removed the 25 pair + 25 name params, timezone, and language params
  - [x] Added one `WiFiManagerParameter` for the Anthropic API key (`type="password"`, length 120)
  - [x] Kept `getParam()` / `saveParamCallback()` / `onConfigModeCallback()` / `WiFiEvent()` structure intact
  - [x] Kept dark theme (`setClass("invert")`), custom menu, `setTitle`

- [x] **3.2 Persist the API key to NVS** (`secrets_store.ino`)
  - [x] `saveParamCallback()` writes the key to `Preferences` namespace `NVS_NAMESPACE`, key `NVS_KEY_API_KEY`
  - [x] Sets the `NVS_KEY_CONFIGURED` flag on save
  - [x] Rejects a malformed key (must start with `sk-ant-`, min length) — on-screen hint, portal re-opens
  - [x] `secret_redactor()` helper; the key never reaches `Serial`

- [x] **3.3 Provisioning-mode screen** (`ui.ino`)
  - [x] `ui_show_provisioning()` paints "Configure WiFi" + the AP SSID + `http://192.168.4.1`
  - [x] AP SSID computed as `WIFI_AP_SSID_PREFIX` + last 6 hex of the MAC
  - [x] QR code for the AP via `M5.Lcd.qrcode()` (confirmed present in M5Stack 0.3.1)

---

## Epic 4: WiFi station mode, screens, reset gesture

- [x] **4.1 Boot mode selection** (`m5clawd.ino`)
  - [x] On boot, reads `NVS_KEY_CONFIGURED`; unset -> provisioning, set -> station
  - [x] Station mode: `WiFi.begin()` with WiFiManager-stored creds; "Connecting" screen
  - [x] Handles connect failure (timeout -> one retry, then a clear error screen)

- [x] **4.2 Boot, connecting, and status screens** (`ui.ino`)
  - [x] Splash screen: "M5Clawd v<FW_VERSION>", brand colors from `config.h`
  - [x] Connecting screen: "Connecting to WiFi..."
  - [x] Status screen: firmware version, WiFi SSID + RSSI, IP, free heap, uptime
  - [x] Button A cycles Splash <-> Status

- [x] **4.3 NVS reset gesture** (`m5clawd.ino` + button poll)
  - [x] Long-press button C >= `RESET_HOLD_MS` -> on-screen "keep holding to reset" confirmation
  - [x] On confirm (+2 s hold): `prefs.clear()` + `wifiManager.resetSettings()` + `ESP.restart()`
  - [x] Button C short press -> toggle LCD backlight

---

## Epic 5: Testing & documentation

- [x] **5.1 Smoke tests on real hardware** — all four scenarios pass
  - [x] Scenario 1 (fresh flash -> portal): AP `M5Clawd-0D0A10` appears, provisioning screen renders, portal reachable
  - [x] Submit valid WiFi creds + a well-formed API key -> device auto-reboots into station mode, reaches `WL_CONNECTED` (IP 192.168.0.154), status screen renders. (A malformed key was correctly rejected and the portal reopened.)
  - [x] Reflash without erase -> creds survive, device goes straight to station mode (status screen)
  - [x] Reset gesture -> long-press C wipes NVS, device returns to the provisioning screen
  - [x] Serial logs captured (boot banner, `[boot] configured -> station mode`, `[wifi] connected`)
  - Note: on the very first provisioning the device needed one manual power-cycle to connect; did not reproduce on retest. See HANDOFF_NOTES known issues.

- [x] **5.2 Update documentation**
  - [x] Added a HANDOFF_NOTES session entry (toolchain finding, strip outcome, build profile)
  - [x] Ticked the checkboxes in this file
  - [x] Toolchain validation did not force an ADR 001 change; the build-profile finding is recorded in HANDOFF_NOTES

---

## Notes

**Blockers:**
- None. Phase 1 is complete: firmware compiles clean and all four hardware smoke scenarios pass on the M5Stack Core.

**Dependencies:**
- Epic 2+ depends on Epic 1 (a working toolchain).
- Epics 3 and 4 depend on Epic 2 (a clean-compiling skeleton).
- Epic 5 depends on Epics 3 and 4.

**Deferred to Phase 2:** Anthropic HTTPS poller, the Usage screen with real data. `poller.ino` is intentionally NOT created this phase.
</content>
</invoke>
