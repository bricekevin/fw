# System Architecture

**Project:** M5Clawd
**Version:** 0.0.1

---

## Architecture Overview

```text
                            +-----------------------------+
                            |   api.anthropic.com         |
                            |   (Anthropic Messages API)  |
                            +--------------+--------------+
                                           ^
                                           | HTTPS POST (TLS 1.2/1.3)
                                           | claude-haiku-4-5, 1 token
                                           | parses response *headers*
                                           v
+----------------------------------------------------+
|  M5Stack Core Basic (ESP32-D0WDQ6)                 |
|                                                    |
|  +----------+   +----------+   +--------------+    |
|  | WiFi STA |---|  Poller  |---|  UI Engine   |    |
|  +----------+   +----------+   +------+-------+    |
|        ^              ^                |           |
|        |              |                v           |
|        |              |        +-----------------+ |
|        |              |        | M5.Lcd (TFT)    | |
|        |              |        | ILI9342C 320x240| |
|        |              |        +-----------------+ |
|        |              |                            |
|        |        +-----+------+                     |
|        +--------| Preferences|--+                  |
|                 |   (NVS)    |  |                  |
|                 +------------+  |                  |
|                                 |                  |
|  +------------+   +-------------+--+               |
|  |WiFiManager |   | Buttons A/B/C  |               |
|  |(vendored,  |   | (M5.BtnA/B/C)  |               |
|  | soft-AP +  |   +----------------+               |
|  | captive    |                                    |
|  | portal)    |                                    |
|  +------------+                                    |
+----------------------------------------------------+
        ^
        | (first boot, or after reset gesture)
        |
        v
  +-------------+         HTTP (192.168.4.1)
  | Phone /     |<------- captive-portal pop-up
  | Laptop      |-------> Save (SSID, password, sk-ant-...)
  +-------------+
```

**Pattern:** Single-MCU embedded device, single Arduino sketch, two operational modes:

1. **Provisioning mode** — Soft-AP + captive portal (WiFiManager). Active on first boot or after NVS reset.
2. **Station mode** — Joined to user's WiFi. Periodic Anthropic poll → render to display.

**Lineage:** M5Clawd is a copy-and-strip of the user's `Crypto_Coin_TickerUS_Stock` project (see [ADR 006](decisions/006-start-from-crypto-ticker.md)). The WiFiManager onboarding, M5Stack display init, button handling, and `Preferences` persistence are reused largely intact; the crypto/stock/chart logic is removed and replaced with the Anthropic poller.

---

## Project Layout

M5Clawd is an **Arduino sketch**, not a PlatformIO project. Arduino concatenates all `.ino` files in a sketch folder at build time, so we split the code into logical "tabs":

```text
m5clawd/
├── m5clawd.ino             main sketch — setup(), loop(), mode selection
├── wifi_portal.ino         WiFiManager setup, getParam(), saveParamCallback(), callbacks
├── poller.ino              Anthropic HTTPS poll + rate-limit header parsing
├── ui.ino                  three screens (splash, usage, status), M5.Lcd drawing
├── secrets_store.ino       Preferences read/write for the API key; reset gesture
├── config.h                compile-time constants (poll interval, AP prefix, version)
├── WiFiManager.cpp / .h     vendored tzapu WiFiManager (copied from the crypto ticker)
├── wm_consts_en.h           vendored WiFiManager support
├── wm_strings_en.h          vendored WiFiManager support
├── strings_en.h             vendored WiFiManager support
├── data/                    SPIFFS assets — splash / status PNGs
├── setup-toolchain.sh       installs arduino-cli + ESP32 core 1.0.4 + libraries
└── flash.sh                 arduino-cli compile + upload (+ SPIFFS upload)
```

> The crypto ticker shipped a single ~190 KB `.ino`. During the copy-strip we split it into the tabs above so the project stays navigable. See [ADR 006](decisions/006-start-from-crypto-ticker.md).

---

## Components

### Boot / Mode Selector

