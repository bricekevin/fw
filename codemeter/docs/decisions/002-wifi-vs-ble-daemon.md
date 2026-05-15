# ADR 002: WiFi-Direct vs BLE + Host Daemon

**Date:** 2026-05-14
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [001-tech-stack-selection](001-tech-stack-selection.md), [003-onboarding-via-wifimanager](003-onboarding-via-wifimanager.md)

---

## Context

Upstream Clawdmeter uses a **two-piece architecture**:

1. A host daemon (Python on macOS, Bash on Linux) running on the user's laptop
2. The daemon polls Anthropic's `api.anthropic.com` over the host's network
3. The daemon pushes a small JSON payload to the ESP32 over BLE GATT

This works well for upstream because the device is a desk-side companion for a developer who is already using the host machine all day, and OAuth credentials are read from the host keychain or `~/.claude/.credentials.json`.

For the M5Clawd port we asked: should we keep this model, or make the device **standalone** (talk to Anthropic directly over WiFi)?

**Forces:**

- The user explicitly asked for "easy to setup once cloned" — minimal host-side software is a win.
- The M5Stack Core Basic is a self-contained device with a USB-C cable; expecting a sibling daemon process diminishes the "plug it in and go" appeal.
- The user has a Mac, but also has other Macs and might want to plug this into a charger on a shelf with no nearby laptop.
- Anthropic OAuth tokens (the kind upstream uses) require a host's keychain interaction. **Anthropic API keys (`sk-ant-…`) work fine on any device** that can do HTTPS.

---

## Decision

**We will run the polling loop on-device over WiFi, with no host daemon.**

Concretely:

1. The device joins the user's WiFi network (creds provided via captive portal — see ADR 003).
2. The device stores an **Anthropic API key** (`sk-ant-…`), also entered via captive portal, in NVS.
3. Every `POLL_INTERVAL_SECONDS` (default 60s), firmware:
   - Opens `WiFiClientSecure` to `api.anthropic.com:443`
   - POSTs a single-token `claude-haiku-4-5` request (same trick upstream uses to elicit usage headers cheaply)
   - Parses response headers for `anthropic-ratelimit-unified-5h-utilization` and `anthropic-ratelimit-unified-week-utilization`
   - Renders the result on the display
4. No daemon, no BLE, no host integration of any kind.

---

## Alternatives Considered

### Option 1: Keep BLE + host daemon (upstream model, ported)

**Description:** Port upstream's macOS Python daemon, adapt the BLE GATT service to fit on Core Basic, keep everything else the same.

**Pros:**
- Reuse most of upstream's daemon code as-is.
- API key never leaves the host — security win.
- Faster polling possible (host is online 24/7, device just renders).

