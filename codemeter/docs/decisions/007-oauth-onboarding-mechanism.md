# ADR 007: OAuth Onboarding via Authorize-URL + Paste-Back Code

**Date:** 2026-05-15
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [003-onboarding-via-wifimanager](003-onboarding-via-wifimanager.md) (supersedes the API-key portion), [008-refresh-token-storage](008-refresh-token-storage.md), [002-wifi-vs-ble-daemon](002-wifi-vs-ble-daemon.md)
**Supersedes:** ADR 003's "custom field for the Anthropic API key" step. ADR 003's WiFi-credential collection via WiFiManager still stands.

---

## Context

Phase 2 hardware testing established that the `anthropic-ratelimit-unified-*`
headers M5Clawd needs are a **Claude Code OAuth** feature — a plain `x-api-key`
request does not surface them (see HANDOFF_NOTES Session 7). The poller now
authenticates with an OAuth Bearer token.

That broke the ADR 003 onboarding model in two ways:

1. **OAuth access tokens expire (~24 h observed).** A static token cannot drive
   a "set it and forget it" desk display. The device must hold a **refresh
   token** and refresh itself (the refresh client is Epic 2; its storage is
   ADR 008).
2. **The credential is no longer pasteable.** ADR 003's plan — a captive-portal
   text field for the secret — assumed an `sk-ant-` API key. The OAuth access
   token is 108 characters and, critically, the user never normally sees it
   (it lives in the OS keychain, managed by the `claude` CLI). Asking a user to
   run a `security find-generic-password` command and paste a 108-char string
   is a developer workaround, not a product.

The device must therefore obtain its **own** OAuth credential through a real
login flow. The Epic 1 spike (recorded in `docs/Phase 3/PHASE_IMP.md`)
established the relevant OAuth parameters:

- `client_id` `9d1c250a-e61b-44d9-88ed-5944d1962f5e` — a **public** client
  (`token_endpoint_auth_method: none`); PKCE is the only client proof.
- Authorize URL `https://claude.com/cai/oauth/authorize` (the claude.ai /
  subscription authorize endpoint — M5Clawd reads subscription "unified" limits).
- Token URL `https://platform.claude.com/v1/oauth/token`.
- A **manual redirect** `https://platform.claude.com/oauth/code/callback`: after
  the user authorizes, claude.ai shows the authorization code on a success page
  for the user to copy. This is exactly the path the `claude` CLI uses for
  headless login (no local browser).
- The metadata-document client also registers **loopback** redirect URIs
  (`http://localhost/callback`) — those require the browser and the OAuth client
  to be the *same machine*, which a separate headless device is not.

The constraint to optimise for remains **"easy setup once cloned"**, and the
project is **standalone — no host computer, no companion app** (ADR 002).

---

## Decision

**Onboarding obtains the device's OAuth credential with an authorization-code +
PKCE flow whose redirect is the manual paste-back code page. The user authorizes
on their own phone/laptop and pastes one short code back into the device's
configuration portal. The device never asks for a raw long-lived token.**

This is the same flow the `claude` CLI uses when it cannot open a browser — a
flow Anthropic already supports for non-browser clients.

### Flow

1. **Stage 1 — WiFi (unchanged from ADR 003).** First boot with no credentials
   starts the WiFiManager soft-AP `M5Clawd-XXXXXX`; the user joins it and enters
   home-WiFi SSID + password. The API-key field from ADR 003 is removed.
2. **Device joins the home network.** The OAuth exchange needs internet, so it
   runs only after the station link is up. The device stays in **concurrent
   AP+STA mode** (`WIFI_AP_STA`) during onboarding: the soft-AP keeps the
   configuration portal reachable while the station link provides internet for
   the token exchange. (This avoids forcing the user to switch their phone
   between networks mid-flow.)
3. **Stage 2 — "Log in with Claude".** The device generates a PKCE
   `code_verifier` (random, kept in RAM for the session) and
   `code_challenge = base64url(SHA-256(verifier))`, builds the authorize URL
   (`client_id`, `response_type=code`, `redirect_uri` = the manual callback,
   `scope=user:inference user:profile`, `code_challenge`,
   `code_challenge_method=S256`, a random `state`), and presents it on the
   configuration portal as a tappable link **and** a QR code (also mirrored on
   the LCD via `M5.Lcd.qrcode()`).
