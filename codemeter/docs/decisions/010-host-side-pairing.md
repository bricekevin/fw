# ADR 010: Host-Side Pairing — Credential Minted on a Computer, Scanned to the Device

**Date:** 2026-05-17
**Status:** Accepted
**Deciders:** Kevin Brice
**Supersedes:** [ADR 007](007-oauth-onboarding-mechanism.md) — the on-device OAuth onboarding flow.
**Revises:** [ADR 002](002-wifi-vs-ble-daemon.md) — its "no host computer" stance becomes "no host computer *at runtime*; a one-time host helper at setup."

---

## Context

Phase 3 built on-device OAuth onboarding per ADR 007: the device generated a
PKCE challenge, the user authorized in a phone browser, and pasted a one-time
code back into the device, which exchanged it for tokens at
`platform.claude.com/v1/oauth/token`.

Hardware testing (Sessions 17–19) found this **does not work**:

- After ~6 onboarding attempts, the token endpoint began returning
  `HTTP 429 {"error":{"type":"rate_limit_error"}}` to every request.
- The 429 persisted **20+ hours** and occurred from **two unrelated networks**
  (home WiFi and a cellular hotspot) — so it is not IP-based.
- The flow uses the Claude Code CLI's *private* OAuth `client_id` in a way
  Anthropic never sanctioned. A burst of failed exchanges from a non-CLI client
  is indistinguishable from an attack on the OAuth endpoint; the endpoint
  appears to have flagged the pattern.

ADR 007 explicitly listed this as a risk ("we pin third-party OAuth-beta
internals… not a stable public contract"). That risk has now materialised, and
it is not fixable with a firmware tweak — the *approach* (a DIY device
impersonating the CLI's OAuth client) is what fails.

---

## Decision

**The OAuth credential is minted on the user's computer, not on the device.**

1. **Host helper.** `pairing/m5clawd-pair.py` (cross-platform Python) wraps
   `claude setup-token` — Anthropic's **sanctioned** command for a long-lived
   authentication token (it exists precisely for headless/non-interactive use).
   The helper renders the resulting token as a QR code encoding
   `http://192.168.4.1/cred?t=<token>`, served at `http://localhost:8765`.
2. **Device ingest.** Onboarding is now **single-stage**. The soft-AP captive
   portal collects the home-WiFi credentials as before; the device also
   registers a custom `/cred` route (via WiFiManager's `setWebServerCallback`).
   The user scans the helper's QR with their phone's normal camera while still
   on the soft-AP — that opens `http://192.168.4.1/cred?t=…`, and the device
   stores the token. When both WiFi and a token are present, the device
   reboots into normal operation.
3. **No on-device OAuth.** The device never runs the authorize/PKCE/exchange
   flow. It holds the long-lived token and uses it directly as the poller's
   Bearer credential. It does not refresh — the token is long-lived; when it
   eventually expires the user re-pairs.

The phone cannot scan a QR with an in-page camera on the HTTP captive portal
(browsers block `getUserMedia` on non-HTTPS origins), so the QR encodes a URL
the **native camera app** opens — no in-page scanner, no HTTPS workaround.

---

## Alternatives Considered

- **Keep on-device OAuth, wait out the limit.** Rejected — 20+ h, multiple IPs,
  unsanctioned; we would be fighting this indefinitely.
- **Host daemon that fetches usage and pushes it to the device** (the original
  Clawdmeter model). Works and uses sanctioned auth, but requires the host to
  be running continuously. The pairing-helper approach needs the host only
  once, at setup — strictly better for this use case.
- **Native host app instead of a script.** Same auth story; a packaging/polish
  upgrade only. Not worth it for a personal device; a Python script is enough.

---

## Consequences

### Positive

- **Sanctioned auth.** `claude setup-token` is an Anthropic-provided command;
  no impersonation of the CLI's interactive OAuth client.
- **No rate-limit wall.** The device never calls the OAuth authorize/token
  endpoints, so it cannot trip their abuse protection.
- **Simpler device.** No PKCE, no authorize-URL builder, no code exchange, no
  token-refresh client needed on the ESP32.
- **Still standalone at runtime.** The host computer is needed once, to mint
  the token. After that the device runs alone, exactly as ADR 002 intended.

### Negative

- **A computer is needed at setup.** This revises ADR 002's "no host computer"
  — now scoped to runtime. A phone alone cannot complete setup.
- **The token is long-lived but not infinite.** When it expires the user must
  re-pair (re-run the helper, re-scan). No silent refresh.
- **Dead code.** Epic 2's refresh client (`oauth_refresh`, `refresh_policy`)
  and Epic 3's PKCE/exchange (`oauth_pkce`, `oauth_exchange_code`,
  `wifi_portal_oauth_stage`) are no longer used. Removing them is a follow-up
  cleanup commit.

### Neutral

- The credential is still stored in plaintext NVS ([ADR 008](008-refresh-token-storage.md)
  storage decision stands). The compensating control — revoke a lost device's
  access — still applies; the user revokes the `setup-token` credential.

---

## Implementation

- `pairing/m5clawd-pair.py` — the host helper (done).
- `pairing/index.html` — landing page for `encinitas3d.com/m5clawd` (done).
- Firmware: single-stage `wifi_portal_onboard()` with the `/cred` route;
  `wifi_portal_oauth_stage()` removed from the boot path.
- **Follow-up:** delete the now-dead OAuth-exchange / PKCE / refresh code.

**Review Date:** end of Phase 3.

---

**Last Updated:** 2026-05-17
**Updated By:** Kevin Brice
