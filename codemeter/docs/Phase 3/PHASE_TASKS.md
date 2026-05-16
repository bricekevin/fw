# Phase 3 - OAuth Onboarding + Token Refresh Tasks

**Status:** 4/18 — Epic 1 spike complete; Epic 2 next
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

- [x] **1.1 Establish Claude Code's OAuth parameters**
  - [x] Find the client_id, authorize endpoint, token endpoint, scopes, and PKCE method (sources: `claude` CLI login behaviour, Anthropic OAuth docs, the credential blob, network observation)
  - [x] Probe the token endpoint host with `openssl s_client` — note whether it needs a root CA beyond the poller's GTS Root R4
  - [x] Record everything in PHASE_IMP.md (these become `config.h` constants)

- [x] **1.2 Prove the refresh grant off-device** — _resolved by decision: deferred to on-device_
  - [x] ~~Script a `refresh_token` grant exchange off-device~~ — **deferred** (2026-05-15): the only live refresh token is the Mac's `claude` Keychain credential; a rotating grant would log it out. The device's own first refresh in Task 5.2 is the proof instead. Grant shape recorded in PHASE_IMP.md from the binary config.
  - [x] Refresh-token rotation: **assume yes, handle defensively** in Epic 2.2; empirical answer back-filled by Task 5.2
  - [x] Request + response shape captured in PHASE_IMP.md (Task 1.1 findings block)

- [x] **1.3 ADR — choose the onboarding mechanism**
  - [x] Compared: authorize-URL + paste-back code vs loopback redirect vs pairing helper vs manual token paste
  - [x] Wrote ADR 007 — authorize-URL + PKCE + paste-back one-time code; supersedes ADR 003's API-key step
  - [x] A viable standalone flow exists (Claude Code's headless login path) — no escalation needed

- [x] **1.4 ADR — refresh-token storage**
  - [x] Threat-modelled a refresh token in plaintext NVS (it cannot be scoped down like an API key — revocation is the real control)
  - [x] Wrote ADR 008 — plaintext NVS, encryption explicitly DEFERRED to Phase 4; Epic 4.2 becomes a docs task; revises ADR 005

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

- [ ] **4.2 Refresh-token storage — documentation** (scoped by ADR 008)
  - [ ] ADR 008 deferred NVS encryption to Phase 4; this task is **docs only**: write the device-loss revocation guidance into the README and the "re-onboard required" UI copy (the mandatory compensating controls from ADR 008)

- [ ] **4.3 Re-onboard robustness**
  - [ ] Verify the device recovers cleanly from: expired access token, revoked refresh token, and a mid-refresh power loss

---

## Epic 5: Testing & documentation

- [ ] **5.1 Host unit tests** — `m5clawd/test/run.sh` green incl. the new refresh-policy suite
- [ ] **5.2 Hardware verification**
  - [ ] Onboard end-to-end via the new flow (no raw-token paste)
  - [ ] Observe a real token refresh across an access-token expiry boundary (force a short margin if needed)
  - [ ] **Carry-over from Task 1.2:** on that first refresh, record whether the response carried a new `refresh_token` (rotation) and whether the old one still worked — back-fill the "refresh rotates?" answer in PHASE_IMP.md
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
