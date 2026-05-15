# Phase 1 - Bootstrap Implementation

**Status:** IN_PROGRESS
**Checkpoint:** v0.1.0

> Embedded-firmware phase — no database, no web backend, no server deployment.
> This guide is adapted accordingly. Code snippets are sketches/intent, not final.

---

## Setup

```bash
# One-time toolchain install (Epic 1, Task 1.1)
cd m5clawd
./setup-toolchain.sh

# Throwaway sanity sketch to prove the toolchain (Task 1.1)
#   write a 10-line M5.begin() + Serial.println sketch in a temp folder
arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 /tmp/hello_m5

# Flash it and watch serial (Task 1.2)
arduino-cli upload --fqbn esp32:esp32:m5stack-core-esp32 -p /dev/cu.usbserial-XXXX /tmp/hello_m5
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
```

If `arduino-cli core install esp32:esp32@1.0.4` fails: that is the ADR 001
risk materializing. Stop, record the error, and revisit ADR 001 — do not
silently install a newer core and proceed.

---

## Implementation

### Copy & split (Epic 2, Task 2.1)

```bash
SRC=~/GIT/Crypto_Coin_TickerUS_Stock/Crypto_Coin_TickerUS_Stock.ino
cp "$SRC/Crypto_Coin_TickerUS_Stock.ino.ino" m5clawd/m5clawd.ino
touch m5clawd/wifi_portal.ino m5clawd/ui.ino m5clawd/secrets_store.ino
# Baseline compile BEFORE stripping anything:
arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 m5clawd
```

Arduino concatenates every `.ino` in the folder, so moving a function from
`m5clawd.ino` into `wifi_portal.ino` needs no `#include`. Shared declarations
go in `config.h` (already created).

### Strip checklist (Epic 2, Tasks 2.2-2.3)

Remove, recompiling after each block:

| Remove | Crypto-ticker symbols to hunt for |
|--------|-----------------------------------|
| WebSockets / Binance feed | `#include <WebSocketsClient.h>`, `webSocket`, `client.setTimeout`, stock-fetch code |
| Chart rendering | candlestick draw routines, timeframe handling |
| 25-pair model | `pairStrings`, `nameStrings`, `pair_*`, `pairs`, pair-cycling on ButtonA |
| SHT3X sensor | `#include "SHT3X.h"`, `SHT3X`, temperature/humidity display |
| NeoPixel | `#include <Adafruit_NeoPixel.h>`, `LEDbar`, `colorWipe` |
| SD-menu loader | `#include "M5StackUpdater.h"`, `updateFromFS` |
| Timezone | `#include <Timezone.h>`, `validateAndNormalizeTimezone` |
| SD config | `#include <SDConfig.h>`, `ccticker.cfg` reads, `SD.begin` |

Keep: `M5Stack.h`, `WiFi.h`, `Preferences.h`, the vendored `WiFiManager.h`,
`ArduinoJson.h` (will be used by the Phase 2 poller — keep the include),
`Free_Fonts.h`, `FS.h`/SPIFFS.

### WiFiManager: one API-key field (Epic 3, Task 3.1)

The crypto ticker registered ~52 `WiFiManagerParameter`s. Replace them all with one.

```cpp
// wifi_portal.ino  — intent sketch
WiFiManager wifiManager;
WiFiManagerParameter apiKeyField;   // declared once, global

void wifi_portal_begin() {
  new (&apiKeyField) WiFiManagerParameter(
      PARAM_ID_ANTHROPIC_KEY, "Anthropic API Key",
      "", 108, "placeholder=\"sk-ant-...\" type=\"password\"");
  wifiManager.addParameter(&apiKeyField);

  wifiManager.setSaveParamsCallback(saveParamCallback);
  wifiManager.setAPCallback(onConfigModeCallback);
  wifiManager.setTitle("M5CLAWD SETUP");
  wifiManager.setClass("invert");                 // dark theme
  std::vector<const char*> menu = {"wifi", "exit"};
  wifiManager.setMenu(menu);

  wifiManager.autoConnect(ap_ssid().c_str());     // "M5Clawd-XXXXXX"
}

String getParam(const String& name) {             // kept from the crypto ticker
  return wifiManager.server->hasArg(name)
       ? wifiManager.server->arg(name) : String();
}
```