4. **User authorizes.** They open the URL on their phone/laptop, sign in to
   their Claude account, approve. claude.ai redirects to the success page, which
   **displays a short authorization code**.
5. **Paste-back.** The user pastes that code into a single field on the device
   portal. The device POSTs `grant_type=authorization_code` + `code` +
   `code_verifier` + `client_id` + `redirect_uri` to the token endpoint and
   receives an **access token + refresh token + expiry**.
6. **Persist + run.** Tokens are stored in NVS per ADR 008; the device leaves
   onboarding, drops the soft-AP, and starts polling.
7. **Re-onboard / change credential.** A dedicated gesture re-runs **only**
   Stage 2 (new OAuth credential) without wiping WiFi credentials. The ADR 003
   button-C 5 s full reset still wipes everything.

The pasted artefact is a short-lived authorization **code**, not a token — an
order of magnitude shorter than the 108-char token ADR 003's plan would have
required, single-use, and useless without the device's in-RAM PKCE verifier.

---

## Alternatives Considered

### Option 1: Loopback redirect, device runs a local HTTP server

**Description:** Use the metadata-document client's registered
`http://localhost/callback` redirect; catch the code on a local server.

**Pros:** No copy-paste — the redirect delivers the code automatically.

**Cons:** `localhost` resolves to the **browser's** machine, not the device.
The user's phone redirecting to `http://localhost/callback` hits the phone, not
M5Clawd. The loopback flow is designed for a CLI and browser on one machine; a
separate headless device cannot be the redirect target. The device is not a
registered public redirect URI and is unlikely to be permitted as one (PRD,
out-of-scope).

**Rejected because:** Technically cannot work for a separate device.

### Option 2: One-shot pairing helper script

**Description:** A Mac/PC script reads the `claude` keychain credential
(including its refresh token) and pushes it to the device over USB or the LAN.

**Pros:** Zero typing; delivers a real refresh token directly.

**Cons:** Requires a host computer and running a script — contradicts the
standalone premise (ADR 002). Worse, it makes the device **share the user's
personal `claude` refresh token** rather than holding its own: both the Mac and
the device would then hold the *same* refresh token, and whichever refreshes
first may rotate it and silently break the other (rotation is assumed — see
ADR 008 / PHASE_IMP Task 1.2). Independent credentials are a correctness
requirement, not a nicety.

**Rejected** as the primary mechanism. Retained only as a possible emergency
fallback if Anthropic's headless flow becomes unavailable.

### Option 3: Keep manual token paste (the Phase 2 dev workaround)

**Description:** Ship the current behaviour — the user extracts the keychain
token and pastes 108 characters into the portal.

**Pros:** Already works; zero new firmware.

**Cons:** The token expires in ~24 h, so this fails the core "7-day unattended"
promise outright — it provides no refresh token at all. And it is not a product
(see Context).

**Rejected because:** It does not solve the expiry problem and is not shippable.

### Option 4: Full authorization-code flow with the device as a registered redirect target

**Description:** Register the device (or a project-hosted redirect) so the code
is delivered without paste-back.

**Rejected because:** Out of scope per the PRD — third-party device redirect
registration is not something we control, and the paste-back code flow already
achieves the goal with one short code.

---

## Consequences

### Positive

- **The user never handles a token.** They do a normal "Log in with Claude" and
  paste one short code — the Story 1 acceptance criterion.
- **The device gets its own credential**, independent of the dev Mac's `claude`
  login. No shared-refresh-token rotation hazard (cf. Option 2).
- **Proven path.** This is the `claude` CLI's own headless-login flow; we are
  not inventing an OAuth interaction Anthropic doesn't already support.
- **No host computer, no app** — consistent with ADR 002. A phone browser is
  enough, exactly as ADR 003 intended for WiFi.
