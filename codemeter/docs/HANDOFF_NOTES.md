# Handoff Notes

**Project:** M5Clawd
**Last Updated:** 2026-05-15 (Session 15)

> This document tracks work sessions, changes, and context for continuity between work sessions or AI agent handoffs.

---

## Session 15 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~30 min
**Focus:** `/3_dev` Phase 3 — **Epic 4 Task 4.2**, the ADR 008 compensating-
control documentation (docs/copy only — no logic). 14/18 Phase 3 tasks.

### Completed — Task 4.2

- **README "Security & Privacy Notes" rewritten.** Replaced the stale
  API-key bullets with the OAuth credential reality: access + refresh token in
  plaintext NVS (encryption deferred -> ADR 008), and **revocation as the
  headline recovery story** — lose/give away/lose the device -> revoke its
  access at claude.ai. Added the wipe-before-handoff control (button C) and
  corrected the TLS/redaction bullet. ADR 008 now linked here and in the
  threat-model footer (was ADR 005).
- **`ui_show_reauth_required()` copy** now carries the ADR 008 revocation line
  ("Lost the device? Revoke its access in your Claude account settings.")
  alongside the hold-B re-onboard instruction.
- **`ui_show_wifi_error()` copy fixed** — "Hold button C 5s to **reconfigure**"
  (was "to re-onboard", which collided with the Task 3.4 button-B re-onboard
  gesture; C does a full WiFi+OAuth reset, which is the right recovery for a
  WiFi failure).

### Verification

- `arduino-cli compile --profile m5clawd` **clean** — 88% flash
  (1,158,850 B), 13% RAM.
- Host tests unaffected (docs + UI copy only) — not re-run.
- **Not flashed.**

### Files Changed

```text
README.md           — Security & Privacy Notes rewritten for the OAuth model
m5clawd/ui.ino      — reauth screen revocation line; wifi_error copy fix
docs/Phase 3/...    — PHASE_TASKS 4.2 checked off
```

### Known Issues / Watch

- The README **onboarding walkthrough (Quick Start §1-§5) is still pre-OAuth**
  — it describes the API-key captive-portal form and has a 2026-05 "being
  redesigned" banner. Out of Task 4.2 scope (4.2 = security notes + revocation
  copy only); a full README onboarding rewrite belongs in a `/6_doc` pass once
  Epic 5 confirms the flow on hardware.
- The README day-to-day "Buttons" list does not yet mention the button-B
  re-onboard gesture — fold into that same `/6_doc` pass.

### Next Session Should

1. **Task 4.3** — re-onboard robustness: verify clean recovery from expired
   access token, revoked refresh token, and mid-refresh power loss.
2. **Epic 5** — flash + the hardware-verification backlog (Sessions 11-15).

---

## Session 14 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~40 min
**Focus:** `/3_dev` Phase 3 — **Epic 4 Task 4.1**, OAuth-specific UI states
(13/18 Phase 3 tasks).

### Completed — Task 4.1

- **`ui_show_reauth_required()`** — a dedicated full screen ("ACTION NEEDED" /
  "Claude login expired" / "Hold button B 5s to log in again"). The display
  now *locks* to it whenever `g_reauthRequired` is set: `show_current_screen()`
  short-circuits to it, `do_poll()`'s tail draws it when a refresh kills the
  credential mid-poll, and the periodic `loop()` refresh is gated off so it is
  never overpainted. Previously a dead credential showed only a small "re-auth"
  status-bar badge.
- **`ui_show_refreshing()`** — a transient indicator drawn at the start of
  `do_refresh()` before the blocking `oauth_refresh()` TLS call. It repaints
  *only* the status-bar badge slot (left 130 px) in `COLOR_WARNING`, so the
  stale Usage cards underneath stay visible; the next `ui_update_usage()`
  restores the real badge. Guarded to the Usage screen (the only view with a
  status bar).

### Verification

- `arduino-cli compile --profile m5clawd` **clean** — 88% flash
  (1,158,726 B), 13% RAM.
- Host tests: **all 5 suites pass** (181 checks) — UI-only change.
- **Not flashed.** Epic 5 hardware checks: confirm the reauth screen appears
  on a real refresh-token death, and that "refreshing" is visible during the
  ~2-4 s refresh window without flickering the cards.

### Files Changed

```text
m5clawd/ui.ino      — new ui_show_reauth_required() + ui_show_refreshing()
m5clawd/m5clawd.ino — show_current_screen()/do_poll()/loop() lock to the reauth
                      screen on g_reauthRequired; do_refresh() draws "refreshing"
m5clawd/config.h    — prototypes for the two new screens
```

### Known Issues / Watch

- Boot-time `g_reauthRequired` (no OAuth credential **and** WiFi down) still
  shows `ui_show_wifi_error()`, not the new reauth screen — intentional (WiFi
  must be fixed first), but the wifi_error copy still says "Hold C" while the
  real re-onboard gesture is now B. Reconcile copy in Task 4.2.
- The reauth screen copy is functional but Task 4.2 (docs) must fold in the
  ADR 008 device-loss / token-revocation guidance.

### Next Session Should

1. **Task 4.2** — docs only: device-loss revocation guidance into the README +
   reconcile the re-onboard UI copy (incl. the wifi_error "Hold C" wording).
2. **Task 4.3** — re-onboard robustness verification.
3. **Epic 5** — flash + hardware verification backlog (Sessions 11-14).

---

## Session 13 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~40 min
**Focus:** `/3_dev` Phase 3 — **Epic 3 Task 3.4**, the "change credential"
gesture. Epic 3 is now complete (12/18 Phase 3 tasks).

### Completed — Task 3.4

