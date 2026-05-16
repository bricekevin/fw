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

To find the OAuth parameters: observe how `claude` logs in (it offers a
headless "open this URL, paste the code back" path when it can't open a
browser — that path is the model for the device's onboarding), and check
Anthropic's OAuth documentation. Record findings here:

```text
client_id        : <fill in from spike>
authorize URL    : <fill in>
token URL        : <fill in>
scopes           : user:inference, ... (from the credential blob)
PKCE             : S256 (confirm)
refresh grant    : POST <token URL>  grant_type=refresh_token&refresh_token=..&client_id=..
refresh rotates? : <yes/no — from Task 1.2>
```

Prove the refresh grant before touching firmware:

```bash
# Off-device refresh exchange — confirms the grant works and shows the shape.
curl -sS -X POST <token URL> \
  -H 'content-type: application/json' \
  -d '{"grant_type":"refresh_token","refresh_token":"<rt>","client_id":"<id>"}'
```

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
