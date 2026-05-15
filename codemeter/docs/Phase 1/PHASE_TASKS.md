# Phase 1 - Bootstrap Tasks

**Status:** 0/16 complete
**Updated:** 2026-05-15

> See HANDOFF_NOTES.md for detailed context. See PHASE_IMP.md for code snippets and commands.
> Checkbox legend: `[ ]` pending, `[~]` in-progress, `[x]` done.

---

## Epic 1: Toolchain validation & hardware bring-up

> Do this epic FIRST. It de-risks the headline unknown (ADR 001: arduino-cli + ESP32 core 1.0.4).

- [ ] **1.1 Validate the arduino-cli toolchain**
  - [ ] Run `m5clawd/setup-toolchain.sh`; confirm ESP32 core 1.0.4 installs
  - [ ] Confirm libraries M5Stack 0.3.1 and ArduinoJson 6.17.3 install
  - [ ] Write a throwaway 10-line `M5.begin()` + `Serial.println` sketch; `arduino-cli compile` it
  - [ ] If core 1.0.4 fails to install or compile: STOP, document findings, revise ADR 001 + `setup-toolchain.sh`

- [ ] **1.2 Confirm flash + serial round-trip on the device**
  - [ ] Identify the device's USB serial port (`ls /dev/cu.*`)
  - [ ] Flash the throwaway sketch; confirm it runs (LCD or serial output)
  - [ ] Confirm `arduino-cli monitor` shows serial at 115200
  - [ ] Note the port name and any CP210x driver step in HANDOFF_NOTES

---

## Epic 2: Copy-and-strip the sketch

> Per ADR 006. Compile after every removal so breakage stays localized.

- [ ] **2.1 Copy the crypto ticker sketch and split into tabs**
  - [ ] Copy `~/GIT/Crypto_Coin_TickerUS_Stock/Crypto_Coin_TickerUS_Stock.ino/Crypto_Coin_TickerUS_Stock.ino.ino` to `m5clawd/m5clawd.ino`
  - [ ] Create empty tab files: `wifi_portal.ino`, `ui.ino`, `secrets_store.ino`
  - [ ] Confirm the unmodified copy still compiles (baseline before stripping)

- [ ] **2.2 Strip the data-feed layer**
  - [ ] Remove `WebSocketsClient` include + all Binance.US streaming code
  - [ ] Remove candlestick / chart rendering code
  - [ ] Remove the 25-pair data model and pair-cycling logic
  - [ ] Compile after each removal

- [ ] **2.3 Strip peripherals and extras**
  - [ ] Remove `SHT3X` sensor (`SHT3X.cpp/.h` references), `Adafruit_NeoPixel`, `M5StackUpdater`, `Timezone`
  - [ ] Remove `SDConfig` / SD-card config (`ccticker.cfg` reading) — all config now comes via the portal
  - [ ] Remove now-orphaned helpers, globals, and includes
  - [ ] Compile clean

- [ ] **2.4 Reach a minimal clean-compiling skeleton**
  - [ ] `setup()` reduced to: `M5.begin()`, SPIFFS init, `Preferences.begin()`, mode-select stub, WiFiManager entry
  - [ ] `loop()` reduced to: `M5.update()` + button-poll stub
  - [ ] `arduino-cli compile` zero errors; review and resolve/triage warnings
  - [ ] Confirm `m5clawd.ino` + tabs are noticeably smaller than the original

---

## Epic 3: Captive portal - one API-key field

- [ ] **3.1 Replace the WiFiManager parameter set** (`wifi_portal.ino`)
  - [ ] Remove the 25 pair + 25 name params, timezone, and language params
  - [ ] Add one `WiFiManagerParameter` for the Anthropic API key (`PARAM_ID_ANTHROPIC_KEY`, `type="password"`, length ~108)
  - [ ] Keep the `getParam()` / `saveParamCallback()` / `onConfigModeCallback()` / `WiFiEvent()` structure intact
  - [ ] Keep dark theme (`setClass("invert")`), custom menu, `setTitle`