- **Button B long-press = re-onboard OAuth only.** Mirrors the existing C
  reset gesture: hold B 5 s -> orange "RE-ONBOARD" confirm screen
  (`ui_show_reonboard_confirm()`); hold to 7 s -> commit. A short B press
  still forces an immediate poll (moved from `wasPressed()` to the
  `wasReleased()` non-confirm branch so the long-press doesn't also poll).
- **`secrets_clear_oauth()`** (`secrets_store.ino`) — `preferences.remove()`s
  only the OAuth keys (`oauth_at` / `oauth_rt` / `oauth_exp`) and the legacy
  `anthropic_key`, leaving the WiFi creds and `NVS_KEY_CONFIGURED` intact.
  After it the device reboots: Stage 1 is skipped (WiFi still configured),
  `secrets_cred_state()` returns `CRED_NONE`, and the existing boot logic
  re-enters `wifi_portal_oauth_stage()` — no new onboarding code needed.
- New `reonboardConfirmShown` flag guards `do_poll()` and the two `loop()`
  paths so a poll/redraw never paints over the confirm screen (same treatment
  `resetConfirmShown` already gets).

### Verification

- `arduino-cli compile --profile m5clawd` **clean** — 88% flash
  (1,158,278 B), 13% RAM.
- Host tests: **all 5 suites pass** (181 checks) — no pure module touched.
- **Not flashed.** Hardware check for Epic 5: confirm B-hold timing feels
  right and doesn't collide with the C reset; confirm the post-clear reboot
  lands in Stage 2 with WiFi still connected.

### Files Changed

```text
m5clawd/secrets_store.ino — new secrets_clear_oauth() (OAuth-only NVS wipe)
m5clawd/ui.ino            — new ui_show_reonboard_confirm() (orange confirm screen)
m5clawd/m5clawd.ino       — B long-press gesture; reonboardConfirmShown guards
m5clawd/config.h          — prototypes for the two new functions
```

### Known Issues / Watch

- Two "hold" gestures now exist (B = re-onboard, C = full reset). The
  `ui_show_wifi_error()` copy still says "Hold button C 5s to re-onboard" —
  technically C wipes everything; left as-is (a WiFi-error device genuinely
  needs the full reset). Revisit copy in Epic 4.1.
- The B gesture is not advertised on-screen anywhere — discoverability is an
  Epic 4.1 (OAuth UI states) concern.

### Next Session Should

1. **Epic 4** — Task 4.1 (OAuth UI states: "re-onboard required" screen +
   "refreshing…" indicator), 4.2 (re-onboard docs), 4.3 (re-onboard
   robustness).
2. **Epic 5** — flash the device and run the hardware verification backlog
   from Sessions 11–13.

---

## Session 12 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~30 min
**Focus:** `/3_dev` Phase 3 — onboarding QR-flow refinement: fix the
3rd-screen QR (resolves a Session 11 known issue).

### Completed

- **QR-flow review.** Confirmed the 3-QR onboarding scheme: (1) `ui_show_provisioning`
  WiFi-join QR for the soft-AP, (2) `ui_portal_client_connected` QR to the
  `192.168.4.1` captive portal, (3) `ui_show_oauth_login` "Log in with Claude"
  screen. Screens 1 and 2 were already correct; screen 3 needed the fix below.
- **QR #3 now encodes the LAN portal URL, not the authorize URL.** Previously
  `ui_show_oauth_login` rendered the ~345-char Claude authorize URL as a dense
  version-14 QR (Session 11 flagged it as "may not scan"). It now encodes the
  short device portal URL (`http://192.168.x.x`, ~20 chars, version 4). The
  phone scans it, lands on the device-hosted Stage 2 page, and does the
  "Log in with Claude" + paste-code there — the web page already carries the
  authorize link (`oauthLoginHtml`) and the `oauthCodeField`.
- `ui_show_oauth_login()` signature dropped its now-unused `authorize_url`
  argument (LCD side no longer needs it; the portal page still builds the link
  from `authUrl`).

### Verification

- `arduino-cli compile --profile m5clawd` **clean** — 88% flash
  (1,157,702 B), 13% RAM.
- Host tests: **all 5 suites pass** (181 checks) — UI-only change, no pure
  module touched.
- **Not flashed** — on-device QR scannability still to be confirmed in Epic 5 /
  `/5_visual`, but a version-4 QR is well within reliable scan range.

### Files Changed

```text
m5clawd/ui.ino          — ui_show_oauth_login(): QR = portal URL (was authorize URL)
m5clawd/wifi_portal.ino — call site updated to the 1-arg signature
m5clawd/config.h        — prototype updated to drop authorize_url
```

### Known Issues / Watch

- The Session 11 "dense authorize-URL QR" risk is **resolved** — QR #3 is now
  low-density. Remaining Session 11 hardware-verification items (web-portal
  mode, AP->STA transition, `code=true` marker) still stand for Epic 5.

### Next Session Should

1. **Task 3.4** — the "change credential" gesture (re-run Stage 2 only).
2. Epic 5 hardware verification — flash the device, photograph the 3 QR
   screens, confirm all three scan.

---

## Session 11 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~2 h
**Focus:** `/3_dev` Phase 3 — **Epic 3 onboarding**, Tasks 3.1-3.3: the OAuth
PKCE machinery, the code exchange, and the full two-stage onboarding flow.

### Completed — Epic 3 Tasks 3.1-3.3 (11/18 Phase 3 tasks)

- **Task 3.1 — PKCE + authorize URL.** New pure module `oauth_pkce.{h,cpp}`:
  base64url, percent-encoding, the authorize-URL builder — host-tested
  (`test/oauth_pkce_test.cpp`, 58 checks). `oauth.ino` gained
  `oauth_pkce_begin()` — mints a per-session `code_verifier`/`state` via the
  hardware RNG (`esp_fill_random`) and derives the S256 `code_challenge` with
  mbedtls.
- **Task 3.2 — code exchange.** `oauth_exchange_code()` POSTs the
  `authorization_code` grant and persists the returned access + refresh tokens.
  Accepts `code` or `code#state`, state-checks, maps 4xx -> `EXCHANGE_BAD_CODE`
  / 5xx+transport -> `EXCHANGE_NET_ERROR`.
- **Task 3.3 — two-stage onboarding flow.** Onboarding is now two staged
  WiFiManager sessions with a reboot between (`wifi_portal_wifi_stage()` /
  `wifi_portal_oauth_stage()`): Stage 1 collects WiFi over the soft-AP captive
  portal; Stage 2 serves the OAuth web portal on the **home-LAN IP**
  (`startWebPortal()`), so the phone keeps internet for claude.com while still
  reaching the device to paste the code. Three LCD onboarding screens, each
  with its own QR (join AP / open portal / log in). The ADR 003 API-key field
  is removed. `station_connect()` hardened (settle delay + 3 attempts — fixes
  the post-onboarding power-cycle bug).

### Decision — ADR 009 (new)

ADR 007 step 2 assumed concurrent AP+STA lets the phone stay on one network.
It does not: an ESP32 soft-AP does not route the STA uplink to AP clients, so a
phone on `M5Clawd-XXXXXX` has no internet. **ADR 009** records the resolution
(decided with Kevin): Stage 2 moves to a web portal on the home LAN; the
soft-AP drops once WiFi is up. Refines ADR 007 steps 2-3.

### Verification

- Host tests: **all 5 suites pass** (181 checks; new `oauth_pkce` = 58).
- Firmware: `arduino-cli compile --profile m5clawd` **clean** — 88% flash
  (1,157,726 / 1,310,720 B), 13% RAM. **Not flashed** — Phase 3 hardware
  verification is Epic 5.

### Files Changed

```text
m5clawd/oauth_pkce.h, oauth_pkce.cpp     — new pure module (PKCE + authorize URL)
m5clawd/test/oauth_pkce_test.cpp, run.sh — new test suite
m5clawd/oauth.ino                        — oauth_pkce_begin() + oauth_exchange_code()
m5clawd/wifi_portal.ino                  — REWRITTEN: two staged portals
m5clawd/secrets_store.ino                — onWifiSaved() + oauthCodeSaveCallback()
                                           (replaces the API-key saveParamCallback)
m5clawd/m5clawd.ino                      — staged boot logic; hardened station_connect()
m5clawd/ui.ino                           — 3 onboarding screens (step QR each)
m5clawd/config.h                         — endpoints, ExchangeResult, prototypes
docs/decisions/009-*.md, 000-index.md    — ADR 009
```

### Known Issues / Watch — HARDWARE VERIFICATION NEEDED

The whole Task 3.3 flow **compiles clean but is hardware-untested**. Epic 5.2
must verify on the device:

- **WiFiManager web-portal mode.** `startWebPortal()` + `process()` serving the
  `/param` page on the STA IP, and `setSaveParamsCallback` firing from it — the
  riskiest unproven assumption. If the param page does not appear/save in web
  mode, Stage 2 needs rework (a small custom `WebServer` is the fallback).
- **AP -> STA transition.** Stage 1 reboots into Stage 2; confirm the hardened
  `station_connect()` actually clears the post-onboarding power-cycle bug.
- **The on-LCD authorize-URL QR is dense** (~345-char URL -> version 14, ~2px/
  module). Scannability unverified — confirm in `/5_visual` / Epic 5; if it
  will not scan, the phone-side portal link is the working path.
- **`code=true` in the authorize URL** — included as the headless-flow marker
  but not explicitly confirmed by the Epic 1 spike; verify the claude.ai
  success page actually shows a code on first real onboarding.
- **Flash at 88%** — watch the 1.31 MB ceiling; a partition-scheme change may
  be needed before Epic 4 polish art lands.

### Next Session Should

1. **Task 3.4** — the "change credential" gesture: re-run only Stage 2 (new
   OAuth credential) without wiping WiFi. A device with no OAuth credential
   already auto-enters Stage 2 on boot; 3.4 adds an on-demand trigger
   (button gesture / portal entry).
2. Epic 4 (OAuth UI states, re-onboard robustness) and Epic 5 (the hardware
   verification listed above — flash the device and run the new flow).
3. Carry-over still open: rotate the Session-7-leaked OAuth tokens.

### Notes

- `oauth_exchange_code()` / `oauth_pkce_begin()` were committed as a capability
  first (Tasks 3.1/3.2), then wired into the flow (3.3) — the same split the
  project used for the refresh client (Tasks 2.2/2.3).

---

## Session 10 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/3_dev` Phase 3 — **Epic 2, the credential store + token-refresh firmware** (method-independent; built ahead of the Epic 3 onboarding flow).

### Completed — Epic 2 done (8/18 Phase 3 tasks)

- **Task 2.4 — `refresh_policy.{h,cpp}`** — pure, Arduino-header-free:
  `should_refresh(expiresAt, now, margin)` and `refresh_retry_delay_s()`. Host
  unit tests added (`test/refresh_policy_test.cpp`, 23 checks, wired into
  `run.sh`).
- **Task 2.1 — OAuth credential model** — `secrets_store.ino` + `config.h`: the
  single `anthropic_key` string is superseded by `oauth_at` / `oauth_rt` /
  `oauth_exp` in NVS. A `CredState` enum (NONE / LEGACY / OAUTH) drives the
  re-onboard decision — a device holding only the legacy token is `CRED_LEGACY`.
  New `UsageData::REAUTH_REQUIRED` status + status-bar badge.
- **Task 2.2 — `oauth.ino` refresh client** — `oauth_refresh()` POSTs the
  refresh grant to `platform.claude.com` over TLS, parses the new credential,
  handles refresh-token rotation defensively, persists it. **ISRG Root X1**
  bundled in `certs.h` (probed + verified — the token host's root, distinct
  from the poller's GTS Root R4).
- **Task 2.3 — refresh scheduling** — `m5clawd.ino`: proactive refresh when the
  access token nears expiry, refresh-and-retry once on a poll 401, transient
  failures back off, a definitive failure -> `g_reauthRequired` / re-onboard
  state with polling halted.

### Verification

- Host tests: **all 4 suites pass** (123 checks — parse_headers 50, format_helpers
  22, state_machine 28, refresh_policy 23).
- Firmware: `arduino-cli compile --profile m5clawd` **clean** — 87% flash
  (1,147,566 / 1,310,720 B), 13% RAM. No device flash this session (Phase 3 is
  not flashed until Epic 5; the dev device stays on the Phase 2 build).

### Files Changed

```text
m5clawd/refresh_policy.h, refresh_policy.cpp   — new pure module
m5clawd/test/refresh_policy_test.cpp, run.sh   — new test suite
m5clawd/oauth.ino                              — new refresh-client tab
m5clawd/certs.h                                — + ISRG Root X1
m5clawd/config.h                               — OAuth consts, NVS keys, CredState,
                                                 RefreshResult, prototypes
m5clawd/secrets_store.ino                      — OAuth credential accessors
m5clawd/usage_data.h                           — + REAUTH_REQUIRED status
m5clawd/m5clawd.ino                            — refresh scheduling in the poll loop
m5clawd/ui.ino                                 — re-auth status-bar badge
```

### Known Issues / Watch

- **Flash at 87%.** Epic 3 adds onboarding + QR code; keep an eye on the
  1.31 MB ceiling — a partition-scheme change may be needed if it gets tight.
- The `claude` CLI binary build that yielded the OAuth params is 2.1.143; the
  client_id is pinned in `config.h` and may change if Anthropic rotates it.

### Next Session Should

1. Start **Epic 3** (onboarding flow) — built to ADR 007: WiFi first, then the
   "Log in with Claude" authorize-URL + paste-back code step over concurrent
   AP+STA. Tasks 3.1-3.4. This replaces the Phase-2 API-key portal field.
2. Carry-over still open: rotate the Session-7-leaked OAuth tokens.

### Notes

- Epic 2 was built before Epic 3 deliberately (it is onboarding-method-
  independent). After Epic 2 a real device with only the legacy `anthropic_key`
  boots to "re-onboard required" — expected; Epic 3 supplies the new onboarding.
- `refresh_token` rotation is still unproven (Task 1.2 deferred) — the firmware
  handles it defensively; Epic 5.2 records the real behaviour.

---

## Session 9 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~45 min
**Focus:** `/3_dev` Phase 3 — **Epic 1, the OAuth research spike**. Knowledge + two ADRs, no firmware (as planned).

### Completed — Epic 1 done (4/18 Phase 3 tasks)

- **Task 1.1 — OAuth parameters established.** Extracted Claude Code's production
  OAuth config from the `claude` CLI binary (`strings`/`grep -a` on the Mach-O at
  `~/.local/share/claude/versions/2.1.143`), the public client-metadata document,
  and the local Keychain credential *structure* (field names/lengths only, no
  secret values). Recorded in `docs/Phase 3/PHASE_IMP.md`:
  - `client_id` `9d1c250a-e61b-44d9-88ed-5944d1962f5e` (public client, PKCE S256,
    no secret). The binary's second `22422756-…` block is a `-local-oauth` dev
    config — ignored.
  - authorize `https://claude.com/cai/oauth/authorize`; token
    `https://platform.claude.com/v1/oauth/token`; manual redirect
    `…/oauth/code/callback`.
  - **Token host `platform.claude.com` chains ISRG Root X1 (Let's Encrypt)** — a
    *different* root from the poller's GTS Root R4. Epic 2.2 must bundle it.
- **Task 1.2 — refresh grant: proof deferred to on-device** (Kevin's call). The
  off-device exchange would use the Mac's live `claude` Keychain refresh token; a
  rotating grant would log it out. The device's own first refresh (Task 5.2) is
  the proof. Firmware will handle rotation defensively regardless.
- **Task 1.3 — ADR 007** (OAuth onboarding): authorize-URL + PKCE + paste-back
  one-time code, two-stage (WiFi, then OAuth) over concurrent AP+STA. Supersedes
  ADR 003's API-key field. Loopback redirect and the pairing-helper were
  considered and rejected. A viable standalone flow exists — no escalation.
- **Task 1.4 — ADR 008** (refresh-token storage): plaintext NVS; NVS encryption
  explicitly **deferred to Phase 4**. A refresh token can't be scoped down like
  an API key, so revocation (not encryption) is the real control — README + the
  re-onboard UI must make it obvious. Revises ADR 005; Epic 4.2 is now docs-only.

### Files Changed

```text
docs/Phase 3/PHASE_IMP.md   — Task 1.1 findings block; Task 1.2 deferral note
docs/Phase 3/PHASE_TASKS.md — Epic 1 checked off (4/18); Epic 4.2 + 5.2 retargeted
docs/decisions/007-*.md     — new: OAuth onboarding ADR
docs/decisions/008-*.md     — new: refresh-token storage ADR
docs/decisions/000-index.md — rows 007/008; status notes on 003/005
docs/decisions/003-*.md, 005-*.md — superseded/revised banners
```

### Next Session Should

1. Start **Epic 2** (credential store + token refresh) — method-independent, can
   begin now. Tasks 2.1 (NVS model: `oauth_at`/`oauth_rt`/`oauth_exp`, retire
   `anthropic_key`), 2.2 (`oauth.ino` refresh client — bundle ISRG Root X1),
   2.3 (refresh scheduling), 2.4 (pure `refresh_policy` module + host tests).
2. Then **Epic 3** is now unblocked — built to ADR 007.
3. Carry-over still open: rotate the Session-7-leaked OAuth tokens; confirm with
   Kevin.

### Notes

- Spike worked on branch `phase-2-20260515` (the existing worktree that holds all
  of Phase 2 + the Phase 3 plan) — Phase 3 continues there rather than a new
  branch, to keep the plan and its implementation together. Still unpushed.
- Epic 1 produced no firmware, by design (research-gated phase).

---

## Session 8 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~30 min
**Focus:** Post-pivot wrap-up — `/6_doc` (reconcile docs with the OAuth pivot) then `/2_pm` (plan the OAuth phase).

### Completed

- **`/6_doc`** — corrected the docs the OAuth pivot made wrong. `2_ARCHITECTURE.md` (API Design + Security now describe OAuth Bearer auth; poll-flow header step), `PHASE_PRD.md` Phase 2 (revision banner), `CLAUDE.md` (auth-model note for agents; also fixed the now-false "cannot flash a device" claim), `README.md` + `1_PROJECT_OVERVIEW.md` (onboarding-redesign callouts). ADRs 003/005 deliberately left — the Phase 3 ADRs supersede them. Commit `a5bd55c`.
- **`/2_pm`** — planned **Phase 3: Claude Code OAuth onboarding + token refresh** (`docs/Phase 3/` — PRD, TASKS, IMP, DEPENDENCIES). 5 epics / 18 tasks: a front-loaded OAuth research spike + ADRs, the NVS credential store + refresh client, the onboarding flow, resilience/secrets, testing. Commit `3831e4b`.

### Decisions made

- **Phase renumber:** OAuth onboarding is MVP-blocking (device dies in ~a day without refresh); the old "Phase 3 Polish" is not. So OAuth takes the Phase 3 slot and Polish becomes Phase 4. Recorded in the Phase 3 docs; `1_PROJECT_OVERVIEW.md`'s roadmap section still needs that renumber applied (left for Phase 3's Task 5.3).
- **Phase 3 is research-gated:** Epic 1 is a spike (OAuth client_id / endpoints / refresh-grant, proven off-device) before any firmware — mirroring how Phase 2 front-loaded the TLS risk. If no viable standalone OAuth flow exists, the standalone-device premise itself needs a rethink.
- **Doc strategy:** corrected load-bearing technical docs now; did not rewrite the onboarding walkthroughs (README/overview) since Phase 3 redefines onboarding — flagged them instead, to avoid double-rewrite churn.

### Files Changed

```text
docs/2_ARCHITECTURE.md, CLAUDE.md, README.md, docs/1_PROJECT_OVERVIEW.md,
docs/Phase 2/PHASE_PRD.md   — OAuth-pivot doc reconciliation
docs/Phase 3/*              — new: Phase 3 plan (PRD/TASKS/IMP/DEPENDENCIES)
```

### Next Session Should

1. Start Phase 3 with `/3_dev` — **Epic 1, the OAuth spike** (establish Claude Code's OAuth parameters; prove the refresh grant off-device; write the two ADRs). Do not write firmware against OAuth internals until the spike lands.
2. Branch `phase-2-20260515` holds all of Phase 2 + the Phase 3 plan (~16 commits), still unpushed — push / open a PR when ready.
3. Carry-overs: the leaked OAuth tokens should be rotated; `station_connect()` hardening is folded into Phase 3 Task 3.3.

---

## Session 7 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1.5 h
**Focus:** First on-hardware test of Phase 2 — and a significant finding that pivots the auth model.

### What happened

Flashed the Phase 2 firmware to the physical M5Stack (worktree `../theClaw-phase-2-20260515`, branch `phase-2-20260515`) and tested against the live API. The poll path works — but the original `x-api-key` design does not surface the usage headers. Switched to OAuth Bearer auth and verified the full path on hardware.

### The headline finding — auth model pivot

- A plain **`x-api-key`** request to `api.anthropic.com/v1/messages` returns `200` but carries **none** of the `anthropic-ratelimit-unified-*` headers. Those headers are a **Claude Code / OAuth** feature.
- Confirmed against the Clawdmeter daemon source: it authenticates with `Authorization: Bearer <oauth-token>` + `anthropic-beta: oauth-2025-04-20` + `User-Agent: claude-code/...`. The "unified" 5h/7d limits are Claude Code subscription limits, not API-key billing limits.
- **Fixed:** the poller now uses OAuth Bearer auth (commit `6083f39`). Re-verified on hardware — a live poll returned `unified-5h-utilization = "0.82"`, `unified-7d-utilization = "0.05"`; the parser produced session 82% / weekly 5%; `outcome=0` (POLL_OK). NVS last-known-good restore also confirmed working across a real reboot (`[boot] restored usage: session=82% weekly=5%`).

### What this validated on hardware

- TLS to `api.anthropic.com` against the pinned GTS Root R4 — **works** (the Phase 2 headline risk, cleared on real hardware).
- Poll loop, header parsing (fraction format `0.82` → 82%), state machine, NVS save/restore — all confirmed.
- Single poll latency ≈ 4.4-4.5 s (close to the 5 s p95 budget — watch it).

### New required work (NOT in Phase 2 scope — needs its own phase)

The stored credential is now a **Claude Code OAuth access token** (`sk-ant-oat01-...`), which **expires (~22 h observed)**. Two new problems, one root:

1. **Token refresh.** A static access token dies in ~a day; "unattended for 7+ days" is impossible without the device holding a **refresh token** and refreshing itself. Refresh needs Claude Code's OAuth client internals (token endpoint, client_id) — to be researched.
2. **Onboarding UX.** A user cannot be expected to paste a 108-char OAuth token. The product answer is the device doing an OAuth flow itself. Options discussed:
   - **Best:** "Log in with Claude" in onboarding — captive portal does OAuth authorization-code + PKCE; user authenticates on claude.ai; device receives access + refresh tokens and self-refreshes. (Redirect must happen after the device is on the home LAN, since the OAuth flow needs internet + reaching the device.)
   - **Interim:** a one-shot `pair` helper script that reads the Keychain credential (incl. refresh token) and pushes it to the device — one command, no daemon.
   - **Current (dev-only):** manual token paste — not shippable.

   The input method and the refresh flow are the *same problem*: onboarding must deliver a **refresh token**. This revisits ADRs 002 / 003 / 005 and deserves a `/2_pm` planning pass as its own phase.

### Files Changed

```text
m5clawd/config.h        — ANTHROPIC_OAUTH_BETA, ANTHROPIC_USER_AGENT;
                          model -> claude-haiku-4-5-20251001
m5clawd/poller.ino      — Authorization: Bearer + anthropic-beta header;
                          User-Agent; body content "." -> "hi"
m5clawd/wifi_portal.ino — portal field relabelled "Claude Code OAuth token",
                          maxlen 120 -> 200
docs/Phase 2/PHASE_TASKS.md — hardware-validation note; 5.2 partial, 5.3 updated
```

All committed on `phase-2-20260515` (commit `6083f39`). Working tree clean.

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| **Token leak** | HIGH | Kevin's Claude Code OAuth access + refresh tokens were printed to the terminal by a bad command (heredoc/stdin collision) and are in this session's transcript | Re-authenticate Claude Code to rotate. The refresh token is the sensitive one. |
| OAuth token expiry | HIGH | Device stops polling (`auth failed`) in ~22 h until re-onboarded | The root reason a refresh flow / proper onboarding is the next phase. |
| Post-onboarding power-cycle | LOW | After portal save the device showed "phone connected" then returned to setup; a manual power-cycle fixed it | Phase 1 known issue recurred — harden `station_connect()` (more retries / pre-connect delay). |
| Usage screen not photographed | MED | Layout/fonts/colors still visually unconfirmed | Device is showing 82%/5% live — photograph it. |
| Single poll ~4.5 s | LOW | Near the 5 s p95 target | Watch; revisit if it regresses. |

### Next Session Should

1. Run `/2_pm` to plan a new phase for **OAuth onboarding + token refresh** — this is the gating work for a real product. Decide between captive-portal OAuth (best) and a pairing helper (interim); expect new ADRs.
2. Finish the Phase 2 hardware items that are still valid: smoke scenario 3 (WiFi drop), the bad-token path, photograph the Usage screen, harden `station_connect()`.
3. Branch `phase-2-20260515` is unpushed (11 commits) — push when ready; consider a PR.
4. `/6_doc` — `PHASE_PRD.md` still describes the `x-api-key` model and Amazon/ISRG roots; it needs reconciling with the OAuth reality.

---

## Session 6 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/3_dev` Phase 2 Epics 4 + 5 — resilience/persistence, then the testing/docs wrap-up.

### What happened

Implemented Epic 4 (NVS persistence + hardening) and the code-side of Epic 5 (host tests, CI, docs) in worktree `../theClaw-phase-2-20260515`. **Phase 2 is now code-complete — 18/20 tasks.** The only remaining work is on-hardware: smoke tests (5.2) and the latency/24 h soak (5.3).

### Completed (Phase 2 Epics 4 + 5 — 18/20 tasks total)

- [x] **4.1** `usage_store.ino` — last-known-good `UsageData` saved to NVS, restored on boot.
- [x] **4.2** per-poll heap log + low-heap warning; `WiFi.reconnect()` nudge on `WIFI_DOWN`.
- [x] **5.1** host unit tests green — 100 checks across the three pure modules.
- [x] **5.4** `.github/workflows/ci.yml` — compile + host tests + secret scan.
- [x] **5.5** docs — `CHANGELOG.md` created, `2_ARCHITECTURE.md` header names fixed, this entry.
- [ ] **5.2 / 5.3** hardware smoke + soak — BLOCKED on a physical device.

### Decisions made

- **Last-known-good is a separate `usage_store.ino` tab**, not part of `secrets_store.ino` — the usage snapshot is not a secret. Stored as one `putBytes()` blob; a size-mismatched blob is discarded on load.
- **NVS write only when a percentage changed.** Writing every 60 s would wear the flash; utilization is stable for long stretches, so `do_poll()` saves only when `session_pct`/`weekly_pct` differ from the last save.
- **Restored data renders, it does not show `--`.** `ui.ino`'s new `usg_has_data()` treats restored-stale data as displayable (dimmed); only a truly fresh device with empty NVS shows `--`. On boot the restored blob is forced to `status=UNKNOWN, stale=true` so the badge reads "connecting" while the numbers stay visible.
- **No new ADR.** The Session 4 TLS-root and header-name findings are confirmations/corrections of implementation detail (documented in `certs.h`, `parse_headers.h`), not reversals of an architecture decision — an ADR would be noise.
- **CI uses the build profile.** `arduino-cli compile --profile m5clawd` is self-installing (the profile declares the platform + libs), so the workflow is just checkout → setup-arduino-cli → compile → `test/run.sh` → secret scan.

### Files Changed

```text
m5clawd/usage_store.ino     — new: NVS save/restore of last-known-good UsageData
m5clawd/config.h            — NVS_KEY_LKG_USAGE, POLL_HEAP_FLOOR, usage_store protos
m5clawd/m5clawd.ino         — restore-on-boot; per-poll save (change-gated);
                              heap warning; WiFi.reconnect() nudge
m5clawd/ui.ino              — usg_has_data(): restored stale data renders, not "--"
.github/workflows/ci.yml    — new: compile + host tests + secret scan
CHANGELOG.md                — new: Phase 1 + Phase 2 entries
docs/2_ARCHITECTURE.md      — rate-limit header names corrected (-week- -> -7d-)
docs/Phase 2/PHASE_TASKS.md — Epics 4 + 5 checked off
```

### Verification

- `arduino-cli compile --profile m5clawd` — clean, no warnings. Sketch **86%** of flash.
- `m5clawd/test/run.sh` — 100 checks, all pass.
- **Still unverified on hardware** (the whole reason 5.2/5.3 exist): the TLS handshake, a live poll, the rate-limit header names/values on a real response, the rendered Usage screen, NVS restore across a real reboot, and 24 h heap behaviour. None of this has run on a device.

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| Phase 2 never run on hardware | HIGH | The entire poll + render path is compile-clean and host-probed but unexecuted on a device | Tasks 5.2/5.3. Flash, watch serial for `[poll] code=200` and the first-poll header dump, photograph the Usage screen, run a 24 h soak. |
| `PHASE_PRD.md` references Amazon/ISRG roots + `-week-` headers | LOW | Planning doc vs shipped reality | Left for `/6_doc`. `certs.h`, `parse_headers.h`, `PHASE_TASKS.md`, `2_ARCHITECTURE.md` are all correct/authoritative now. |
| Usage-screen layout offsets unconfirmed | MED | `USG_*` constants in `ui.ino` are hand-tuned guesses | Carried from Session 5 — tune against a photo during 5.2. |
| Sketch at 86% of flash | LOW | ~171 KB headroom | Fine for now; revisit before Phase 3 PNG art. |

### Next Session Should

1. **Get an M5Stack Core on USB and run Tasks 5.2 + 5.3** — this is the first real execution of everything built in Sessions 3-6. `cd m5clawd && ./flash.sh`, then `arduino-cli monitor -p /dev/cu.usbserial-02132522 -c baudrate=115200`. Watch for: TLS handshake success, `[poll] code=200`, the first-poll header dump (confirms the `5h`/`7d` names resolve), the Usage screen rendering, and NVS restore after a power-cycle.
2. If the header dump shows `(empty)` values, the names need revisiting — but the Clawdmeter daemon is a strong source, so this is unlikely.
3. Tune the `USG_*` layout constants in `ui.ino` against a photo of the screen.
4. Once 5.2/5.3 pass, Phase 2 is done — run `/6_doc` (fix `PHASE_PRD.md`), then `/2_pm` for Phase 3 (splash art, battery, audio).

---

## Session 5 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/3_dev` Phase 2 Epic 3 — the Usage screen, status overlays, and the 3-screen button cycle.

### What happened

Implemented Epic 3 (4 tasks) in worktree `../theClaw-phase-2-20260515`. The device now renders the polled usage on a proper Usage screen, with a status-badge bar and flicker-free partial redraw. Phase 2 is 13/20 tasks done — Epics 1-3 complete.

### Completed (Phase 2 Epic 3 — 13/20 tasks total)

- [x] **3.1** `ui_show_usage()` — status bar + SESSION (5h) / WEEKLY (7d) cards.
- [x] **3.2** `ui_update_usage()` — per-field cached partial redraw.
- [x] **3.3** status badges + stale/no-data handling.
- [x] **3.4** `m5clawd.ino` — 3-screen cycle on button A, button B force-poll.

### Decisions made

- **Badge wording.** The design system's "State Indicators" table uses terse glyph-ish labels (`auth!`, `api-`, `wifi-`). The Usage badge instead uses readable phrases (`auth failed`, `api error`, `wifi down`, `rate limited`, `poll ok`, `connecting`). Same intent, clearer on a 320 px panel — a minor, deliberate deviation from `3_DESIGN_SYSTEM.md`.
- **Big numbers in font 7.** Per the design system, the percentage uses the built-in 7-segment font 7; the `%` sign is drawn separately in `FSSB18` (font 7 is digits-only). Compiles fine — actual rendering unverified (no hardware this session).
- **`UNKNOWN` shows `--`, not `0%`.** Before the first successful poll the cards show `--` placeholders so a pre-poll device never looks like genuine 0% usage. Stale (last-known-good) data stays on screen but dimmed; the badge conveys the error.
- **`g_lastPollMs`** (a `millis()` stamp) drives the "updated Xs ago" text — chosen over the epoch-based `UsageData::last_poll_unix` so the relative time works even before NTP syncs.

### Files Changed

```text
m5clawd/ui.ino       — new: ui_show_usage / ui_update_usage + Usage helpers
m5clawd/m5clawd.ino  — Screen enum -> Splash/Usage/Status; button A cycle,
                       button B force-poll; g_lastPollMs; poll-loop screen
                       refresh + sustained-failure auto-jump to Status
m5clawd/config.h     — #include "format_helpers.h"; ui_show_usage/ui_update_usage
                       prototypes
docs/Phase 2/PHASE_TASKS.md — Epic 3 checked off
```

### Verification

- `arduino-cli compile --profile m5clawd` — clean, no warnings. Sketch **86%** of flash.
- `m5clawd/test/run.sh` — 100 checks, all pass (pure modules untouched this session).
- **Not verified:** the rendered screen. No hardware in this session — the Usage layout, font-7 rendering, and badge colors are unconfirmed. The `USG_*` y-offset constants in `ui.ino` are hand-tuned guesses; expect to nudge them against a photo.

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| Usage screen never rendered on hardware | MED | Layout/fonts/colors unconfirmed | Flash + photograph. Tune `USG_CARD*_TOP` / `USG_VAL_*` / `USG_RESET_DY` in `ui.ino` if cards look cramped. Confirm built-in font 7 is loaded in this M5Stack build. |
| `2_ARCHITECTURE.md` + `PHASE_PRD.md` say `-week-` headers | MED | Docs vs code | Carried from Session 4 — fix during `/6_doc`. |
| On-device TLS/poll still unverified | MED | Whole poll path compile-clean but never run | Carried from Session 4 — first real test is smoke Task 5.2. |
| Sketch at 86% of flash | LOW | Headroom ~171 KB | Epic 4 adds little code; fine. Watch before Phase 3 art. |

### Next Session Should

1. Continue `/3_dev` Phase 2 with **Epic 4** — last-known-good `UsageData` persistence to NVS (so a cold boot shows the last numbers) + heap/reconnect hardening.
2. Then **Epic 5** — host tests are green; the big remaining item is the on-hardware smoke run (Task 5.2), which is the first real TLS/poll/render test. Also the optional CI workflow (Task 5.4) and doc fixes (Task 5.5).
3. When hardware is available: flash, watch serial for `[poll] code=200` + the first-poll header dump, and photograph the Usage screen.

---

## Session 4 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1.5 h
**Focus:** `/3_dev` Phase 2 Epic 2 — the Anthropic HTTPS poller, TLS, and the poll loop.

### What happened

Implemented Epic 2 (4 tasks) in worktree `../theClaw-phase-2-20260515`. The device now polls `api.anthropic.com` over validated TLS, parses the rate-limit headers into `UsageData`, and runs a backoff-driven poll loop. Two live probes during the work overturned assumptions baked into the foundation docs — see Decisions.

### Completed (Phase 2 Epic 2 — 9/16 tasks total)

- [x] **2.1** `certs.h` — TLS root CA. Probed the live chain; bundled the self-signed GTS Root R4.
- [x] **2.2** `poller.ino` — `WiFiClientSecure` + `HTTPClient` POST, stack-scoped for heap safety.
- [x] **2.3** rate-limit header extraction + ArduinoJson 6 error-body classification.
- [x] **2.4** `m5clawd.ino` — `do_poll()` loop, exponential backoff, NTP, WiFi auto-reconnect.

### Decisions made (IMPORTANT — these overturn earlier doc assumptions)

- **TLS chain is Google Trust Services, not Amazon/ISRG.** A live `openssl s_client` probe showed `api.anthropic.com` chains `leaf -> WE1 -> GTS Root R4`. The PRD/risk table assumed Amazon Root CA 1 / ISRG Root X1 — wrong. `certs.h` pins the **self-signed GTS Root R4** (valid to 2036). The server actually sends a *cross-signed* R4 (expires 2028); the self-signed root — same key — validates the identical chain and lasts longer. Verified: `openssl s_client ... -CAfile gtsr4.pem` returns `Verify return code: 0 (ok)`.
- **Rate-limit header names use `7d`, not `week`.** Confirmed against the Clawdmeter daemon source (`Clawdmeter/daemon/claude_usage_daemon.py`): `anthropic-ratelimit-unified-7d-utilization` / `-7d-reset`. **`docs/2_ARCHITECTURE.md` and `docs/Phase 2/PHASE_PRD.md` both say `-week-` — they are now factually wrong** and should be corrected (see Known Issues).
- **Header value formats confirmed -> NTP is mandatory.** The daemon shows utilization is a `0.0..1.0` fraction (`pct = round(value*100)`) and reset is an absolute Unix timestamp (`(reset - now)`). `parse_headers.{h,cpp}` + its tests were revised from the Epic 1 assumptions to match. Because reset is an absolute timestamp, the device must know wall-clock time — `poller_begin()` now starts NTP (`configTime`), and `poller_time_now()` returns 0 until it syncs (reset countdowns read 0 on the very first poll, correct thereafter). The PRD had NTP as an open question; the confirmed format makes it required.

### Files Changed

```text
m5clawd/certs.h            — new: pinned GTS Root R4 PEM + provenance notes
m5clawd/poller.ino         — new: TLS poll, header parse, error classify, NTP
m5clawd/m5clawd.ino        — poll loop (do_poll), backoff, auto-reconnect,
                             g_usage/g_pollState/g_apiKey globals; lib includes
m5clawd/config.h           — includes state_machine.h; poller prototypes;
                             POLL_HTTP_TIMEOUT_MS + NTP_SERVER_* constants
m5clawd/parse_headers.{h,cpp} — revised to confirmed formats (fraction / epoch)
m5clawd/test/parse_headers_test.cpp — tests updated to the confirmed formats
docs/Phase 2/PHASE_TASKS.md — Epic 2 checked off
```

### Verification

- `arduino-cli compile --profile m5clawd` — clean, no warnings. Sketch now **85%** of flash (was 71%; +14% is mbedTLS/WiFiClientSecure/HTTPClient). ~189 KB headroom remains for Epic 3.
- `m5clawd/test/run.sh` — 100 checks across 3 suites, all pass.
- TLS root verified against the live host with `openssl` (see Decisions).
- **Not yet verified on hardware:** the actual on-device TLS handshake, a real poll, and the first-poll header-name dump. Needs a flash (smoke Task 5.2).

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| `2_ARCHITECTURE.md` + `PHASE_PRD.md` say `-week-` rate-limit headers | MED | Docs contradict the shipped code (`-7d-`) | Correct both during `/6_doc`. The code/`PHASE_TASKS.md` are authoritative. |
| On-device TLS/poll unverified | MED | Poller is compile-clean + host-probed but never run on the device | Flash and watch serial for `[poll] code=200` and the first-poll header dump. If headers read `(empty)`, the names need another look. |
| Sketch at 85% of flash | LOW | Headroom shrinking | Fine for Epic 3 (UI draw code is small). Watch it; an OTA partition scheme may need thought before Phase 3 art. |

### Next Session Should

1. Continue `/3_dev` Phase 2 with **Epic 3** — the Usage screen + status overlays (`ui.ino`). It consumes `g_usage`; the Epic 1 formatters (`format_helpers`) are ready and unused so far.
2. Then **Epic 4** — last-known-good `UsageData` persistence to NVS + heap/reconnect hardening.
3. When hardware is available, flash and run smoke Task 5.2 — this is the first real on-device TLS/poll test.

---

## Session 3 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1 h
**Focus:** `/2_pm` Phase 2 planning, then `/3_dev` Phase 2 Epic 1 — the pure-logic foundation for the Anthropic poller.

### What happened

Planned Phase 2 (Anthropic poller + Usage UI; 16 tasks, 5 epics — see `docs/Phase 2/`), then implemented Epic 1 in worktree `../theClaw-phase-2-20260515` (branch `phase-2-20260515`). Epic 1 is the Arduino-header-free module layer the poller and the Usage UI will both consume; all of it is host-testable without an ESP32 toolchain.

### Completed (Phase 2 Epic 1 — 5/16 tasks)

- [x] **1.1** `UsageData` struct + `Status` enum moved out of `config.h` into a new pure `usage_data.h` (`<stdint.h>` only); added a `bool stale` field for last-known-good display. `config.h` includes it.
- [x] **1.2** `parse_headers.{h,cpp}` — `parse_anthropic_headers()` parses the four `anthropic-ratelimit-unified-*` header values into `UsageData`.
- [x] **1.3** `format_helpers.{h,cpp}` — `format_reset_countdown()` and `format_relative_time()`.
- [x] **1.4** `state_machine.{h,cpp}` — poll-outcome -> `Status` mapping + exponential backoff.
- [x] **1.5** `test/` host unit-test harness — `run.sh` + three suites, 102 checks, all passing.

### Files Changed

```text
m5clawd/usage_data.h        — new: pure UsageData struct (was inline in config.h)
m5clawd/config.h            — UsageData removed; #include "usage_data.h" added
m5clawd/parse_headers.{h,cpp} — new: rate-limit header parser
m5clawd/format_helpers.{h,cpp} — new: countdown / relative-time formatters
m5clawd/state_machine.{h,cpp}  — new: poll-state machine + backoff
m5clawd/test/{run.sh,test_util.h,*_test.cpp} — new: host g++ unit tests
docs/Phase 2/               — new: PRD, TASKS, IMP, DEPENDENCIES (from /2_pm)
```

### Decisions made

- **`UsageData` extracted to its own pure header.** The pure-logic modules cannot include `config.h` (it pulls in `<Arduino.h>`). `usage_data.h` is `<stdint.h>`-only so host `g++` compiles it. `config.h` includes it — no behavior change for the device build.
- **Backoff bounds are function parameters, not hardcoded.** `sm_next_delay_s(st, base_s, cap_s)` takes the bounds so `state_machine.cpp` stays free of `config.h`; the firmware passes `POLL_INTERVAL_S` / `POLL_BACKOFF_MAX_S`, the tests pass literals.
- **Header value FORMATS are documented assumptions, not confirmed.** `parse_headers.h` spells out the assumed utilization (fraction vs percent) and reset (relative seconds vs absolute epoch) formats. These MUST be confirmed against a real `200` response in Task 2.3 — the parser is built to be revised there.
- **`secret_redactor()` is not in the host harness.** It lives in `secrets_store.ino` and takes an Arduino `String`; host-testing it would mean refactoring Phase 1 code for a one-line function already verified on-device. Left device-side. (QA doc lists it as a unit-test target — this is a deliberate, documented deviation.)

### Verification

- `m5clawd/test/run.sh` — 102 checks across 3 suites, all pass.
- `arduino-cli compile --profile m5clawd` — clean, exit 0 (sketch 71% of flash). The pure `.cpp` modules compile into the firmware; the `test/` subdir is not picked up by arduino-cli, as intended.

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| Rate-limit header value formats unconfirmed | MED | `parse_headers` may need adjustment | Names + formats are assumptions. Task 2.3 dumps all headers from a live response; revise `parse_headers.{h,cpp}` + its tests then. |

### Next Session Should

1. Continue `/3_dev` Phase 2 with **Epic 2** — the Anthropic HTTPS poller.
2. **Do Task 2.1 first** (TLS root CA validation) — it is the phase's highest-risk node. Probe `api.anthropic.com`'s served chain with `openssl s_client` before writing poll code.
3. Epic 3 (Usage UI) can be worked in parallel with Epic 2 — it only needs Epic 1's `UsageData` + formatters, not a live poll.

---

## Session 2 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~1.5 h
**Focus:** `/3_dev` Phase 1 — toolchain validation, copy-strip, captive portal, screens, reset gesture. Firmware compiles clean and is validated on hardware; Phase 1 complete.

### What happened

Worked Phase 1 Epics 1–4 in worktree `../theClaw-phase-1-20260515`. The crypto ticker (4296-line `.ino`) was copied in and reduced to a 549-line, 4-tab skeleton that compiles clean against the pinned toolchain.

### Toolchain finding (important — read before next build)

`arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32` **fails** on this Mac. The global sketchbook (`~/Documents/Arduino/libraries`) carries a generic Arduino `SD` library (`architectures=*`) that shadows the ESP32 core's own `SD(esp32)`. Because `M5Stack.h` force-includes `<SD.h>`, the broken SD is picked and the build dies with "Architecture or board not supported".

**Fix:** added `m5clawd/sketch.yaml` — an arduino-cli build profile named `m5clawd`. Building with `--profile m5clawd` resolves libraries only from the profile + platform-bundled set, ignoring the global sketchbook. `flash.sh` was updated to use `--profile`. **All builds must use `arduino-cli compile --profile m5clawd`** (or `./flash.sh`), not a bare `--fqbn` invocation. This is *not* an ADR 001 change — ESP32 core 1.0.4 itself installs and compiles fine; the issue was library shadowing. ESP32 core 1.0.4 + M5Stack 0.3.1 + ArduinoJson 6.17.3 all confirmed working.

Note: `setup-toolchain.sh` downgraded the globally-installed M5Stack (0.4.6 -> 0.3.1) and ArduinoJson (7.4.2 -> 6.17.3) to the pinned versions. These are global; the build profile pins them anyway, but other sketches on this Mac now see the older versions.

### Decisions made

- **Build profile over bare FQBN** — `sketch.yaml` profile `m5clawd`; isolates from the global sketchbook and pins exact versions. `flash.sh` updated.
- **`startConfigPortal()` not `autoConnect()`** for provisioning — so the portal opens even when WiFi creds already exist (needed for re-onboarding after a rejected key). `wifi_portal_begin()` loops the portal until a valid key is saved, then `ESP.restart()`s into station mode.
- **Two-stage reset gesture** — hold C 5 s -> confirm screen; +2 s more -> wipe. Release early aborts.

### Completed

- [x] **1.1** toolchain validated; throwaway `M5.begin()` sketch compiles
- [x] **2.1–2.4** copy-strip: 4296-line crypto ticker -> 549-line skeleton across `m5clawd.ino` / `wifi_portal.ino` / `secrets_store.ino` / `ui.ino`; compiles clean
- [x] **3.1–3.3** captive portal with one Anthropic API-key field; NVS persistence; redactor; provisioning screen with QR
- [x] **4.1–4.3** boot-mode selection, splash/connecting/status/error screens, button-A cycling, button-C reset gesture + backlight toggle
- [x] **1.2** flashed to the M5Stack Core; serial round-trip confirmed at 115200
- [x] **5.1** all four hardware smoke scenarios pass (see Hardware validation below)
- [x] **5.2** docs updated (this entry, PHASE_TASKS)

### Files Changed

```text
m5clawd/m5clawd.ino        — REPLACED: crypto ticker -> stripped skeleton (globals, setup, loop, buttons)
m5clawd/wifi_portal.ino    — new: WiFiManager glue, one API-key param
m5clawd/secrets_store.ino  — new: NVS persistence + secret_redactor
m5clawd/ui.ino             — new: text-only screens (splash/connecting/status/provisioning/error/reset)
m5clawd/config.h           — added cross-tab function prototypes + <Arduino.h>
m5clawd/sketch.yaml        — new: arduino-cli build profile (the SD-shadowing fix)
m5clawd/flash.sh           — updated to build with --profile m5clawd
docs/Phase 1/PHASE_TASKS.md — checkboxes updated
```

### Hardware validation (same session)

Flashed and smoke-tested on the physical M5Stack Core. Device port: `/dev/cu.usbserial-02132522` (CP2104, plug-and-play on macOS — no driver step). All four Task 5.1 scenarios pass:

- **Fresh flash -> portal**: AP `M5Clawd-0D0A10` raised, provisioning screen renders (CONFIGURE WIFI + SSID + QR).
- **Valid onboarding**: a malformed key was correctly rejected and the portal reopened; a valid key was accepted, persisted, and the device auto-rebooted into station mode — serial showed `[boot] configured -> station mode` / `[wifi] connected, IP 192.168.0.154`; status screen renders.
- **Reflash -> creds survive**: a normal (non-erase) reupload kept NVS; device booted straight to the status screen.
- **Reset gesture**: long-press C wiped NVS and returned to the provisioning screen.

Serial-capture note: `arduino-cli monitor` exits immediately when run as a detached background process (no stdin/TTY). The pattern that works is a foreground call — `arduino-cli monitor ... >log & MPID=$!; sleep N; kill $MPID` — and it only captures while the device is actively emitting (i.e. catch a reset/boot inside the window).

### Known Issues

| Issue | Severity | Impact | Notes |
| ----- | -------- | ------ | ----- |
| First post-provisioning boot once needed a manual power-cycle to connect | LOW | One-off; did not reproduce on retest | Likely a flaky first WiFi connect right after the AP/STA teardown. If it recurs, revisit `station_connect()` — the 30 s timeout + single retry may need more retries or a short pre-connect delay. |
| `M5.Lcd.qrcode()` scannability not formally verified | LOW | QR may need version/width tuning | QR renders on the provisioning screen; not test-scanned. Params: text, x, y, width=100, version=4. |
| Global libs downgraded | LOW | Other sketches on this Mac see M5Stack 0.3.1 / ArduinoJson 6.17.3 | Intended (matches ADR 001 pins); the crypto ticker also wants these versions. |

### Next Session Should

1. Phase 1 is **complete and hardware-validated** -> run `/2_pm` for Phase 2 (the Anthropic HTTPS poller + the Usage screen).
2. Optionally photograph the device screens for the PR record.
3. If the one-off power-cycle issue recurs, harden `station_connect()`.

---

## Session 1 — 2026-05-15

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~40 min
**Focus:** Continuation of `/1_start` — user surfaced a second reference project; revised the tech-stack decisions and doc set to match it.

### What happened

The user pointed at `~/GIT/Crypto_Coin_TickerUS_Stock` — a **working M5Stack Core ticker that already uses WiFiManager the way the user wants**. They chose to start M5Clawd as a **copy-and-strip of that project**, and left the build-toolchain choice to the agent.

This invalidated the original (Session 0) tech-stack plan, which had assumed PlatformIO + M5Unified + ArduinoJson 7. The crypto ticker is Arduino IDE + classic `M5Stack.h` + ArduinoJson 6.17.3 — a proven toolchain on the exact hardware.

### Decisions made

- **Toolchain:** Arduino sketch built via **`arduino-cli`** (scriptable, but stays Arduino-IDE-compatible). Overrides ADR 001's PlatformIO choice.
- **HAL:** classic `M5Stack.h` (lib 0.3.1), not M5Unified. `M5.Lcd.*` drawing, no LVGL/M5GFX.
- **JSON:** ArduinoJson 6.17.3 (match the reference; v6 API).
- **Starting point:** `m5clawd/` will be a copy-and-strip of the crypto ticker's canonical sketch — keep WiFiManager onboarding / M5 display / buttons / `Preferences`; strip Binance WebSockets, charts, 25-pair management, SHT3X sensor, NeoPixel, SD-card config, M5StackUpdater, Timezone.
- **References:** there are now **two** read-only reference projects — `Clawdmeter/` (concept) and `Crypto_Coin_TickerUS_Stock` (scaffolding source). Neither is ever edited.

### Completed

- [x] Explored the crypto ticker; extracted its WiFiManager pattern (`getParam`, `saveParamCallback`, `onConfigModeCallback`, `WiFiEvent`, `autoConnect`, dark theme)
- [x] **Revised ADR 001** — Arduino / `arduino-cli` / `M5Stack.h` / ArduinoJson 6 (original PlatformIO plan preserved in a revision note)
- [x] **Added ADR 006** — copy-and-strip the crypto ticker
- [x] **Revised ADR 004** — now covers both read-only reference projects
- [x] **Rewrote** `2_ARCHITECTURE.md`, `CLAUDE.md`, `README.md` for the Arduino toolchain + project layout (`.ino` tabs, not `src/*.cpp`)
- [x] **Patched** `3_DESIGN_SYSTEM.md` (M5.Lcd fonts), `4_QUALITY_ASSURANCE.md` (arduino-cli + host-`g++` tests), `1_PROJECT_OVERVIEW.md` (tech mentions)
- [x] **`git init`** at `/Users/kevinbrice/GIT/theClaw/`; added `.gitignore`; committed the foundation docs
- [x] **Scaffolded `m5clawd/`** — copied the vendored WiFiManager files from the crypto ticker (`WiFiManager.cpp/.h`, `wm_*.h`, `strings_en.h`); wrote `config.h` (constants + `UsageData` struct), `setup-toolchain.sh` (arduino-cli core/lib install), `flash.sh` (compile + upload). Committed.

### Files Changed

```text
Docs (revised this session)
  ├── docs/decisions/001-tech-stack-selection.md   — full rewrite (Arduino toolchain)
  ├── docs/decisions/004-port-vs-fork.md           — revised (two references)
  ├── docs/decisions/006-start-from-crypto-ticker.md — new
  ├── docs/decisions/000-index.md                  — added ADR 006, revision dates
  ├── docs/2_ARCHITECTURE.md                       — full rewrite
  ├── docs/3_DESIGN_SYSTEM.md                      — typography + asset pipeline
  ├── docs/4_QUALITY_ASSURANCE.md                  — build/test commands
  ├── docs/1_PROJECT_OVERVIEW.md                   — tech mentions, risk row
  ├── CLAUDE.md                                    — full rewrite
  └── README.md                                    — full rewrite

Scaffolding (new this session)
  ├── .gitignore                                   — new
  ├── m5clawd/config.h                             — new (constants + UsageData)
  ├── m5clawd/setup-toolchain.sh                    — new (arduino-cli install)
  ├── m5clawd/flash.sh                              — new (compile + upload)
  └── m5clawd/{WiFiManager.cpp,.h,wm_*.h,strings_en.h} — copied from the crypto ticker
```

### Known Issues

| Issue                                              | Severity | Impact                                                              | Notes                                                                       |
| --------------------------------------------------- | -------- | ------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `m5clawd/m5clawd.ino` does not exist yet           | n/a      | The folder is not a valid Arduino sketch until the copy-strip runs   | Expected — scaffolding only. The sketch is Phase 1 task 1 (copy-strip, ADR 006). `flash.sh` errors clearly until then. |
| `arduino-cli` 1.2.2 + ESP32 core 1.0.4 unverified  | MED      | A modern arduino-cli may refuse to build against the 2020-era core 1.0.4 | **Validate first thing in Phase 1.** If it fails: revise ADR 001 + `setup-toolchain.sh` rather than silently bumping the core. |
| Crypto ticker has no license file                 | LOW      | M5Clawd's license can't be finalized until provenance is clear      | The reused glue is the user's own code, so likely fine; confirm pre-release  |

### Follow-Up Required

- [x] `git init` at `/Users/kevinbrice/GIT/theClaw/` and commit the foundation docs — done this session
- [ ] Run `/2_pm "Phase 1: copy-strip the crypto ticker + captive portal"` — Priority: HIGH (next step)
- [ ] Phase 1 task 1: copy the crypto ticker's canonical sketch into `m5clawd/m5clawd.ino`, split into `.ino` tabs, strip per ADR 006, get a clean `arduino-cli compile` — Priority: HIGH
- [ ] Validate `arduino-cli` + ESP32 core 1.0.4 by running `m5clawd/setup-toolchain.sh` on the dev Mac — Priority: HIGH
- [ ] Confirm exact M5Stack Core revision (Micro-USB vs USB-C) — Priority: LOW (M5Stack.h handles it)

### Context Notes

**Why the toolchain flipped:** Session 0 designed in the abstract (PlatformIO is "cleaner"). Session 1 had a concrete, working, on-hardware reference. Matching the thing we're copying beats an abstractly-nicer toolchain — copying a project and *then* converting its build system would be churn for no gain. `arduino-cli` recovers the scriptability that made PlatformIO attractive.

**The crypto ticker's WiFiManager is the asset.** The user explicitly likes its onboarding UX. The single most valuable thing we're inheriting is `wifi_portal.ino`'s pattern. Don't re-derive it.

### Next Session Should

1. Run `/2_pm "Phase 1: copy-strip the crypto ticker + captive portal"` to break Phase 1 into tasks.
2. Begin `/3_dev`: validate the toolchain (`m5clawd/setup-toolchain.sh`), then copy the crypto ticker's canonical sketch into `m5clawd/m5clawd.ino`, split into `.ino` tabs, strip per ADR 006, get a clean `arduino-cli compile`.

---

## Session 0 — 2026-05-14

**Agent / Developer:** Kevin Brice (with Claude Code, Opus 4.7 1M)
**Duration:** ~45 min
**Focus:** Project kickoff (`/1_start`) — discovery, architecture decisions, foundation docs.

### Completed

- [x] Cloned upstream reference `HermannBjorgvin/Clawdmeter` into `Clawdmeter/`
- [x] Identified two major mismatches with the user's stated goal:
      (1) no `wifimanager` branch in upstream — turned out to mean **the tzapu/WiFiManager library**, not a branch;
      (2) upstream targets **Waveshare ESP32-S3-Touch-AMOLED-2.16**, not M5Stack Core Basic
- [x] Confirmed direction with user: full port to M5Stack Core Basic + WiFi-direct (no host daemon) + captive portal
- [x] Created 5 ADRs documenting the decisions
- [x] Generated full doc set: `CLAUDE.md`, `README.md`, `docs/1_PROJECT_OVERVIEW.md`, `docs/2_ARCHITECTURE.md`, `docs/3_DESIGN_SYSTEM.md`, `docs/4_QUALITY_ASSURANCE.md`, this file

### Files Changed

```text
Docs
  ├── /Users/kevinbrice/GIT/theClaw/CLAUDE.md                       — new (replaces starterPack's own)
  ├── /Users/kevinbrice/GIT/theClaw/README.md                        — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/1_PROJECT_OVERVIEW.md       — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/2_ARCHITECTURE.md           — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/3_DESIGN_SYSTEM.md          — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/4_QUALITY_ASSURANCE.md      — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/HANDOFF_NOTES.md            — new (this file)
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/000-index.md      — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/001-tech-stack-selection.md       — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/002-wifi-vs-ble-daemon.md          — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/003-onboarding-via-wifimanager.md  — new
  ├── /Users/kevinbrice/GIT/theClaw/docs/decisions/004-port-vs-fork.md                — new
  └── /Users/kevinbrice/GIT/theClaw/docs/decisions/005-secrets-storage-nvs.md         — new

Reference
  └── /Users/kevinbrice/GIT/theClaw/Clawdmeter/                       — read-only clone (do not edit)
```

### Upstream Reference Pin

We cloned `HermannBjorgvin/Clawdmeter` at whatever HEAD was current on 2026-05-14. To check what we have:

```bash
git -C /Users/kevinbrice/GIT/theClaw/Clawdmeter log -1 --oneline
```

If upstream meaningfully evolves and we want to re-sync the reference:

```bash
git -C /Users/kevinbrice/GIT/theClaw/Clawdmeter pull
```

### Known Issues

| Issue                                                | Severity | Impact                                                                         | Notes                                                                                                |
| ---------------------------------------------------- | -------- | ------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| `m5clawd/` directory does not yet exist              | n/a      | Phase 1 must create it                                                          | This is expected — `/1_start` only creates docs                                                       |
| User has not validated hardware revision specifics   | LOW      | If they have a `Gray` variant (with IMU) vs vanilla Basic, MPU6886 detection logic might branch | M5Unified auto-detects, so likely fine. Confirm at first flash. |
| No git repo at project root yet                      | LOW      | Can't track changes across sessions                                             | Recommend `git init` at `/Users/kevinbrice/GIT/theClaw/` before Phase 1 starts                       |

### Follow-Up Required

- [ ] `git init` at `/Users/kevinbrice/GIT/theClaw/` and commit foundation docs — Priority: HIGH
- [ ] Add `Clawdmeter/` to `.gitignore` (or accept it as a vendored reference subdir — either is fine, document the choice) — Priority: MEDIUM
- [ ] Confirm exact M5Stack Core Basic revision the user owns (USB-C vs Micro-USB, FIRE variant, etc.) before Phase 1 — Priority: MEDIUM. M5Unified handles all of these but knowing prevents surprises.
- [ ] Run `/2_pm "Phase 1: bootstrap + captive portal + first poll"` to plan Phase 1 — Priority: HIGH (next step)

### Tech Debt Created

- **Plaintext NVS storage of API key.** Known limitation — see ADR 005. Plan to harden in a future phase via NVS encryption.
- **Bundled root CA may go stale.** We'll pick a CA at Phase 2 implementation time. Plan to add an OTA path before MVP ships so we have a recovery option if Anthropic rotates roots.

### Context Notes

**Hardware mismatch surfacing:** When the user said "check out wifimanager" they meant the **library**, not a branch — language ambiguity. Upstream has no WiFi at all; everything goes over BLE to a host daemon. Caught this early, saved a wasted session.

**Original CLAUDE.md was replaced:** The previous `/Users/kevinbrice/GIT/theClaw/CLAUDE.md` described the starterPack itself (the toolkit). It is fully preserved in git history once we `git init`. The new one is for the M5Clawd project.

**Design philosophy decided:** "Easy to setup once cloned" is the cardinal UX rule. Every design call should be re-checked against that — if it adds friction at first boot, push back.

### Next Session Should

1. Confirm the M5Stack Core Basic revision (Micro-USB vs USB-C; FIRE vs Gray vs Basic).
2. Run `/2_pm "Phase 1: bootstrap + captive portal + first poll"` to break Phase 1 into tasks.
3. Then `/3_dev` to start implementing: create `m5clawd/platformio.ini`, scaffold `src/main.cpp`, etc.

---

## Template for New Sessions

```markdown
## Session [N] — [DATE]

**Agent / Developer:** [NAME]
**Duration:** [TIME_SPENT]
**Focus:** [BRIEF_SUMMARY]

### Completed
- [x] [TASK]

### Files Changed
```text
[CATEGORY]
  ├── [filepath] — [WHAT_CHANGED]
```

### Known Issues
| Issue | Severity | Impact | Notes |
|-------|----------|--------|-------|

### Follow-Up Required
- [ ] [ACTION] — Priority: [LEVEL]

### Tech Debt Created
- [DEBT] — [REASON] — [WHEN_TO_ADDRESS]

### Context Notes
[IMPORTANT_INFO]

### Next Session Should
1. [NEXT_STEP]
```

---

## Quick Reference

### File categories

- **Firmware:** `m5clawd/*.ino`, `m5clawd/*.h`
- **Build / tooling:** `m5clawd/setup-toolchain.sh`, `m5clawd/flash.sh`, `m5clawd/test/*`
- **Reference:** `Clawdmeter/*` (read-only)
- **Docs:** `docs/*`, `CLAUDE.md`, `README.md`

### Severity levels

- **CRITICAL:** Device bricks, secrets leak, can't onboard
- **HIGH:** Blocks Phase progress or causes data loss
- **MEDIUM:** Impacts functionality but has workaround
- **LOW:** Cosmetic or edge-case only

### Priority levels

- **HIGH:** Must be done next session
- **MEDIUM:** Should be done within current phase
- **LOW:** Backlog
