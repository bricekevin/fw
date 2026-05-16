# ADR 003: Captive-Portal Onboarding via WiFiManager

**Date:** 2026-05-14
**Status:** Accepted — API-key step superseded by [ADR 007](007-oauth-onboarding-mechanism.md)
**Deciders:** Kevin Brice
**Related:** [002-wifi-vs-ble-daemon](002-wifi-vs-ble-daemon.md), [005-secrets-storage-nvs](005-secrets-storage-nvs.md), [007-oauth-onboarding-mechanism](007-oauth-onboarding-mechanism.md)

---

> **Superseded in part (2026-05-15).** Phase 2 testing showed the device needs a
> Claude Code **OAuth** credential, not an `sk-ant-` API key. The "custom field
> for the Anthropic API key" step below is replaced by the OAuth flow in
> [ADR 007](007-oauth-onboarding-mechanism.md). The WiFi-credential collection
> via WiFiManager (Stage 1) described here **still stands**.

---

## Context

Given ADR 002 (device polls Anthropic directly over WiFi), the device needs three secrets at runtime:

1. WiFi SSID
2. WiFi password
3. Anthropic API key (`sk-ant-…`)

We must collect these in a way that satisfies "easy to setup once cloned":

- **No recompile per secret.** Hardcoding credentials in `src/secrets.h` and rebuilding for every WiFi network is friction users will hate; it also means secrets land in `git status` traps.
- **No accompanying app.** A custom mobile app for provisioning is overkill.
- **Works on every phone and laptop OS.** No special drivers, no extra installs.

The de-facto solution on ESP32 is **captive-portal Wi-Fi provisioning**: the device becomes a temporary access point on first boot, the user connects to it, and a phone/laptop pops a "sign in to network" page where the user fills in real credentials.

---

## Decision

**We will use [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) as the captive-portal provisioning library, extended with one custom field for the Anthropic API key.**

Flow:

1. **First boot:** Firmware reads NVS. No WiFi creds present → starts soft-AP `M5Clawd-XXXXXX` (XXXXXX = last 6 of MAC). Screen shows AP SSID + a QR code to it.
2. **User connects** their phone to `M5Clawd-XXXXXX`. iOS / Android / macOS auto-pop a "Sign in" captive portal page; on Windows the user opens any browser and gets redirected.
3. **Configuration page** shows:
   - WiFi network selector (scanned + manual entry)
   - WiFi password field
   - **Custom field:** Anthropic API key (`sk-ant-...`), with a "Test" button that validates by hitting `api.anthropic.com` once
   - "Save" button
4. **On Save:** firmware writes SSID + password to WiFiManager's slots and the API key to a separate NVS namespace (see ADR 005). Device reboots into station mode.
5. **Subsequent boots:** Creds present → connect to WiFi → start polling. No portal.
6. **Reset gesture:** Hold **button C** for 5 seconds → clears NVS → reboots back to portal mode. Screen confirms before wiping.

---

## Alternatives Considered

### Option 1: Hardcoded `secrets.h`

**Description:** Ship `src/secrets.h.example`; user copies to `src/secrets.h` with real values before flashing.

**Pros:**
- Zero firmware complexity. No AP, no captive portal, no HTTP server.
- Smaller binary.

