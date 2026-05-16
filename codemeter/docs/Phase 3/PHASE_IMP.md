# Phase 3 - OAuth Onboarding + Token Refresh Implementation

**Status:** PLANNING
**Checkpoint:** v0.3.0

> Embedded-firmware phase. Epic 1 is research — its output fills in the
> constants and request shapes the later epics need. Code below is intent, not
> final, and is deliberately vague where the spike must resolve it.

---

## Epic 1 — the spike (do this first)

The goal is to answer, with evidence, the Open Questions in PHASE_PRD.md. Useful
probes (run from this Mac — the `claude` CLI and a Claude credential are here):

```bash
# The token endpoint host — pin its root CA the way Phase 2 Task 2.1 did.
openssl s_client -connect <token-host>:443 -servername <token-host> </dev/null \
  | openssl x509 -noout -issuer -subject

# Inspect the credential STRUCTURE only — never echo raw tokens (a heredoc/stdin
# collision leaked them once; see HANDOFF Session 7). Read fields, not secrets.
security find-generic-password -s "Claude Code-credentials" -w \
  | python3 -c "import sys,json; d=json.load(sys.stdin)['claudeAiOauth']; \
      print({k:(len(v) if isinstance(v,str) else v) for k,v in d.items()})"
```

### Task 1.1 findings — Claude Code OAuth parameters (recorded 2026-05-15)

Sources: the production OAuth config object inside the `claude` CLI binary
(`~/.local/share/claude/versions/2.1.143`, Mach-O — extracted with `strings` /
`grep -a`); the public client-metadata document; the local Keychain credential
*structure* (field names + lengths only, no secret values); and an `openssl
s_client` probe of the token host.

```text
client_id        : 9d1c250a-e61b-44d9-88ed-5944d1962f5e
                   (production UUID client. The binary also carries a second
                    config block, CLIENT_ID 22422756-60c9-4084-8eb7-27705fd5cf9a
                    with OAUTH_FILE_SUFFIX "-local-oauth" + localhost MCP proxy —
                    that is the dev/local build config; ignore it.)
authorize URL    : https://claude.com/cai/oauth/authorize        (CLAUDE_AI — subscription)
                   https://platform.claude.com/oauth/authorize   (CONSOLE — API account)
                   -> use the CLAUDE_AI one: M5Clawd reads Claude Code subscription
                      ("unified") rate limits, which are the claude.ai account's.
token URL        : https://platform.claude.com/v1/oauth/token
manual redirect  : https://platform.claude.com/oauth/code/callback
                   (MANUAL_REDIRECT_URL — the headless "paste the code back" path;
                    after auth claude.ai shows the code at
                    https://platform.claude.com/oauth/code/success?app=claude-code)
loopback redirect: http://localhost/callback , http://127.0.0.1/callback
                   (from the client-metadata doc — the browser-on-same-machine path;
                    NOT usable by a headless device — see Task 1.3 ADR)
scopes           : user:inference user:profile user:file_upload user:mcp_servers
                   user:sessions:claude_code   (from this account's credential blob;
                   M5Clawd only needs user:inference [+ user:profile])
PKCE             : S256  (code_challenge / code_challenge_method present in binary;
                   token_endpoint_auth_method = "none" -> public client, PKCE is
                   the only client proof; no client secret)
token resp fields: access_token, refresh_token, expires_in, token_type
refresh grant    : POST https://platform.claude.com/v1/oauth/token
                   content-type: application/json
                   {"grant_type":"refresh_token","refresh_token":"..",
                    "client_id":"9d1c250a-e61b-44d9-88ed-5944d1962f5e"}
                   -> 200 {"access_token","refresh_token","expires_in",
                           "token_type"}  (response field names confirmed in binary)
refresh rotates? : ASSUME YES — handle defensively. See Task 1.2 note below.
```

**Token-host root CA (affects Epic 2.2 / certs.h):** `platform.claude.com`
chains `Let's Encrypt E7` -> **ISRG Root X1**. This is a *different* root from
the poller's `api.anthropic.com` (GTS Root R4) — Epic 2.2 must bundle ISRG Root
X1 as a second pinned CA. (Caveat: Let's Encrypt leaf certs rotate every ~90
days; pin the *root*, ISRG Root X1, not the leaf or the E7 intermediate.)

**Credential structure** (`security find-generic-password -s "Claude
Code-credentials"`, JSON under key `claudeAiOauth`): `accessToken` (108 ch),
`refreshToken` (108 ch), `expiresAt` (ms epoch), `scopes` (array),
`subscriptionType`, `rateLimitTier`. Note `expiresAt` is **milliseconds**; the
NVS model in Epic 2.1 stores seconds.

### Task 1.2 — refresh grant proof: DEFERRED to on-device (decided 2026-05-15)

The off-device curl exchange below would have to use the live Keychain refresh
token; if the grant rotates the refresh token it would invalidate the Mac's
`claude` login. Decision (Kevin): **do not run it off-device** — the device's
own first refresh during Epic 5.2 hardware testing is the empirical proof, and
that is non-disruptive to the dev Mac. The grant shape is taken from the binary
config + the public client-metadata doc + standard OAuth `refresh_token`
semantics (recorded in the Task 1.1 block above).

