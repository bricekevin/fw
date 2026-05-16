# Phase 3 - Claude Code OAuth Onboarding + Token Refresh PRD

**Duration:** ~1.5-2 weeks (evening/weekend pace; research-gated)
**Status:** PLANNING
**Owner:** Kevin Brice

> **Phase renumbering.** `1_PROJECT_OVERVIEW.md`'s original roadmap reserved
> "Phase 3" for *Polish* (splash art, battery, audio). That work is not
> MVP-blocking; OAuth onboarding **is** (without it the device stops working
> within a day). So OAuth onboarding takes the Phase 3 slot and Polish becomes
> Phase 4. Update the overview's roadmap when this phase is accepted.

---

## Problem Statement

**Problem:** Phase 2 hardware testing proved the device's whole data path works
— but only with **Claude Code OAuth** auth, not a plain API key (a `x-api-key`
request returns 200 with none of the `anthropic-ratelimit-unified-*` headers).
The poller was switched to OAuth Bearer auth and verified on hardware (session
82% / weekly 5%). That fix exposed two new problems:

1. **OAuth access tokens expire (~24 h observed).** The device currently holds a
   single static access token — it stops polling within a day. The project's
   core promise, "unattended for 7+ days," is impossible without the device
   holding a **refresh token** and refreshing the access token itself.
2. **Onboarding is dev-grade.** Today a user must run a `security` Keychain
   command, copy a ~108-char OAuth token, and paste it into a captive portal.
   That is not a product. Users do not normally see this token.

**Why Now:** This is the gating work between "a firmware that demoed once" and
"a device you can actually set on your desk and forget." Phase 2 is otherwise
code-complete; this phase makes it real.

**Impact:** Every user — and the viability of the standalone-device concept
itself. The original Clawdmeter used a host daemon precisely because it had
live access to a self-refreshing credential; M5Clawd's standalone pivot must
now own OAuth + refresh, or it isn't a product.

---

## Objectives

**Primary Goal:** A user onboards the device once, through a flow that never
asks them to handle a raw long-lived token; the device then holds a refresh
token, refreshes its access token automatically, and polls unattended for 7+
days without intervention.

**Success Metrics:**
- Onboarding asks the user to paste at most a short code (or nothing, if a
  cleaner flow proves feasible) — never a 108-char token.
- The device performs a real token refresh on its own and keeps polling across
  an access-token expiry boundary (verified on hardware, not just in theory).
- 7-day unattended run: poll-success ≥ 99%, no manual re-onboarding.
- A revoked / unrefreshable token produces a clear on-screen "re-onboard"
  prompt, not a silent dead device.

---

## User Stories

### Story 1: Onboard without touching a token
**As a** user setting up the device
**I want** to authorize it with my Claude account through a normal login
**So that** I never have to find, copy, or understand an OAuth token.

**Acceptance:** onboarding presents a "Log in with Claude" step (a URL/QR to
authorize on claude.ai); the user authorizes and, at most, pastes a short
one-time code back; the device obtains its own tokens from there.

### Story 2: Set it and forget it
**As a** user who leaves the device running
**I want** it to keep working for days without my attention
**So that** it is a desk display, not a chore.

**Acceptance:** the device refreshes its access token before expiry; a 7-day
run needs zero manual steps.

### Story 3: Know when it needs me
**As a** user
**I want** a clear prompt if the device truly can't refresh (token revoked, or
I logged out of Claude elsewhere)
**So that** I'm not left guessing why numbers went stale.

**Acceptance:** an unrefreshable credential shows a distinct "re-onboard"
state with instructions, not a generic error.

---

## Requirements

### Functional Requirements
1. A research spike must establish Claude Code's OAuth parameters — client ID,
   authorize + token endpoints, scopes, PKCE method, and the refresh-grant
   request/response shape — and prove a refresh exchange works off-device,
   **before** any firmware is written against them.