**Cons:**
- Setup becomes "flash device" + "install daemon" + "pair via macOS Bluetooth" + "wait for daemon to discover MAC" — multi-step, error-prone (cf. upstream README's troubleshooting section).
- BLE bonding on macOS/Linux is the #1 source of upstream issues (per their `feedback_dbus_monitor_pipe` memory).
- Defeats the standalone shelf-device use case.
- The user already has a Mac per their setup, but explicitly asked for "easy to setup" — daemon install isn't.

**Rejected because:** The whole point of the port is to make this easy to deploy. Keeping the daemon retains all the upstream complexity without the upstream hardware benefits.

### Option 2: WiFi + BLE both (belt and suspenders)

**Description:** Run a BLE peripheral _and_ a WiFi client. Daemon writes to BLE if it's there, otherwise the device polls Anthropic itself.

**Pros:**
- Backwards-compatible with the upstream daemon.
- Two failure modes give resilience.

**Cons:**
- ESP32 WiFi + BT coexistence is real but constrained; complicates the radio scheduler.
- Doubles the surface area for bugs and the code we have to maintain.
- We don't have an upstream daemon for M5Clawd anyway — this just adds an unused code path.

**Rejected because:** Speculative complexity. If the user later wants a host-side override (e.g., to push notifications), we can add it then.

### Option 3: Run a tiny daemon on the user's local network (not host)

**Description:** A Docker container on a Raspberry Pi / NAS polls Anthropic, exposes results on the LAN, device fetches over plain HTTP.

**Pros:**
- API key kept in one place (the Pi), not on each device.
- Centralized poll-rate management.

**Cons:**
- Requires the user to run a Pi.
- Adds a new failure mode (LAN-side service down → device shows stale data).

**Rejected because:** Overkill for one user with one device. The user does not currently run a homelab service for this.

---

## Consequences

### Positive

- **One-step setup:** Flash → connect to AP → enter WiFi + API key → done. No host software.
- **Truly standalone:** Plug the M5Stack into a USB charger on a shelf; it works regardless of which laptop is on.
- **No host-side OS divergence:** Upstream's macOS/Linux split (Python vs Bash daemon) doesn't apply.
- **API key is portable:** Same key works from device, laptop, CI — no keychain integration to debug.

### Negative

- **API key lives on the device.** If someone steals or finds the device, they can extract the NVS partition and read it. Mitigated by:
  - Encouraging use of a dedicated **Anthropic project key** with low limits, scoped only to `claude-haiku-4-5` (cheapest model).
  - Documenting NVS-encryption setup as a future hardening step.
- **TLS handshake cost every poll** (~3–4s on ESP32 to `api.anthropic.com`). Mitigated by reusing the `WiFiClientSecure` connection between polls when possible.
- **Device needs WiFi.** A laptop-paired BLE device works offline (with daemon); a WiFi device on a flaky network shows "no data". We surface this clearly on screen.
- **Anthropic's CA chain becomes a firmware concern.** Need to bundle a current root certificate (Amazon Root CA 1 / DigiCert Global Root depending on edge) and refresh it if Anthropic rotates.

### Neutral

- **BLE HID buttons (Space, Shift+Tab) are removed.** Could be added back as a Phase 4+ feature; not in MVP.
- **Polling rate is now bound by API rate limits, not by daemon scheduling.** Default 60s leaves plenty of headroom.

---

## Implementation

**Timeline:** Captive portal + first poll = Phase 1; reliable polling loop + error UI = Phase 2.

**Steps:**

1. Phase 1: WiFiManager captive portal collects SSID + password + API key; saves to NVS; reboots into station mode
2. Phase 1: Single one-shot poll on boot, render result
3. Phase 2: Recurring poll loop with exponential backoff on failure
4. Phase 2: Status UI ("connecting…", "WiFi down", "API key invalid", "rate limited")
5. Phase 3+: TLS session reuse, OTA updates, optional BLE HID add-back

**Dependencies:**

- `WiFiClientSecure` with a bundled root cert (initial: `ISRG Root X1` and `Amazon Root CA 1`, the two most likely Anthropic edge CAs)
- `HTTPClient` from Arduino-ESP32
- `ArduinoJson 6.17.3` for response parsing (v6 API — matches the crypto ticker; see [ADR 001](001-tech-stack-selection.md))

**Risks:**

- **TLS heap pressure:** ESP32 SSL handshakes can fail with `out of memory` if heap is fragmented after long uptimes. Mitigation: tear down WiFiClientSecure between polls, add a watchdog-style heap check.
- **CA expiry:** If Anthropic rotates to a CA we don't bundle, firmware breaks. Mitigation: bundle 2+ likely roots; document OTA-update path before MVP ships.

---

## Validation

**How will we know this was the right call?**

- A user who just flashed the device can have it showing real usage data in **under 5 minutes** from first power-on.
- The device runs unattended for 7+ days with no human intervention (modulo WiFi outages).
- We don't end up wishing we had a daemon — meaning the "device is offline, what now?" UX is good enough.

**Review Date:** End of Phase 2

---

## References

- [Upstream Clawdmeter daemon source](../../Clawdmeter/daemon/)
- [Anthropic rate-limit headers docs](https://docs.anthropic.com/en/api/rate-limits)
- [WiFiClientSecure caveats](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/README.md)

---

**Last Updated:** 2026-05-14
**Updated By:** Kevin Brice
