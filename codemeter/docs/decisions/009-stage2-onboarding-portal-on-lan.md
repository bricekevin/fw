# ADR 009: Stage-2 OAuth Onboarding Portal on the Home LAN

**Date:** 2026-05-15
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [007-oauth-onboarding-mechanism](007-oauth-onboarding-mechanism.md) (refines its steps 2-3), [003-onboarding-via-wifimanager](003-onboarding-via-wifimanager.md)
**Refines:** ADR 007's steps 2-3 — the *surface* the onboarding portal is served on. ADR 007's decision (authorize-URL + PKCE + paste-back code) is unchanged.

---

## Context

ADR 007 step 2 specified that during onboarding the device "stays in concurrent
AP+STA mode ... the soft-AP keeps the configuration portal reachable while the
station link provides internet for the token exchange," and claimed this
"avoids forcing the user to switch their phone between networks mid-flow."

Implementing Epic 3 surfaced a flaw in that claim. An ESP32 soft-AP does **not**
NAT or route its station uplink to AP clients. A phone joined to the
`M5Clawd-XXXXXX` soft-AP therefore has **no internet** — it cannot open
`claude.com` to authorize. Concurrent AP+STA gives *the device* internet (its
own STA link) but does nothing for a phone on the AP. So ADR 007's flow, taken
literally, still forces a network switch — or worse, leaves the user on a portal
they cannot complete.

The constraint to optimise for remains **"easy setup once cloned"** (and ADR
002's standalone, no-app premise).

---

## Decision

**Onboarding is two staged portals on two different network surfaces, with a
reboot between them. Stage 1 (WiFi) runs on the soft-AP captive portal. Stage 2
(OAuth) runs as a web portal on the device's home-LAN IP — the soft-AP is
dropped once WiFi is up.**

### Flow

1. **Stage 1 — WiFi, on the soft-AP.** A fresh device raises the soft-AP
   `M5Clawd-XXXXXX` and runs the WiFiManager captive portal. The user joins the
   AP and enters home-WiFi credentials. On a successful save the device reboots.
2. **Reboot into station mode.** The device connects to the home network.
3. **Stage 2 — OAuth, on the home LAN.** Still holding no OAuth credential, the
   device serves the WiFiManager **web portal** (`startWebPortal()`, no soft-AP,
   no captive DNS) on its home-LAN IP. It mints a PKCE session and shows, on the
   LCD, the "Log in with Claude" authorize-URL QR **and the LAN portal address**.
4. The user — phone still on home WiFi, so it has internet — scans the QR,
   authorizes on `claude.com`, then opens the LAN portal address and pastes the
   one-time code. The device runs the code exchange (ADR 007) and reboots into
   normal operation.

The LCD presents the flow as **three steps, each with its own QR**: (1) join the
soft-AP, (2) open the soft-AP portal `192.168.4.1` (in case the captive portal
does not auto-open), (3) the "Log in with Claude" authorize URL.

### Why a reboot between stages, not one AP+STA session

The vendored tzapu WiFiManager exits `startConfigPortal()` once WiFi connects;
keeping one portal session alive across a WiFi-connect to then serve OAuth is
fragile. Two clean portal sessions — `startConfigPortal()` for Stage 1, then
`startWebPortal()` for Stage 2 after a reboot — reuse the library exactly as
intended and match the Phase-1-proven `while (!done) startConfigPortal()`
pattern. The reboot also gives Stage 2 a clean station connection.

---

## Alternatives Considered

### Option 1: Keep the soft-AP up through Stage 2 (ADR 007 as written)

Serve the Stage 2 portal on the soft-AP at `192.168.4.1` while STA is also up.

**Rejected because:** the phone on the soft-AP has no internet, so the user
must leave the AP to authorize on `claude.com` and rejoin to paste the code — a
round-trip network switch. The LAN-portal option keeps the phone on one network
(home WiFi) the entire time: it reaches both `claude.com` and the device.

### Option 2: Single AP+STA portal session, no reboot

Collect WiFi and OAuth in one `startConfigPortal()` session.

**Rejected because:** fighting WiFiManager's portal-exit-on-connect behaviour is
fragile and hard to verify; and it does not fix the no-internet-on-AP problem.

### Option 3: A companion app / pairing helper

**Rejected by ADR 002 / ADR 007** — the project is standalone, no host app.

---

## Consequences

### Positive

- **The phone stays on home WiFi the whole of Stage 2** — internet for
  `claude.com` and reachability of the device, no network switching.
- Each stage is a clean, independent WiFiManager session — robust, and the
  Stage 1 path is unchanged from the Phase-1-proven captive-portal flow.
- The post-reboot station connection is fresh, which dovetails with the
  `station_connect()` hardening folded into Epic 3 Task 3.3.

### Negative

- **The user must read the LAN IP off the LCD** to reach the Stage 2 portal
  (there is no captive-portal auto-redirect on the home LAN). Mitigation: show
  it large, and as a QR; mDNS is a possible future improvement (out of scope).
- **An extra reboot** in the onboarding path. Acceptable — onboarding is a
  one-time flow and the device already reboots after Stage 1.
- If the device is on the home network but the user's phone is on a *different*
  network (e.g. guest VLAN), the LAN portal is unreachable. Out of scope — the
  same-network assumption is reasonable for a desk device.

### Neutral

- Stage 2 runs only while the device holds no usable OAuth credential. A device
  with a valid credential boots straight to polling; a device whose credential
  died re-enters Stage 2 (this is also the Task 3.4 "change credential" path).

---

## Validation

- A first-time user completes onboarding with the phone never leaving home WiFi.
- Verified on hardware in Epic 5.2 (the multi-step onboarding run).

**Review Date:** End of Phase 3.

---

## References

- [ADR 007](007-oauth-onboarding-mechanism.md) — the onboarding mechanism this refines
- `docs/Phase 3/PHASE_TASKS.md` — Epic 3, Tasks 3.1-3.3
- tzapu WiFiManager — `startConfigPortal()` vs `startWebPortal()`

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