- **Location:** `m5clawd.ino` — `setup()`
- **Tech:** Arduino-ESP32 `setup()` lifecycle; `M5.begin()` for hardware init
- **Responsibility:** Init M5Stack hardware and SPIFFS; open `Preferences`; if no WiFi creds present, enter provisioning mode; otherwise station mode.

### WiFiManager (Provisioning)

- **Library:** tzapu/WiFiManager — **vendored in-tree** (`WiFiManager.cpp/.h` + `wm_*` files), copied from the crypto ticker
- **Tech:** Soft-AP + captive DNS + Arduino `WebServer`
- **Responsibility:** Serve the config page; collect WiFi creds (built-in) and the Anthropic API key (one custom `WiFiManagerParameter`); persist to `Preferences`; reboot into station mode.
- **Reused pattern from the crypto ticker** (`wifi_portal.ino`):
  - `WiFiManager wifiManager;` global
  - `getParam(name)` — reads a field from `wifiManager.server->arg(name)`
  - `saveParamCallback()` — on Save, pulls fields via `getParam()`, writes to `Preferences`
  - `onConfigModeCallback()` — on portal open, paints a setup screen on the LCD
  - `WiFiEvent()` — swaps LCD status art on AP station connect/disconnect
  - `wifiManager.autoConnect("M5Clawd-XXXXXX")`, `setClass("invert")` dark theme, custom menu
- **Difference from the crypto ticker:** The ticker registered ~52 `WiFiManagerParameter`s (25 pairs + 25 names + timezone + language). M5Clawd registers **one** — the API key.

### Preferences Store (NVS)

- **Library:** Arduino `Preferences.h` (over the ESP32 NVS partition)
- **Namespace:** `m5clawd` (the crypto ticker used `settings`; we use our own to avoid any collision if both ever share a device)
- **Keys:**
  - `anthropic_key` — `sk-ant-...` (string)
  - `wifi_configured` — bool flag, gates provisioning vs station mode
  - `poll_interval_s` — uint, default 60 (future: portal-tunable)
  - `last_session_pct`, `last_weekly_pct`, … — last-known-good usage, for display when WiFi is down at boot
