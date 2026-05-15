# ADR 006: Start from a Copy-and-Strip of the Crypto Ticker

**Date:** 2026-05-15
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [001-tech-stack-selection](001-tech-stack-selection.md), [003-onboarding-via-wifimanager](003-onboarding-via-wifimanager.md), [004-port-vs-fork](004-port-vs-fork.md)

---

## Context

After the 2026-05-14 kickoff, the plan was to build `m5clawd/` from scratch on a PlatformIO + M5Unified stack ([ADR 001, original](001-tech-stack-selection.md)).

On 2026-05-15 the user pointed us at an existing project: **`Crypto_Coin_TickerUS_Stock`** (`~/GIT/Crypto_Coin_TickerUS_Stock`) — a cryptocurrency / US-stock price ticker that **runs on the user's M5Stack Core Basic** and **uses tzapu/WiFiManager exactly the way the user wants onboarding to work**.

Inspection confirmed the crypto ticker contains, working and proven on the target hardware:

- A complete **WiFiManager captive-portal flow**: `WiFiManager wifiManager;`, `WiFiManagerParameter` custom fields, a `getParam()` helper, `saveParamCallback()` persisting to `Preferences`, `onConfigModeCallback()` painting the LCD, `WiFiEvent()` swapping status images, `autoConnect("SETUP PRICE TICKER")`, dark-theme `setClass("invert")`, custom menu.
- **M5Stack Core display + button** handling via classic `M5Stack.h` (`M5.Lcd.*`, `M5.update()`, `M5.BtnA/B/C`).
- **`Preferences`-based persistence** of all settings.
- **SPIFFS PNG assets** drawn with `M5.Lcd.drawPngFile`.
- A **working flash toolchain** (Arduino IDE, ESP32 board manager 1.0.4, M5Stack lib 0.3.1) the user already operates.

The crypto ticker also carries a lot we do **not** want: ~39 `.ino` variants and stale sketches, ~29 ad-hoc build/flash shell scripts, a bundled `flash_download_tool_3.9.6/`, Binance.US WebSocket streaming, candlestick chart rendering, 25-trading-pair management, an SHT3X temperature sensor, NeoPixel effects, and SD-card config.

The question: do we keep building M5Clawd from scratch, or start from this proven project?

---

## Decision

**We will create `m5clawd/` as a copy-and-strip of `Crypto_Coin_TickerUS_Stock`.**

Concretely:

1. **Copy** the *single canonical* sketch directory `Crypto_Coin_TickerUS_Stock.ino/` and its in-tree vendored libraries into a clean `m5clawd/` — **not** the whole repo (no 29 shell scripts, no flash tool, no stale sketches, no `crypto-1-0/` binaries).
2. **Keep** (the scaffolding worth reusing):
   - The WiFiManager integration: `WiFiManager.cpp/.h`, `wm_consts_en.h`, `wm_strings_en.h`, `strings_en.h`
   - The `getParam()` / `saveParamCallback()` / `onConfigModeCallback()` / `WiFiEvent()` pattern
   - M5Stack display init and button handling
   - `Preferences` persistence wiring
   - SPIFFS PNG asset loading
3. **Strip** (everything crypto-ticker-specific):
   - `WebSocketsClient` and all Binance.US streaming code
   - Candlestick / chart rendering
   - The 25 trading-pair / name `WiFiManagerParameter` fields (→ replaced by a single Anthropic API-key field)
   - SHT3X temperature/humidity sensor (`SHT3X.cpp/.h`)
   - `Adafruit_NeoPixel` LED-bar effects (may return as a Phase 3 nice-to-have)
   - SD-card config (`ccticker.cfg`, `SDConfig`) — all config now flows through the captive portal
   - `M5StackUpdater` SD-menu loader
   - `Timezone` library (unless a future phase wants local-time display)
4. **Add** the M5Clawd-specific logic:
   - One `WiFiManagerParameter` for the Anthropic API key
   - An HTTPS poller (`HTTPClient` + `WiFiClientSecure`) hitting `api.anthropic.com`
   - Rate-limit-header parsing → `UsageData`
   - The three M5Clawd screens (splash, usage, status)
5. **Do not** copy the crypto ticker's `.git/` — `m5clawd/` is fresh code under this project's (future) git root, not a continuation of the crypto ticker's history.

The crypto ticker repo itself stays **untouched and read-only**, exactly like `Clawdmeter/` (see [ADR 004](004-port-vs-fork.md)).

---

## Alternatives Considered

### Option 1: Build from scratch (the original plan)

**Description:** Write `m5clawd/` clean on PlatformIO + M5Unified, referencing the crypto ticker only by reading it.

**Pros:**
- Cleanest possible result — no inherited cruft.
- Free choice of modern toolchain.

**Cons:**
- Re-implements a WiFiManager flow that already works perfectly on this exact hardware. Pure waste.
- Every re-implementation is a chance to reintroduce a bug the crypto ticker already solved (captive-portal callbacks, `Preferences` lifecycle, M5 button debouncing).
- Slower to first working flash.

**Rejected because:** The crypto ticker's WiFiManager onboarding *is the thing the user explicitly likes*. Re-deriving it from scratch discards proven, wanted code.

### Option 2: Fork the crypto ticker repo in place

