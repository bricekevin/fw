# Design System

**Project:** M5Clawd
**Target panel:** 320×240 IPS LCD (ILI9342C), full color, no touch.

---

## Scope

M5Clawd has no web frontend, no responsive layout, no design tokens consumed by build tooling. This is a **firmware-rendered UI on a fixed 320×240 RGB565 panel**. Everything below is the canonical reference for screen layouts, color choices, and typography decisions that the firmware will hard-code.

---

## Brand alignment

We adopt the Anthropic / Claude visual language (per upstream Clawdmeter), keeping things readable on a small panel:

```text
Primary (Claude orange):     #DA7756   (RGB565: 0xDBAA)
Background (deep charcoal):  #1A1815   (RGB565: 0x18E3)
Surface (panel bg, slightly lighter): #26221E (RGB565: 0x21A3)
Text primary (warm white):   #F5F0E8   (RGB565: 0xFF9D)
Text secondary (warm gray):  #8A8780   (RGB565: 0x8C50)
Success (calm green):        #6EA47A   (RGB565: 0x6D2F)
Warning (amber):             #E0A050   (RGB565: 0xE52A)
Error (muted red):           #C25450   (RGB565: 0xC2AA)
```

**Rule:** Never use raw hex literals in `ui.ino` other than these constants. They live in `m5clawd/config.h` as `COLOR_PRIMARY`, `COLOR_BG`, etc. (RGB565 `uint16_t` values — what `M5.Lcd` draw calls expect).

**Why these:** Upstream uses Anthropic brand colors. We match for visual continuity, but slightly darken the background since IPS LCD blacks aren't as deep as upstream's AMOLED and we want to reduce eyestrain in a low-light room.

---

## Typography

The classic `M5Stack.h` library exposes `M5.Lcd` (TFT_eSPI-backed) with built-in numbered fonts plus the GFX "Free Fonts" via `Free_Fonts.h` (bundled — the crypto ticker includes it). We use only these — no custom font conversion in MVP.

| Role         | Font (M5.Lcd)                          | Size hint | Used for                              |
| ------------ | -------------------------------------- | --------- | ------------------------------------- |
| Display      | `M5.Lcd.setTextFont(7)` (7-seg, large) | ~48 px h  | Percentage numbers on the Usage screen |
| Header       | `FreeSansBold12pt7b` (via `setFreeFont`) | ~26 px h  | Screen titles                         |
| Body         | `M5.Lcd.setTextFont(2)`                | ~16 px h  | Labels, status text                   |
| Caption      | `M5.Lcd.setTextFont(1)`                | ~8 px h   | Footnotes (e.g., last-poll timestamp) |

**Notes:**

- Built-in font `7` is a 7-segment-style numeric font — great for the big `%` values, digits only.
- Free Fonts come from `Free_Fonts.h` (`#include` it, then `M5.Lcd.setFreeFont(FSSB12)` etc.) — the crypto ticker already bundles this header; reuse it.

**Rule:** No custom font conversion in MVP. If a future phase wants a custom typeface, convert it to a GFX font header with `fontconvert` — but only if a built-in font genuinely can't do the job.

---

## Spacing

Hand-set margins on a 320×240 panel — no spacing system per se, but consistent values:

```c++
constexpr int PAD_EDGE    = 8;   // outer margin from panel edge
constexpr int PAD_SECTION = 16;  // between major UI sections
constexpr int PAD_ITEM    = 4;   // between adjacent labels/values
```

Status bar at top is 18 px tall (Body font + 2 px padding).

---

## Screen Layouts

### 1. Splash (boot / idle)

```text
+----------------------------------+
|                                  |
|         [Clawd sprite]           |
|          (centered)              |
|                                  |
|              M5Clawd             |
|                                  |
+----------------------------------+
```