- [ ] **3.2 Persist the API key to NVS** (`secrets_store.ino`)
  - [ ] `saveParamCallback()` writes the key to `Preferences` namespace `NVS_NAMESPACE`, key `NVS_KEY_API_KEY`
  - [ ] Set the `NVS_KEY_CONFIGURED` flag on save
  - [ ] Reject an obviously-malformed key (must start with `sk-ant-`) — show an on-screen hint, stay in portal
  - [ ] Add a `secret_redactor()` helper; confirm the key never reaches `Serial`

- [ ] **3.3 Provisioning-mode screen** (`ui.ino`)
  - [ ] `onConfigModeCallback()` paints "Configure WiFi" + the AP SSID + `http://192.168.4.1`
  - [ ] Compute the AP SSID as `WIFI_AP_SSID_PREFIX` + last 6 hex of the MAC
  - [ ] Render a QR code for the AP via `M5.Lcd.qrcode()` (fallback: SSID/URL text only if unavailable)

---

## Epic 4: WiFi station mode, screens, reset gesture

- [ ] **4.1 Boot mode selection** (`m5clawd.ino`)
  - [ ] On boot, read `NVS_KEY_CONFIGURED`; unset -> provisioning, set -> station
  - [ ] Station mode: `WiFi.begin()` with WiFiManager-stored creds; show the "Connecting" screen
  - [ ] Handle connect failure (timeout -> retry, then fall back to a clear error screen)

- [ ] **4.2 Boot, connecting, and status screens** (`ui.ino`)
  - [ ] Splash screen: static "M5Clawd v<FW_VERSION>" text, brand colors from `config.h`
  - [ ] Connecting screen: "Connecting to <ssid>..."
  - [ ] Status screen: firmware version, WiFi SSID + RSSI, IP, free heap, uptime
  - [ ] Button A cycles Splash <-> Status

- [ ] **4.3 NVS reset gesture** (`secrets_store.ino` + button poll)
  - [ ] Long-press button C >= `RESET_HOLD_MS` -> on-screen "keep holding to reset" confirmation
  - [ ] On confirm: `prefs.clear()` + `wifiManager.resetSettings()` + `ESP.restart()`
  - [ ] Button C short press -> toggle LCD backlight

---

## Epic 5: Testing & documentation

- [ ] **5.1 Smoke tests on real hardware**
  - [ ] Scenario 1 (fresh flash -> portal): full-erase upload, power-cycle, AP appears, portal reachable
  - [ ] Submit valid WiFi creds + a well-formed API key -> device reboots and reaches `WL_CONNECTED`
  - [ ] Reflash without erase -> creds survive, device goes straight to station mode
  - [ ] Reset gesture -> device returns to portal mode
  - [ ] Paste serial logs into the phase notes / PR

- [ ] **5.2 Update documentation**
  - [ ] Add a HANDOFF_NOTES session entry (toolchain findings, port name, strip outcome)
  - [ ] Tick the checkboxes in this file as work completes
  - [ ] If the toolchain validation forced changes, update ADR 001 + `1_PROJECT_OVERVIEW.md`

---

## Notes

**Blockers:**
- None at planning time. The first potential blocker surfaces in Task 1.1 (toolchain). Resolve it before proceeding past Epic 1.

**Dependencies:**
- Epic 2+ depends on Epic 1 (a working toolchain).
- Epics 3 and 4 depend on Epic 2 (a clean-compiling skeleton).
- Epic 5 depends on Epics 3 and 4.
- Read access to `~/GIT/Crypto_Coin_TickerUS_Stock` (the copy source) is required for Task 2.1.

**Deferred to Phase 2:** Anthropic HTTPS poller, the Usage screen with real data. `poller.ino` is intentionally NOT created this phase.