**Description:** `git clone` the crypto ticker, branch it, evolve M5Clawd inside its repo and history.

**Pros:**
- Inherits git history.
- No copy step.

**Cons:**
- Inherits ~39 stale `.ino` files, 29 shell scripts, the flash tool, binaries — an enormous cleanup tax.
- The history is crypto-ticker history; it tells M5Clawd's story misleadingly.
- The crypto ticker is the user's *working device firmware* — evolving M5Clawd in the same repo risks the user's running ticker.

**Rejected because:** Same logic as [ADR 004](004-port-vs-fork.md) rejected forking Clawdmeter — the cruft and misleading history outweigh the history-preservation benefit.

### Option 3: Extract just the WiFiManager files, build the rest fresh

**Description:** Copy only `WiFiManager.*` + `wm_*` and the `getParam`/`saveParamCallback` snippet; write everything else new.

**Pros:**
- Less inherited cruft than a full sketch copy.

**Cons:**
- The M5Stack display init, button handling, and `Preferences` wiring are *also* worth reusing and are entangled with `setup()`.
- Drawing the keep/strip line at "just WiFiManager" means re-deriving the rest — partially re-incurring Option 1's cost.

**Rejected because:** The strip list is short and mechanical; copying the whole canonical sketch and deleting is faster and lower-risk than cherry-picking fragments.

---

## Consequences

### Positive

- **Fastest path to a flashing device.** Strip down a working sketch rather than grow one from nothing.
- **The wanted WiFiManager UX is preserved verbatim** — the user already approves of it.
- **Proven hardware bring-up.** Display init, button reads, `Preferences`, SPIFFS — all already known-good on this device.
- **`m5clawd/` ends up *simpler* than the crypto ticker** — we delete more than we add (no WebSockets, no charts, no SD, no sensor).

### Negative

- **We inherit the crypto ticker's coding style**, including a single ~190 KB `.ino` file. Mitigation: during the strip, split into a few `.ino`/`.h` tabs (Arduino concatenates `.ino` files in a sketch folder) so M5Clawd is navigable.
- **Inherited old dependency pins** (board 1.0.4, M5Stack 0.3.1, ArduinoJson 6.17.3). Documented in [ADR 001](001-tech-stack-selection.md); accepted.
- **Provenance / licensing:** the crypto ticker has no clear license. The reused WiFiManager is MIT (tzapu). The crypto-ticker-original glue code we keep is the user's own work, so the user may license M5Clawd freely — but this should be confirmed before any public release.
- **Manual divergence from the crypto ticker.** If the user fixes a WiFiManager bug in the ticker later, it won't auto-propagate. Acceptable — low-churn code.

### Neutral

- The crypto ticker's `data/` PNGs are crypto-branded; M5Clawd needs its own splash/status art. The *mechanism* (`drawPngFile` from SPIFFS) is reused; the *assets* are replaced.
- M5Clawd now has **two** read-only reference projects: `Clawdmeter/` (the concept) and `Crypto_Coin_TickerUS_Stock` (the scaffolding source).

---

## Implementation

**Timeline:** Phase 1, task 1 — before any feature work.

**Steps:**

1. Create `m5clawd/`; copy `Crypto_Coin_TickerUS_Stock.ino/Crypto_Coin_TickerUS_Stock.ino.ino` + vendored `WiFiManager.*`, `wm_*`, `strings_en.h` into it
2. Rename the sketch (the `.ino` must match its folder name for Arduino) to `m5clawd.ino`
3. Delete the strip-list items (WebSockets, charts, pairs, SHT3X, NeoPixel, SD config, updater, Timezone)
4. Confirm the stripped sketch still compiles with `arduino-cli`
5. Replace the 25 pair/name `WiFiManagerParameter`s with one API-key field
6. Verify the captive portal still renders and `saveParamCallback` persists the key
7. Hand off to Phase 2 for the Anthropic poller

**Dependencies:**

- `arduino-cli` toolchain set up per [ADR 001](001-tech-stack-selection.md)
- Read access to `~/GIT/Crypto_Coin_TickerUS_Stock` (the copy source)

**Risks:**

- **The strip breaks compilation in non-obvious ways** (crypto code referenced from `setup()`/`loop()` in tangled spots). Mitigation: strip incrementally, compiling after each removal, so breakage is localized.
- **The single huge `.ino`** is hard to navigate while stripping. Mitigation: split into tabs early (`m5clawd.ino`, `wifi_portal.ino`, `poller.ino`, `ui.ino`).

---

## Validation

**How will we know this was the right call?**

- A stripped, compiling `m5clawd.ino` exists at the end of Phase 1 task 1, noticeably *smaller* than the crypto ticker's sketch.
- The captive portal still works after the strip with zero changes to WiFiManager itself.
- We did not re-debug display init, button handling, or `Preferences` — they "just worked" because they were copied intact.

**Review Date:** End of Phase 1

---

## References

- Copy source: `~/GIT/Crypto_Coin_TickerUS_Stock/Crypto_Coin_TickerUS_Stock.ino/`
- [ADR 004: Port to sibling repo, do not fork](004-port-vs-fork.md) — same reasoning, applied to Clawdmeter
- [ADR 003: Captive-portal onboarding](003-onboarding-via-wifimanager.md) — the WiFiManager pattern being reused

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