- Solid `COLOR_BG`.
- Phase 1: static "M5Clawd" text in `Font4`, primary color.
- Phase 3: Clawd splash — either static PNGs in SPIFFS (`M5.Lcd.drawPngFile`, the crypto ticker's mechanism) or a small frame sequence. A pixel-art Clawd is optional polish, not MVP.

### 2. Usage (primary operational screen)

```text
+----------------------------------+
|  WiFi(*) Updated 14:23      ↑ 12 |  <- status bar (Body, secondary text)
+----------------------------------+
|  SESSION                         |  <- header (Font4, primary)
|       42 %                       |  <- value (Font7, primary)
|  resets in 2h 14m                |  <- subtext (Body, secondary)
+----------------------------------+
|  WEEKLY                          |
|       18 %                       |
|  resets in 4d 7h                 |
+----------------------------------+
```

- Status bar shows WiFi state, last-poll time, and a small heap indicator.
- Two equal-height "cards" (no actual borders, just whitespace) for session and weekly.
- Numeric values dominate visually — that's the at-a-glance promise.

### 3. Status (config / diagnostics)

```text
+----------------------------------+
|  M5Clawd                         |
|  Firmware v0.0.1                 |
|  WiFi: HomeNet (RSSI -62 dBm)    |
|  IP: 192.168.1.42                |
|  API: ok  ·  last 14:23          |
|  Heap: 145 KB free               |
|  Uptime: 4h 12m                  |
|                                  |
|  Long-press C to reset           |
+----------------------------------+
```

- All `Body` size. Pure text dump for diagnostics.
- Shown when user presses A from Usage; also shown automatically if Status != OK for >2 polls.

### 4. Provisioning (captive-portal active)

```text
+----------------------------------+
|  Configure WiFi                  |
|                                  |
|  1) Connect your phone to        |
|     wifi: "M5Clawd-A1B2C3"       |
|                                  |
|  2) Browser opens automatically. |
|     Or visit:                    |
|     http://192.168.4.1           |
|                                  |
|  [   QR code for the AP SSID  ]  |
|                                  |
|  This portal closes after save.  |
+----------------------------------+
```

- QR code rendered via `M5.Lcd.qrcode(...)` (built into `M5Stack.h`). Encodes a `WIFI:` URI so iOS / Android can one-tap join.
- This screen is **only** shown in provisioning mode.

---

## State Indicators

Status badges in the top-right of the Usage screen:

| Icon / text   | Meaning                                  | Color           |
| ------------- | ---------------------------------------- | --------------- |
| `wifi`       | Connected, polling OK                     | `COLOR_SUCCESS` |
| `wifi…`      | Connecting                                | `COLOR_WARNING` |
| `wifi-`      | WiFi disconnected (showing stale data)    | `COLOR_ERROR`   |
| `api-`       | WiFi OK but Anthropic call failing        | `COLOR_ERROR`   |
| `auth!`      | API key rejected                          | `COLOR_ERROR`   |
| `rate!`      | We're being rate-limited                  | `COLOR_WARNING` |

**Why text-not-icons in MVP:** Icons require asset conversion (PNG → RGB565). Text is free and readable on a 320-pixel-wide panel. Phase 3 can swap to Lucide-style icons matching upstream's approach if desired.

---

## Animation Budget

- Usage screen: redraw only the **changing fields** (the % numbers and reset countdowns). Background and titles stay put — saves ~80% of pixel pushes per poll.
- Splash: 8 fps frame rate target for Phase 3 animations. Upstream runs 10–15 fps but with PSRAM-cached frames; we should benchmark before committing.
- No transition animations between screens — instant cuts. UI is utilitarian.

---

## Accessibility

- High-contrast: `#F5F0E8` text on `#1A1815` background = 14.7:1 contrast (WCAG AAA).
- No color-only signals: WiFi status uses an icon-glyph + a color, not color alone.
- Font sizes are dictated by the device (no zoom). Phase 3 may add a "large numbers only" mode (drop subtext) for users wanting glanceability over detail.

---

## Asset Pipeline (Phase 3+ only)

The crypto ticker stores PNGs in `data/` and uploads them to SPIFFS, then draws them with `M5.Lcd.drawPngFile(SPIFFS, "/name.png", x, y)`. M5Clawd reuses that mechanism.

For Phase 3 splash / status art:

1. Author or export PNGs sized for the 320×240 panel (or smaller, centered).
2. Drop them in `m5clawd/data/`.
3. `flash.sh` uploads the `data/` folder as a SPIFFS image alongside the firmware.
4. Draw with `M5.Lcd.drawPngFile(SPIFFS, "/splash.png", 0, 0)`.

If a pixel-art Clawd animation is wanted, a short PNG frame sequence cycled on a timer is the simplest path — no sprite/zoom machinery needed. Defer all of this until Phase 3; MVP ships with a static text splash.

---

**Last Updated:** 2026-05-15
