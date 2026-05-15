# Phase 2 - Anthropic Poller + Usage UI Implementation

**Status:** PLANNING
**Checkpoint:** v0.2.0

> Embedded-firmware phase — no database, no web backend, no server deployment.
> Code snippets below are intent sketches, not final code. They reference real
> symbols from `config.h` and the Phase 1 tabs.

---

## Setup

No new toolchain. Phase 1's `m5clawd` build profile (`sketch.yaml`) is reused.
**All builds must use the profile** — a bare `--fqbn` invocation fails on this
Mac (global-sketchbook SD shadowing; see HANDOFF_NOTES Session 2).

```bash
cd m5clawd
arduino-cli compile --profile m5clawd            # build check
./flash.sh                                       # compile + upload
arduino-cli monitor -p /dev/cu.usbserial-02132522 -c baudrate=115200
```

Host unit tests (new this phase):

```bash
m5clawd/test/run.sh                               # host-g++ compile + run
```

---

## Implementation

### Step 0 — extract `UsageData` to a pure header (Task 1.1)

The pure-logic modules cannot include `config.h` (it pulls in `<Arduino.h>`).
Move the struct into its own Arduino-free header.

```cpp
// usage_data.h  — pure, host-compilable
#pragma once
#include <stdint.h>

struct UsageData {
  uint8_t  session_pct;
  uint8_t  weekly_pct;
  uint32_t session_reset_s;
  uint32_t weekly_reset_s;
  uint32_t last_poll_unix;
  bool     stale;            // true = last-known-good, not a fresh poll
  enum Status {
    UNKNOWN, OK, WIFI_DOWN, API_UNREACHABLE, AUTH_FAILED, RATE_LIMITED,
  } status;
};
```

```cpp
// config.h  — replace the inline struct with:
#include "usage_data.h"
```

### TLS root CA — do this FIRST (Task 2.1)

The single highest-risk item. Probe the live chain before writing poll code:

```bash
openssl s_client -connect api.anthropic.com:443 -showcerts </dev/null 2>/dev/null \
  | openssl x509 -noout -issuer -subject
```

Bundle the chaining root(s) as PEM literals. Two is cheap insurance:

```cpp
// certs.h
#pragma once
// Amazon Root CA 1
static const char ANTHROPIC_ROOT_CA_1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)EOF";
// ISRG Root X1 (fallback if the chain ever moves to Let's Encrypt)
static const char ANTHROPIC_ROOT_CA_2[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)EOF";
```

`WiFiClientSecure::setCACert()` takes one cert. If the served chain is
ambiguous, try root 1, and on handshake failure retry once with root 2.
**Never** ship `client.setInsecure()`.

### The poll request (Tasks 2.2 - 2.3)

```cpp
// poller.ino  — intent sketch
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>      // v6 API
#include "certs.h"
#include "config.h"

static const char* RL_HEADERS[] = {
  "anthropic-ratelimit-unified-5h-utilization",
  "anthropic-ratelimit-unified-week-utilization",
  "anthropic-ratelimit-unified-5h-reset",
  "anthropic-ratelimit-unified-week-reset",
};

UsageData poller_poll(const String& apiKey) {
  UsageData d{};
  WiFiClientSecure client;                 // stack-scoped -> freed on return
  client.setCACert(ANTHROPIC_ROOT_CA_1);

  HTTPClient http;
  http.begin(client, ANTHROPIC_MESSAGES_URL);
  http.addHeader("x-api-key", apiKey);
  http.addHeader("anthropic-version", ANTHROPIC_API_VERSION);
  http.addHeader("content-type", "application/json");
  http.collectHeaders(RL_HEADERS, 4);

  const char* body =
    "{\"model\":\"" ANTHROPIC_POLL_MODEL "\",\"max_tokens\":1,"
    "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";
  int code = http.POST((uint8_t*)body, strlen(body));

  if (code == 200) {
    // FIRST real poll: dump every header to confirm the exact names.
    d = parse_anthropic_headers(
          http.header(RL_HEADERS[0]), http.header(RL_HEADERS[1]),
          http.header(RL_HEADERS[2]), http.header(RL_HEADERS[3]));
    d.status = UsageData::OK;
  } else {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, http.getString());
    const char* etype = doc["error"]["type"] | "";
    d.status = (code == 401)                  ? UsageData::AUTH_FAILED
             : (strcmp(etype,"rate_limit_error")==0) ? UsageData::RATE_LIMITED
             :                                   UsageData::API_UNREACHABLE;
  }

  http.end();
  client.stop();
  Serial.printf("[poll] code=%d heap=%u key=%s\n",
                code, ESP.getFreeHeap(), secret_redactor(apiKey));
  return d;
}
```

Heap discipline: `WiFiClientSecure` and `HTTPClient` are stack-scoped so they
destruct on return; `http.end()` + `client.stop()` are explicit on every path.

### Poll loop + backoff (Task 2.4)