### Persist the key (Epic 3, Task 3.2)

```cpp
// secrets_store.ino  — intent sketch
#include <Preferences.h>
Preferences prefs;

void saveParamCallback() {
  String key = getParam(PARAM_ID_ANTHROPIC_KEY);
  if (!key.startsWith("sk-ant-")) {
    ui_portal_hint("Key must start with sk-ant-");
    return;                                        // stay in portal
  }
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_API_KEY, key);
  prefs.putBool(NVS_KEY_CONFIGURED, true);
  prefs.end();
  // WiFiManager restarts the ESP after save → next boot is station mode
}

const char* secret_redactor(const String& k) {     // never log the raw key
  return k.startsWith("sk-ant-") ? "sk-ant-***" : "***";
}
```

### Boot mode selection (Epic 4, Task 4.1)

```cpp
// m5clawd.ino  — intent sketch
void setup() {
  M5.begin();                       // LCD + buttons + power
  SPIFFS.begin(true);
  prefs.begin(NVS_NAMESPACE, true);
  bool configured = prefs.getBool(NVS_KEY_CONFIGURED, false);
  prefs.end();

  if (!configured) {
    ui_show_provisioning();
    wifi_portal_begin();            // blocks in the captive portal
  } else {
    ui_show_connecting();
    WiFi.begin();                   // uses WiFiManager-persisted creds
  }
}
```

### Reset gesture (Epic 4, Task 4.3)

```cpp
// in loop(), after M5.update()
if (M5.BtnC.pressedFor(RESET_HOLD_MS)) {
  ui_show_reset_confirm();
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  wifiManager.resetSettings();      // clears WiFiManager's WiFi creds
  ESP.restart();
}
```

### Screens (Epic 4, Task 4.2)

`M5.Lcd` direct drawing — see `docs/3_DESIGN_SYSTEM.md` for layouts. Colors come
from `config.h` (`COLOR_BG`, `COLOR_PRIMARY`, ...). Phase 1 screens are
text-only (no PNGs). Use `M5.Lcd.fillScreen`, `setTextColor`, `setTextFont` /
`setFreeFont`, `drawString`. QR via `M5.Lcd.qrcode(text, x, y, size)`.

---

## Testing

```bash
# Build check
arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 m5clawd

# Full-erase flash for smoke test 1 (fresh-flash → portal mode)
arduino-cli upload -e --fqbn esp32:esp32:m5stack-core-esp32 -p /dev/cu.usbserial-XXXX m5clawd

# Normal flash (creds survive)
cd m5clawd && ./flash.sh

# Watch serial
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
```

Run smoke scenarios from `PHASE_TASKS.md` Task 5.1 / `docs/4_QUALITY_ASSURANCE.md`.

---

## Rollback Plan

This is firmware on a single dev device — "rollback" is just reflashing a known
build. No production, no database, no users.

**If a flashed build misbehaves:**

```bash
# Revert the offending commits and reflash
git log --oneline -10
git revert <bad_commit> --no-edit
cd m5clawd && ./flash.sh
```

**If the device is in a bad state (boot loop, bricked-looking):**

```bash
# Full-erase upload wipes flash + NVS, giving a clean slate
arduino-cli upload -e --fqbn esp32:esp32:m5stack-core-esp32 -p /dev/cu.usbserial-XXXX m5clawd
```

The ESP32 ROM bootloader cannot be bricked by an app-level flash, so a
full-erase upload always recovers the device.

**Rollback risks:** A full-erase upload also wipes stored credentials — the user
must re-onboard through the captive portal afterward. That is expected and
acceptable for a dev device.

---

## Completion Checklist

- [ ] Toolchain validated; `arduino-cli` builds against ESP32 core 1.0.4 (or ADR 001 revised)
- [ ] `m5clawd.ino` copy-stripped; `arduino-cli compile` clean
- [ ] Captive portal shows one API-key field; key persists to NVS
- [ ] Provisioning vs station mode selection works on boot
- [ ] Station mode connects to WiFi with stored creds
- [ ] Splash / connecting / status screens render
- [ ] Reset gesture (long-press C) wipes NVS and returns to portal
- [ ] Smoke scenarios 1-4 pass on real hardware
- [ ] HANDOFF_NOTES and PHASE_TASKS updated

---

**Updated:** 2026-05-15
