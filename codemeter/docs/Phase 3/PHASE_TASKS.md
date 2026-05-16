# Phase 3 - OAuth Onboarding + Token Refresh Tasks

**Status:** 0/18 — planning
**Updated:** 2026-05-15

> See PHASE_PRD.md for requirements and PHASE_IMP.md for spike commands and
> code intent. See HANDOFF_NOTES.md Session 7 for the finding that triggered
> this phase. Checkbox legend: `[ ]` pending, `[~]` in-progress, `[x]` done.
>
> **Epic 1 is a research spike — do it first and do not write firmware against
> OAuth internals until it lands.** Epics 2-4 depend on its findings; Epic 3's
> exact shape is set by the Epic 1 ADR.

---

## Epic 1: OAuth research spike + decision

> Front-loaded risk epic, mirroring Phase 2's "TLS first" sequencing. Output is
> knowledge + two ADRs, not firmware.

- [ ] **1.1 Establish Claude Code's OAuth parameters**
  - [ ] Find the client_id, authorize endpoint, token endpoint, scopes, and PKCE method (sources: `claude` CLI login behaviour, Anthropic OAuth docs, the credential blob, network observation)
  - [ ] Probe the token endpoint host with `openssl s_client` — note whether it needs a root CA beyond the poller's GTS Root R4
  - [ ] Record everything in PHASE_IMP.md (these become `config.h` constants)

- [ ] **1.2 Prove the refresh grant off-device**
  - [ ] Script a `refresh_token` grant exchange (curl/python) against the token endpoint; confirm it returns a fresh access token
  - [ ] Determine whether the refresh token rotates (is a new one returned each time?)
  - [ ] Capture the exact request + response shape

- [ ] **1.3 ADR — choose the onboarding mechanism**
  - [ ] Compare: authorize-URL + paste-back one-time code (expected) vs one-shot pairing helper vs anything the spike surfaces
  - [ ] Write a new ADR selecting one; it supersedes ADR 003 (WiFiManager-only onboarding)
  - [ ] If no viable standalone flow exists, STOP and escalate to the user — the standalone premise is at stake

- [ ] **1.4 ADR — refresh-token storage**
  - [ ] Threat-model a refresh token in plaintext NVS vs a scoped API key
  - [ ] Decide NVS encryption in-scope (Epic 4) or explicitly deferred; revise ADR 005 accordingly

---

## Epic 2: Credential store + token refresh (firmware)

> Method-independent — needed whatever Epic 1 picks for onboarding.

- [ ] **2.1 Credential model in NVS** (`secrets_store.ino` / `config.h`)
  - [ ] Store access token + refresh token + `expiresAt` (supersede the single `anthropic_key` string)
  - [ ] Migration: a device holding only an old single token degrades to "re-onboard required"
  - [ ] `secret_redactor()` covers both token types; neither ever logged

- [ ] **2.2 Refresh client** (`oauth.ino` — new tab)
  - [ ] `oauth_refresh()` — stack-scoped `WiFiClientSecure` + `HTTPClient` POST of the refresh grant to the token endpoint (heap discipline as in `poller.ino`)
  - [ ] Bundle the token-endpoint root CA in `certs.h` if different from the poller's
  - [ ] Parse the new access token + expiry (+ rotated refresh token); persist atomically via Epic 2.1

- [ ] **2.3 Refresh scheduling** (`m5clawd.ino`)
  - [ ] Refresh proactively when the access token is within a margin of expiry; also refresh-and-retry once on a poll `401`
  - [ ] Refresh failure -> `AUTH_FAILED` / a new "re-onboard required" status; bounded backoff, no refresh storm
  - [ ] Wire into the existing poll loop / state machine without blocking buttons

- [ ] **2.4 Pure-logic refresh-decision module** (`refresh_policy.{h,cpp}` + tests)
  - [ ] `should_refresh(expiresAt, now, margin)` and the failure/backoff decision — Arduino-header-free
  - [ ] Host unit tests in `m5clawd/test/`

---

## Epic 3: Onboarding flow

> Built to the Epic 1.3 ADR. The tasks below assume the expected outcome —
> authorize-URL + paste-back one-time code. Revise if the ADR picks otherwise.

- [ ] **3.1 "Log in with Claude" portal step** (`wifi_portal.ino` / `ui.ino`)
  - [ ] After WiFi is entered, the portal shows the authorize URL (+ a QR) for the user to open on their phone/laptop
  - [ ] Generate + hold the PKCE verifier/challenge for the session

- [ ] **3.2 Code exchange** (`oauth.ino`)
  - [ ] Portal field for the one-time code the user pastes back
  - [ ] Device exchanges code + PKCE verifier for access + refresh tokens at the token endpoint; persists them
  - [ ] Reject a bad code with a clear on-screen hint (reuse the portal-hint pattern)

- [ ] **3.3 Onboarding sequencing** (`m5clawd.ino` / `wifi_portal.ino`)
  - [ ] Stage the flow: WiFi credentials first, then the OAuth step (the OAuth exchange needs internet)
  - [ ] Fold in the carry-over fix: harden `station_connect()` so the post-onboarding boot does not need a manual power-cycle (Phase 1/2 known issue)

- [ ] **3.4 "Change credential" path**
  - [ ] A way to re-run just the OAuth step (re-onboard the token) without wiping WiFi creds — e.g. a dedicated button gesture or portal menu entry

---

## Epic 4: Resilience & secrets

- [ ] **4.1 OAuth-specific UI states** (`ui.ino`)
  - [ ] Distinct "re-onboard required" screen/badge (refresh token revoked / invalid) with instructions
  - [ ] A transient "refreshing…" indication; ensure stale data stays visible meanwhile

- [ ] **4.2 Refresh-token storage hardening** (scope per ADR 1.4)
  - [ ] If in-scope: NVS encryption for the credential blob; else document the deferral and the device-loss revocation guidance

- [ ] **4.3 Re-onboard robustness**
  - [ ] Verify the device recovers cleanly from: expired access token, revoked refresh token, and a mid-refresh power loss

---

## Epic 5: Testing & documentation

- [ ] **5.1 Host unit tests** — `m5clawd/test/run.sh` green incl. the new refresh-policy suite
- [ ] **5.2 Hardware verification**
  - [ ] Onboard end-to-end via the new flow (no raw-token paste)
  - [ ] Observe a real token refresh across an access-token expiry boundary (force a short margin if needed)
  - [ ] Multi-day unattended run — poll-success ≥ 99%, no manual steps
- [ ] **5.3 Documentation** — new ADRs (onboarding supersedes 003; refresh-storage revises 005); update `1_PROJECT_OVERVIEW.md` (roadmap renumber, onboarding), `2_ARCHITECTURE.md` (onboarding + a token-refresh component), `README.md` (the real onboarding walkthrough), `CLAUDE.md`, HANDOFF_NOTES

---

## Notes

**Blockers:**
- Epic 1 gates everything. If the spike finds no viable standalone OAuth flow,
  the phase (and the standalone-device premise) needs a rethink with the user.

**Dependencies (intra-phase):**
- Epic 1 (spike) → everything.
- Epic 2 (refresh) is method-independent and can start once Epic 1.1/1.2 land.
- Epic 3 (onboarding) needs the Epic 1.3 ADR.
- Epics 4 and 5 depend on 2 + 3.

**Carry-over from Phase 2:**
- `station_connect()` hardening (post-onboarding power-cycle bug) — folded into Task 3.3.
- Confirm the user rotated the OAuth tokens leaked into the Session-7 transcript.

**Task count:** 18 numbered tasks across 5 epics (4/4/4/3/3).