**Cons:**
- Recompile per credential change.
- Easy to commit `secrets.h` by accident (gitignore helps but isn't bulletproof).
- "Easy to setup once cloned" goal isn't met — user has to edit code before flashing.

**Rejected because:** It directly contradicts the project goal.

### Option 2: ESP-IDF SmartConfig / ESP-Touch

**Description:** Espressif's proprietary provisioning protocols. User installs an "EspTouch" app and broadcasts creds.

**Pros:**
- No AP mode needed (creds flow over UDP broadcast).
- Officially supported.

**Cons:**
- Requires the user to install an app per platform.
- Broken on many newer WiFi routers (band-steering, isolation, 802.11r).
- Doesn't give us a UI surface for the API key — we'd still need a captive portal for that, defeating the savings.

**Rejected because:** Adds friction (app install) without removing the captive-portal need.

### Option 3: ESP32 BLE Provisioning (`WiFiProv.h`)

**Description:** Use the Arduino-ESP32 `WiFiProv` library, which provisions over BLE with the official Espressif mobile app.

**Pros:**
- More secure than open AP (encrypted BLE channel).
- No HTTP server needed.

**Cons:**
- User must install Espressif's app.
- Still need a UI surface for the API key field — `WiFiProv` only handles WiFi.
- Brings BLE into the firmware just for provisioning; doubles the radio's role.

**Rejected because:** Mobile-app dependency contradicts "works on every phone and laptop".

### Option 4: Build our own captive portal from scratch

**Description:** Roll our own `WebServer` handler + DNS server + AP-mode toggle.

**Pros:**
- No external library — full control.
- Tailored exactly to our 3-field needs.

**Cons:**
- DNS redirection (the part that makes the captive portal "pop" automatically) is tricky, especially across iOS/Android quirks.
- WiFiManager already does this correctly; reinventing is busywork.

**Rejected because:** WiFiManager is mature, MIT-licensed, and has solved exactly this problem.

---

## Consequences

### Positive

- **First boot is self-explanatory:** `M5Clawd-XXXXXX` SSID + auto-pop captive portal is recognizable UX on every modern OS.
- **No app install.** Phone browser is enough.
- **No recompile for new networks or keys.** Reset gesture re-enters portal mode.
- **One library handles both WiFi creds and our custom API-key field.** WiFiManager's `WiFiManagerParameter` is built for exactly this.

### Negative

- **AP mode is open (unencrypted) by default.** Anyone in proximity during onboarding can sniff the form submission. Mitigation: portal is only active during the ~2-min onboarding window. We document this clearly. Future work: WPA2 the AP with a static password printed on the device case.
- **WiFiManager adds ~30KB binary size.** Acceptable.
- **DNS spoofing dependency.** WiFiManager runs a captive DNS server to force the portal pop. If the user's phone has DNS-over-HTTPS enabled aggressively, the auto-pop may not fire — they can still open `http://192.168.4.1` manually. We surface this URL on the device screen.

### Neutral

- **No bonjour/mDNS discovery in MVP.** Could be added later (`http://m5clawd.local`).
- **Captive portal serves over plain HTTP** (no TLS). Acceptable on a 192.168.4.x AP-mode network; the password never leaves the local link.

---

## Implementation

**Timeline:** ~2 days of Phase 1 work

**Steps:**

1. Vendor the tzapu WiFiManager files into the `m5clawd/` sketch folder (copied from the crypto ticker — it already vendors them this way)
2. On boot, check NVS for the `wifi_configured` flag
3. If unset: start WiFiManager with a custom `WiFiManagerParameter` for the API key; paint portal info on the LCD via `onConfigModeCallback`
4. In `saveParamCallback`: persist the API key to `Preferences`, set `wifi_configured = true`, reboot
5. If set: skip the portal, `WiFi.begin()` with stored creds
6. Implement the long-press button C handler (5s) → wipe NVS → restart
7. Display the AP-SSID QR code with `M5.Lcd.qrcode(...)` (built into `M5Stack.h`)

**Dependencies:**

- tzapu WiFiManager — vendored in-tree (not a package-managed dependency)
- `M5.Lcd.qrcode()` from the classic `M5Stack.h` library

**Risks:**

- **WiFi 6 / WPA3-only networks:** ESP32 (original) is WPA2 only. If the user's home network is WPA3-only, the device won't join. Mitigation: detect and show a clear error; document the requirement in README.
- **Captive portal pops differently per OS:** Test on iOS Safari, Android Chrome, macOS, Windows. If auto-pop fails, the on-screen URL is the fallback.

---

## Validation

**How will we know this was the right call?**

- A first-time user flashes the firmware and is showing live data in under 5 minutes, no docs consulted beyond the on-screen prompts.
- After NVS reset, the user can re-onboard without bricking the device.
- The captive portal works across at least one iOS device, one Android device, and one laptop (macOS or Windows).

**Review Date:** End of Phase 1

---

## References

- [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)
- [Espressif captive-portal example](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples)
- [Captive Network Assistant behavior on iOS](https://datatracker.ietf.org/doc/html/rfc8908)

---

**Last Updated:** 2026-05-14
**Updated By:** Kevin Brice