```bash
# Reference only — NOT run in the spike. The device performs the equivalent.
curl -sS -X POST https://platform.claude.com/v1/oauth/token \
  -H 'content-type: application/json' \
  -d '{"grant_type":"refresh_token","refresh_token":"<rt>","client_id":"9d1c250a-e61b-44d9-88ed-5944d1962f5e"}'
```

**Consequence for the firmware:** rotation is unproven, so Epic 2.2 must treat
the grant as *possibly* rotating — if the response carries a `refresh_token`,
persist it atomically *before* the next refresh; if it does not, keep the
existing one. This defensive handling is correct whether rotation happens or
not. Epic 5.2 must explicitly observe and record the first real refresh
(does the response include a new `refresh_token`? does the old one still work?)
and back-fill the "refresh rotates?" answer here.

Then write the two ADRs (onboarding mechanism; refresh-token storage).

---

## Epic 2 — credential store + refresh

### Credential model (Task 2.1)

The single `anthropic_key` string becomes a small record. Keep NVS keys <= 15 chars.

```cpp
// stored in Preferences namespace NVS_NAMESPACE
//   "oauth_at"   access token   (String)
//   "oauth_rt"   refresh token  (String)
//   "oauth_exp"  expiresAt      (uint32 epoch, or uint64 ms -> store seconds)
```

A device with only the legacy `anthropic_key` and no `oauth_rt` cannot refresh
-> boot it straight into the "re-onboard required" state.

### Refresh client (Task 2.2)

```cpp
// oauth.ino — intent
bool oauth_refresh() {
  WiFiClientSecure client;                 // stack-scoped, as in poller.ino
  client.setCACert(TOKEN_HOST_ROOT_CA);    // may == ANTHROPIC_ROOT_CA
  HTTPClient http;
  http.begin(client, OAUTH_TOKEN_URL);
  http.addHeader("content-type", "application/json");
  int code = http.POST(refresh_body(stored_refresh_token()));
  if (code != 200) { http.end(); client.stop(); return false; }
  // parse access_token, expires_in, (rotated) refresh_token; persist atomically
  ...
  http.end(); client.stop();
  return true;
}
```

### Scheduling (Task 2.3)

Refresh decision is the pure module (Task 2.4): refresh when
`now >= expiresAt - REFRESH_MARGIN_S`, or after a poll returns `401`
(refresh once, then retry the poll). A failed refresh -> the state machine's
new re-onboard state, with bounded backoff so it does not hammer the endpoint.

```cpp
// refresh_policy.h — pure, host-testable
bool should_refresh(uint32_t expires_at, uint32_t now, uint32_t margin_s);
```

---

## Epic 3 — onboarding (authorize-URL + paste-back code, pending the ADR)

Two-stage: WiFiManager collects WiFi creds as today; once the device is on the
home LAN it shows the Claude authorize URL + a QR (`M5.Lcd.qrcode`). The user
authorizes on their phone and pastes the returned one-time code into a portal
field. The device runs the PKCE code exchange and stores the tokens.

PKCE: generate a random `code_verifier`, `code_challenge = S256(verifier)`;
the verifier is held in RAM for the session and used in the code exchange.

Fold the `station_connect()` hardening in here (Task 3.3): add retries / a
short pre-connect delay so the first post-onboarding boot connects without a
manual power-cycle.

---

## Testing

```bash
m5clawd/test/run.sh                                  # host tests incl. refresh_policy
cd m5clawd && arduino-cli compile --profile m5clawd  # build (profile — see Phase 1)
./flash.sh                                           # to the device on USB
```

Hardware (Task 5.2): onboard via the new flow; force a near-term expiry
(small `REFRESH_MARGIN_S`, or a short-lived token) and confirm the device
refreshes and keeps polling across the boundary; then a multi-day run.

Serial capture: `arduino-cli monitor` does not work non-TTY — use the pyserial
reset-and-read approach from Session 7 (see `docs/HANDOFF_NOTES.md`).

---

## Rollback Plan

Firmware on one dev device — rollback is reflashing a known-good build
(`git revert` + `./flash.sh`). A bad OAuth/refresh build fails closed: the UI
shows "re-onboard required" rather than bricking. A full-erase upload always
recovers the device (and wipes the stored credential — re-onboard after).

---

## Completion Checklist

- [ ] Spike done; OAuth parameters recorded; refresh proven off-device
- [ ] Two ADRs written (onboarding supersedes 003; refresh-storage revises 005)
- [ ] NVS credential model holds access + refresh token + expiry
- [ ] `oauth_refresh()` works; tokens persist; rotation handled
- [ ] Refresh scheduled (proactive + on-401); failure -> re-onboard state
- [ ] Onboarding flow works end-to-end with no raw-token paste
- [ ] `station_connect()` hardened (no power-cycle needed)
- [ ] Host tests green; hardware refresh verified across an expiry boundary
- [ ] Multi-day unattended run passes
- [ ] Docs + ADRs updated; HANDOFF entry added

---

**Updated:** 2026-05-15