The state machine (pure module) owns the backoff math. `loop()` just consults it.

```cpp
// m5clawd.ino loop()  — intent sketch
static uint32_t nextPollAt = 0;          // millis() deadline
static PollState pstate;                 // from state_machine.h

if (WiFi.status() == WL_CONNECTED && millis() >= nextPollAt) {
  UsageData d = poller_poll(g_apiKey);
  pstate = sm_advance(pstate, d.status);          // pure
  nextPollAt = millis() + sm_next_delay_s(pstate) * 1000UL;
  if (d.status == UsageData::OK) {
    usage_store_save(d);                          // NVS last-known-good
    g_usage = d;
  } else {
    g_usage.status = d.status;                    // keep stale numbers
    g_usage.stale  = true;
  }
  if (currentScreen == SCREEN_USAGE) ui_update_usage(g_usage);
}
```

Force-refresh (button B) just sets `nextPollAt = millis()`.

### Pure-logic modules (Epic 1)

Keep these Arduino-header-free — only `<cstdint>`, `<string>`, `<cstdlib>`, etc.
so `test/run.sh` can host-compile them.

```cpp
// format_helpers.cpp
std::string format_reset_countdown(uint32_t s) {
  if (s < 60)        return "<1m";
  uint32_t d = s/86400, h = (s%86400)/3600, m = (s%3600)/60;
  char buf[16];
  if (d) snprintf(buf,sizeof buf,"%ud %uh", d, h);
  else   snprintf(buf,sizeof buf,"%uh %um", h, m);
  return buf;
}
```

```cpp
// state_machine.h  — exponential backoff
struct PollState { uint8_t fails; };
inline uint32_t sm_next_delay_s(const PollState& s) {
  uint32_t d = POLL_INTERVAL_S << s.fails;       // 60,120,240,...
  return d > POLL_BACKOFF_MAX_S ? POLL_BACKOFF_MAX_S : d;
}
```

(`POLL_INTERVAL_S` / `POLL_BACKOFF_MAX_S` are in `config.h` — for the pure test
build, pass them in or mirror them in a tiny pure constants header.)

### Usage screen (Epic 3)

`M5.Lcd` direct drawing — layout per `docs/3_DESIGN_SYSTEM.md` "2. Usage".
Full paint on screen-switch; `ui_update_usage()` redraws only changed fields
(blank the old value's rect with `COLOR_BG`, draw the new). Big % in font 7,
titles via `setFreeFont`, countdowns in font 2. Status badge per the
"State Indicators" table — glyph + color.

---

## Testing

```bash
# Host unit tests
m5clawd/test/run.sh

# Build check
cd m5clawd && arduino-cli compile --profile m5clawd

# Flash + watch the first real poll (confirm header names!)
./flash.sh
arduino-cli monitor -p /dev/cu.usbserial-02132522 -c baudrate=115200
```

Smoke scenarios 2 and 3 from `docs/4_QUALITY_ASSURANCE.md`:
- **2 Happy path** — valid key -> Usage screen with real numbers within 90 s.
- **3 WiFi drop** — kill the router; `wifi-` within one interval, numbers
  persist; restore; `wifi` back within two intervals.

Performance: enable per-poll timing prints, confirm < 5 s p95; leave the device
running 24 h and check free heap + poll-success rate the next day.

---

## Rollback Plan

Firmware on a single dev device — "rollback" is reflashing a known-good build.

```bash
git log --oneline -10
git revert <bad_commit> --no-edit
cd m5clawd && ./flash.sh
```

If the device boot-loops, a full-erase upload recovers it (and wipes NVS, so
re-onboard via the portal afterward):

```bash
arduino-cli upload -e --profile m5clawd -p /dev/cu.usbserial-02132522 .
```

A bad bundled CA cannot brick the device — TLS just fails and the UI shows
`api-`; revert `certs.h` and reflash.

---

## Completion Checklist

- [ ] `UsageData` extracted to a pure `usage_data.h`; device build still clean
- [ ] TLS handshake to `api.anthropic.com` succeeds against a bundled root CA
- [ ] `poller.ino` POSTs and parses the four rate-limit headers into `UsageData`
- [ ] Body error parsing maps `401` / rate-limit / unreachable to a `Status`
- [ ] Poll loop runs every 60 s with exponential backoff on failure
- [ ] Usage screen renders session/weekly %, countdowns, status bar
- [ ] Status overlays cover every non-OK `Status`; stale data stays visible
- [ ] Last-known-good `UsageData` persists to NVS and restores on boot
- [ ] Buttons: A cycles 3 screens, B forces a poll
- [ ] `test/run.sh` passes, >= 60% coverage on pure modules
- [ ] Smoke scenarios 2 + 3 pass on hardware; serial log + screen photo in PR
- [ ] Single poll < 5 s p95; 24 h uptime/heap run started
- [ ] HANDOFF_NOTES, PHASE_TASKS, CHANGELOG updated

---

**Updated:** 2026-05-15