2. An ADR must choose the onboarding mechanism (expected: authorize-URL +
   paste-back one-time code, mirroring `claude`'s headless login) and supersede
   ADR 003.
3. The firmware must store an access token, a refresh token, and an expiry
   timestamp in NVS (superseding the current single-string credential).
4. The firmware must refresh the access token automatically — proactively
   before expiry and/or reactively on a `401` — by POSTing the refresh grant
   over TLS, then persisting the new tokens.
5. Refresh-token rotation must be handled: if the grant returns a new refresh
   token, it replaces the stored one atomically.
6. A failed refresh (invalid/revoked refresh token) must transition to a
   distinct "re-onboard required" UI state.
7. Onboarding must be re-runnable to swap the credential without necessarily
   wiping WiFi credentials.

### Non-Functional Requirements
- **Security:** the refresh token is long-lived and high-value — more sensitive
  than a scoped API key. This phase must decide (ADR) whether NVS encryption is
  in-scope now or explicitly deferred, and must never log either token.
- **Reliability:** 7-day unattended run; refresh failures degrade gracefully.
- **Resilience:** a refresh attempt must not wedge the poll loop; clock skew
  must not cause a refresh storm.
- **Testability:** the expiry/refresh-decision logic is a pure, host-tested
  module (the `m5clawd/test/` harness pattern).

### System Constraints
- ESP32 / Arduino core 1.0.4; TLS via `WiFiClientSecure` (heap-heavy — the
  refresh call has the same heap discipline as the poller).
- The token endpoint may be on a different host than `api.anthropic.com`
  (e.g. `console.anthropic.com`) → may need a second pinned root CA.
- The OAuth flow is a beta (`anthropic-beta: oauth-2025-04-20`); Anthropic may
  change client/endpoints — pinning third-party OAuth internals is inherently
  fragile and must be documented as such.

---

## Scope

### In Scope
- OAuth research spike + decision ADRs.
- NVS credential model: access + refresh token + expiry.
- On-device token refresh (TLS refresh-grant call, rotation, scheduling).
- The chosen onboarding flow (authorize-URL + paste-back code, pending the spike).
- OAuth-specific UI states (re-onboard required, refreshing).
- A "change credential" path that doesn't force a full WiFi re-entry.
- Carry-over fix: harden `station_connect()` (the post-onboarding power-cycle bug).
- Host tests for the refresh-decision logic; hardware verification incl. a real
  refresh across an expiry boundary.

### Out of Scope (deferred)
- **Polish** — splash art, battery indicator, audio chime (now Phase 4).
- **A full OAuth authorization-code + redirect flow with the device as a
  registered redirect target** — unlikely to be permitted; the paste-back code
  flow is the realistic path. The spike confirms.
- **mDNS, web config beyond onboarding, multi-account.**

---

## Dependencies

### External Dependencies
- Phase 2 firmware (the OAuth Bearer poller) — done, on branch `phase-2-20260515`.
- Anthropic's Claude Code OAuth endpoints + client (beta; behaviour to be
  established by the spike).
- A Claude subscription account to test onboarding + refresh against.

### Prerequisites
- Phase 2 complete (poll path hardware-verified).

### Assumptions
- Claude Code's OAuth supports a headless / paste-back-code path (the `claude`
  CLI offers one when it can't open a browser — strong signal it exists).
- The refresh grant is a standard OAuth `refresh_token` grant to a token
  endpoint. **The spike must confirm — do not build against assumptions.**

---

## Risks & Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Anthropic's OAuth client can't be driven by a third-party device at all | LOW-MED | HIGH | Epic 1 spike is the first work; if it's a hard blocker, fall back to the one-shot pairing-helper option and reassess the standalone premise — surface to the user immediately. |
| Refresh-grant format / endpoints undocumented or shift | MED | MED | Pin what the spike finds; document fragility; keep the values in `config.h` constants so a change is a one-line fix. |
| Refresh token in plaintext NVS is a real exposure | MED | MED | ADR in Epic 1 decides NVS encryption in/out; at minimum document and recommend revoking on device loss. |
| Token endpoint on a new host needs another root CA | MED | LOW | Probe with `openssl s_client` during the spike (same method as Phase 2 Task 2.1); bundle the root. |
| Clock skew causes premature/looping refresh | LOW | MED | Refresh decision uses a margin; NTP already wired (Phase 2); cap refresh attempts with backoff. |

---

## Open Questions

- [ ] What exactly are Claude Code's OAuth client_id, authorize URL, token URL,
      and refresh-grant shape? (Resolved by Epic 1 spike.)
- [ ] Is a paste-back one-time-code onboarding viable, or is a pairing helper
      needed? (Resolved by Epic 1 → ADR.)
- [ ] Does refreshing rotate the refresh token each time? (Resolved by 1.2.)
- [ ] Is NVS encryption in-scope for this phase? (Resolved by Epic 1 ADR.)
- [ ] Token-leak follow-up: the user's Phase-2 OAuth tokens were exposed in a
      transcript — confirm they were rotated.

---

**Approved By:** Kevin Brice
**Updated:** 2026-05-15