- **WiFi SSID/password** are managed by WiFiManager's own persistence (`WiFi.persistent(true)` internals), not by us directly.
- **Reset:** Long-press button C for 5 s → `prefs.clear()` + `wifiManager.resetSettings()` + `ESP.restart()`. (Mirrors the crypto ticker's reset path.)

### Poller

- **Location:** `poller.ino`
- **Tech:** `WiFiClientSecure` + `HTTPClient` (Arduino-ESP32 stdlib). **No WebSockets** — the crypto ticker streamed Binance over `WebSocketsClient`; M5Clawd does a simple one-shot HTTPS POST.
- **Responsibility:** Every `poll_interval_s`, open a TLS connection to `api.anthropic.com:443`, POST a minimal request, parse rate-limit headers, hand a `UsageData` to the UI.
- **Failure handling:** Exponential backoff (60s → 120s → 240s → 300s cap). Surface an error category to the UI.

### UI Engine

- **Location:** `ui.ino`
- **Tech:** `M5.Lcd.*` — the classic `M5Stack.h` display API (TFT_eSPI-backed). `drawString`, `fillRect`, `setTextFont`, `drawPngFile` from SPIFFS.
- **Responsibility:** Three screens — Splash, Usage, Status. Redraw only the changing fields on the Usage screen to avoid full-screen flicker.
- **No LVGL, no M5GFX.** Direct `M5.Lcd` drawing, exactly as the crypto ticker does.

### Button Handler

- **Location:** `m5clawd.ino` loop + `ui.ino`
- **Tech:** Classic `M5Stack.h` — `M5.update()` each loop, then `M5.BtnA/B/C.wasPressed()` / `.pressedFor(ms)`
- **Mapping (MVP):**
  - **A** (left) — cycle screens (Splash ↔ Usage ↔ Status)
  - **B** (middle) — force-refresh poll
  - **C** (right) — short press: toggle backlight; long-press 5 s: reset NVS (with on-screen confirmation)

---

## Data Flow

### Provisioning flow (first boot, or post-reset)

```text
power on
  -> M5.begin() -> LCD + SPIFFS init
  -> Preferences.begin("m5clawd") -> check "wifi_configured" flag
  -> not set -> wifiManager.autoConnect("M5Clawd-XXXXXX")
       -> onConfigModeCallback() paints "Configure WiFi" + QR on the LCD
       -> AP up, captive DNS up, WebServer on 192.168.4.1
  -> user submits the form (SSID, password, API key)
  -> saveParamCallback():
        prefs.putString("anthropic_key", getParam("anthropic_key"))
        prefs.putBool("wifi_configured", true)
  -> WiFiManager restarts the ESP -> next boot enters station flow
```

### Station flow (normal operation)

```text
power on
  -> M5.begin() -> LCD + SPIFFS init
  -> Preferences.begin("m5clawd")
  -> read anthropic_key (cache in RAM)
  -> WiFi.begin() with WiFiManager-stored creds
  -> UI: "Connecting to <ssid>..."
  -> on WL_CONNECTED: kick off first poll immediately
  -> poller loop (every poll_interval_s):
       1. WiFiClientSecure client; client.setCACert(root_pem);
       2. HTTPClient http; http.begin(client, "https://api.anthropic.com/v1/messages");
       3. addHeader: x-api-key, anthropic-version, content-type
       4. http.POST({"model":"claude-haiku-4-5","max_tokens":1,
                     "messages":[{"role":"user","content":"."}]})
       5. read response headers:
            anthropic-ratelimit-unified-5h-utilization
            anthropic-ratelimit-unified-week-utilization
            anthropic-ratelimit-unified-5h-reset
            anthropic-ratelimit-unified-week-reset
       6. build UsageData; persist last-known-good to Preferences
       7. ui_render(data)
       8. http.end(); client.stop(); wait poll_interval_s
```

---

## Data Model

### `UsageData` struct

```c++
struct UsageData {
  uint8_t  session_pct;        // 0..100 — 5-hour rate-limit utilization
  uint8_t  weekly_pct;         // 0..100 — weekly rate-limit utilization
  uint32_t session_reset_s;    // seconds until the 5h window resets
  uint32_t weekly_reset_s;     // seconds until the weekly window resets
  uint32_t last_poll_unix;     // epoch of the last successful poll
  enum Status {
    UNKNOWN, OK, WIFI_DOWN, API_UNREACHABLE, AUTH_FAILED, RATE_LIMITED,
  } status;
};
```

This is the only data shape in the firmware. Last-known-good values are mirrored into `Preferences` so a reboot while WiFi is down still shows something meaningful.

---

## API Design (outbound)

We are an HTTP client, not a server. The only outbound call:

```http
POST https://api.anthropic.com/v1/messages HTTP/1.1
x-api-key: sk-ant-...
anthropic-version: 2023-06-01
content-type: application/json

{"model":"claude-haiku-4-5","max_tokens":1,"messages":[{"role":"user","content":"."}]}
```

We use the response **headers**, not the body. The body is parsed (ArduinoJson 6.x) only enough to detect errors (e.g. `{"error":{"type":"authentication_error",…}}`).

**Why this exact request?** It's the cheapest way to get fresh rate-limit headers — Haiku is the cheapest model, 1-token output and single-character input minimize cost. This is the trick the Clawdmeter daemon uses; we do it on-device.

---

## Infrastructure

- **Hosting:** None — device runs standalone.
- **Build:** `arduino-cli` (sketch stays Arduino-IDE-compatible). See [ADR 001](decisions/001-tech-stack-selection.md).
- **CI/CD:** Initially none. Future: GitHub Actions running `arduino-cli compile`.
- **Build artifact:** A firmware `.bin` (flashed over USB-C) plus a SPIFFS image (`data/` PNGs).

---

## Tech Stack

| Layer            | Technology                              | Why                                                        |
| ---------------- | --------------------------------------- | ----------------------------------------------------------- |
| MCU              | ESP32-D0WDQ6 (M5Stack Core Basic)       | The user's device                                           |
| Framework        | Arduino-ESP32, board manager **1.0.4**  | Matches the crypto ticker (pinned — newer cores regressed) |
| Build / flash    | `arduino-cli`                           | Scriptable; sketch stays Arduino-IDE-openable               |
| Board HAL        | classic `M5Stack.h` (lib v0.3.1)        | Proven on Core Basic by the crypto ticker                   |
| Display          | `M5.Lcd.*` (TFT_eSPI-backed)            | Built into `M5Stack.h`; `drawPngFile` for SPIFFS art        |
| WiFi provisioning | tzapu/WiFiManager (vendored in-tree)   | The crypto ticker's exact, user-approved onboarding         |
| HTTP / TLS       | `HTTPClient` + `WiFiClientSecure`       | Stdlib; HTTPS POST to Anthropic                             |
| JSON             | ArduinoJson **6.17.3**                  | The version the crypto ticker pins                          |
| Storage          | `Preferences.h` (NVS)                   | Built-in; the crypto ticker's persistence approach          |
| Assets           | SPIFFS PNGs                             | `M5.Lcd.drawPngFile`, as the crypto ticker does             |
| Language         | C++ (Arduino dialect)                   | Whatever core 1.0.4 compiles                                |

---

## Security

- **Authentication (outbound):** Anthropic API key in the `x-api-key` header.
- **Encryption (in transit):** TLS 1.2/1.3 to `api.anthropic.com`, root cert pinned via `WiFiClientSecure::setCACert`.
- **Encryption (at rest):** **None in MVP.** API key is plaintext in NVS. See [ADR 005](decisions/005-secrets-storage-nvs.md) for the threat model and hardening path.
- **Inbound services:** None in station mode. The captive portal (provisioning mode) is HTTP-only on a private 192.168.4.x AP, active only for the ~2-minute onboarding window.
- **Logging:** The API key must **never** be printed to serial. Error reporting uses `sk-ant-***` redaction.

### Threat model summary

| Threat                                          | Mitigation                                                                  |
| ----------------------------------------------- | --------------------------------------------------------------------------- |
| API key extraction from a stolen device         | Recommend a scoped/low-limit key; reset gesture for fast rotation            |
| MITM during captive-portal onboarding           | Open AP is unencrypted; surfaced in README; ~2-min exposure window           |
| MITM during Anthropic polling                   | TLS with pinned root; no plaintext fallback                                  |
| Firmware tampering (reflash with hostile code)  | Out of scope for MVP. Future: ESP32 Secure Boot v2.                          |

---

## Monitoring

- **Local:** Serial at 115200 baud. Per-poll status line, no secrets: `[poll ok] session=42% weekly=18% next=60s heap=145KB`.
- **On-device:** Last-poll timestamp + status badge on the Usage screen.
- **Off-device:** None in MVP.

---

## Reference Projects

M5Clawd has two read-only reference projects (see [ADR 004](decisions/004-port-vs-fork.md)):

### `Crypto_Coin_TickerUS_Stock` — scaffolding source

`~/GIT/Crypto_Coin_TickerUS_Stock`. M5Clawd is copy-and-stripped from it ([ADR 006](decisions/006-start-from-crypto-ticker.md)). **Kept:** WiFiManager onboarding, M5Stack display/buttons, `Preferences`, SPIFFS PNG loading. **Stripped:** Binance WebSockets, candlestick charts, 25-pair management, SHT3X sensor, NeoPixel effects, SD-card config, M5StackUpdater, Timezone.

### `Clawdmeter/` — concept reference

[HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter). Different hardware (Waveshare ESP32-S3 AMOLED) and transport (BLE + host daemon). We borrow only the *idea* and the Anthropic poll trick — the exact rate-limit header names, confirmed against its daemon source. We do **not** use its firmware code.

---

**Last Updated:** 2026-05-15
**Architect:** Kevin Brice