- **PKCE means the public `client_id` is not a secret** and the pasted code is
  worthless to an eavesdropper without the device's in-RAM verifier.

### Negative

- **We pin third-party OAuth-beta internals.** `client_id`, endpoints, and the
  `anthropic-beta: oauth-2025-04-20` behaviour are not a stable public contract;
  Anthropic may change them. Mitigation: keep every value as a `config.h`
  constant so a change is a one-line edit; document the fragility; the firmware
  fails *closed* into the "re-onboard required" state rather than bricking.
- **The authorize URL is long** (~200+ chars with PKCE params) — a dense QR on a
  320×240 LCD may be hard to scan. Mitigation: render the QR primarily in the
  phone-side configuration portal (full browser viewport); the LCD QR is a
  secondary aid; always show the tappable URL as well.
- **Two-stage onboarding is more steps than ADR 003's single page.** Mitigation:
  concurrent AP+STA keeps the user on one network the whole time; on-screen and
  in-portal prompts guide each step.
- **Paste-back is still a manual step.** It is one short code, not a token — an
  accepted, large improvement over the status quo, and the realistic ceiling
  given Option 1 cannot work.

### Neutral

- The PKCE `code_verifier` lives only in RAM for the onboarding session; if the
  device reboots mid-onboarding the user restarts Stage 2. Acceptable — the
  window is seconds to a couple of minutes.
- Concurrent AP+STA raises ESP32 RAM/heap pressure during onboarding only; the
  TLS-heavy token exchange happens once. Watch heap (same discipline as the
  poller), but onboarding is not the steady-state path.

---

## Implementation

**Timeline:** Phase 3, Epic 3 (built to this ADR).

**Steps (high level — see `docs/Phase 3/PHASE_IMP.md` Epic 3):**

1. Remove the ADR 003 API-key `WiFiManagerParameter`.
2. Add `oauth.ino`: PKCE generation (`mbedtls` SHA-256), authorize-URL builder,
   `authorization_code` token exchange.
3. Keep the WiFiManager portal alive in `WIFI_AP_STA` for Stage 2; add the
   authorize-URL/QR display and the single paste-back code field.
4. Sequence onboarding: WiFi first, then the OAuth step once STA is up
   (Task 3.3, which also folds in the `station_connect()` hardening).
5. Add the "change credential" gesture that re-runs only Stage 2 (Task 3.4).
6. Persist tokens per ADR 008.

**Dependencies:**

- `mbedtls` SHA-256 (bundled with Arduino-ESP32) for the PKCE challenge.
- `WiFiClientSecure` + `HTTPClient` for the token exchange (heap discipline as
  in `poller.ino`); token-host root CA per ADR 008 / PHASE_IMP Task 1.1
  (ISRG Root X1).
- `M5.Lcd.qrcode()` for the on-LCD QR aid.

**Risks:**

- **Anthropic changes the headless flow or `client_id`.** Pin in `config.h`;
  document; fail closed to "re-onboard required".
- **The authorize-code format is longer than hoped.** Spike Task 1.2 was
  deferred to on-device, so the exact code length/format is confirmed during
  Epic 3 first run; the paste field must not assume a length.

---

## Validation

**How will we know this was the right call?**

- A first-time user onboards end-to-end without ever seeing or pasting a token —
  WiFi, then "Log in with Claude", then one short code (PRD Story 1).
- The device holds its own refresh token afterwards and the dev Mac's `claude`
  login is unaffected.
- The "change credential" gesture swaps the OAuth credential without re-entering
  WiFi.

**Review Date:** End of Phase 3 (after Epic 5 hardware verification).

---

## References

- `docs/Phase 3/PHASE_IMP.md` — Task 1.1 OAuth-parameter findings
- `docs/Phase 3/PHASE_PRD.md` — Phase 3 requirements and user stories
- [RFC 7636 — PKCE](https://datatracker.ietf.org/doc/html/rfc7636)
- [RFC 8252 — OAuth 2.0 for Native Apps](https://datatracker.ietf.org/doc/html/rfc8252)
- [ADR 003](003-onboarding-via-wifimanager.md) — the WiFi-onboarding flow this extends

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
