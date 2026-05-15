// poller.ino — Anthropic HTTPS poll.
//
// Once per poll interval the firmware POSTs a minimal request to the Anthropic
// Messages API and reads the `anthropic-ratelimit-unified-*` response headers
// (see docs/2_ARCHITECTURE.md "Poller"). The response BODY is used only to
// classify errors; the usage numbers come entirely from the headers.
//
// Heap discipline: WiFiClientSecure and HTTPClient are stack-scoped so they
// destruct — and free their TLS buffers — on every return path. A leak here
// would surface as falling free heap over a 24 h run.

// WiFiClientSecure.h / HTTPClient.h are included from m5clawd.ino (the primary
// tab) so the HTTPClient type is visible where Arduino auto-generates this
// tab's function prototypes.
#include <time.h>
#include "certs.h"
#include "parse_headers.h"

// --- Rate-limit headers ----------------------------------------------------
// Names confirmed against the Clawdmeter reference daemon (note "7d", not
// "week"). HTTPClient only retains headers named here via collectHeaders().
static const char *RL_5H_UTIL  = "anthropic-ratelimit-unified-5h-utilization";
static const char *RL_7D_UTIL  = "anthropic-ratelimit-unified-7d-utilization";
static const char *RL_5H_RESET = "anthropic-ratelimit-unified-5h-reset";
static const char *RL_7D_RESET = "anthropic-ratelimit-unified-7d-reset";
static const char *RL_HEADERS[] = {RL_5H_UTIL, RL_7D_UTIL, RL_5H_RESET, RL_7D_RESET};

// The minimal request body — cheapest possible call; we only want the headers.
static const char POLL_BODY[] =
    "{\"model\":\"" ANTHROPIC_POLL_MODEL "\","
    "\"max_tokens\":1,"
    "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";

static bool s_headers_dumped = false;  // dump header values once, to confirm

// --- Time -------------------------------------------------------------------
// configTime() kicks off background NTP. Until it lands, time() returns a
// near-boot value; poller_time_now() reports 0 in that case so parse_reset()
// degrades gracefully (reset countdowns read 0 until the clock syncs).
void poller_begin() {
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  Serial.println("[poll] NTP sync requested");
}

uint32_t poller_time_now() {
  time_t t = time(nullptr);
  return (t > 1600000000) ? static_cast<uint32_t>(t) : 0;  // 0 = not yet synced
}

// --- Error classification ---------------------------------------------------
// Map an HTTP status (and, where useful, the body's error.type) to a
// PollOutcome the state machine understands.
static PollOutcome classify_error(int http_code, const String &body) {
  StaticJsonDocument<512> doc;
  const char *etype = "";
  if (deserializeJson(doc, body) == DeserializationError::Ok) {
    etype = doc["error"]["type"] | "";
  }

  if (http_code == 401 || http_code == 403) return POLL_AUTH_FAIL;
  if (http_code == 429)                     return POLL_RATE_LIMITED;
  if (strcmp(etype, "authentication_error") == 0) return POLL_AUTH_FAIL;
  if (strcmp(etype, "permission_error") == 0)     return POLL_AUTH_FAIL;
  if (strcmp(etype, "rate_limit_error") == 0)     return POLL_RATE_LIMITED;

  Serial.printf("[poll] error code=%d type=%s\n",
                http_code, etype[0] ? etype : "(none)");
  return POLL_API_UNREACHABLE;
}

// One-time dump of the retained rate-limit header values, so the first real
// poll on hardware confirms the header names actually resolved.
static void dump_headers_once(HTTPClient &http) {
  if (s_headers_dumped) return;
  s_headers_dumped = true;
  Serial.println("[poll] rate-limit headers (first poll):");
  for (size_t i = 0; i < sizeof(RL_HEADERS) / sizeof(RL_HEADERS[0]); i++) {
    String v = http.header(RL_HEADERS[i]);
    Serial.printf("  %s = \"%s\"\n", RL_HEADERS[i],
                  v.length() ? v.c_str() : "(empty)");
  }
}

// --- The poll ---------------------------------------------------------------
// Returns the outcome; on POLL_OK, *out holds the freshly parsed usage data.
// On any failure *out is left untouched (the caller keeps last-known-good).
PollOutcome poller_poll(const String &api_key, UsageData *out) {
  if (api_key.length() == 0) {
    Serial.println("[poll] no API key configured");
    return POLL_NO_KEY;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[poll] WiFi not connected");
    return POLL_WIFI_DOWN;
  }

  WiFiClientSecure client;                 // stack-scoped: TLS buffers freed on return
  client.setCACert(ANTHROPIC_ROOT_CA);

  HTTPClient http;
  http.setTimeout(POLL_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  if (!http.begin(client, ANTHROPIC_MESSAGES_URL)) {
    Serial.println("[poll] http.begin() failed");
    return POLL_API_UNREACHABLE;
  }
  http.addHeader("x-api-key", api_key);
  http.addHeader("anthropic-version", ANTHROPIC_API_VERSION);
  http.addHeader("content-type", "application/json");
  http.collectHeaders(RL_HEADERS, sizeof(RL_HEADERS) / sizeof(RL_HEADERS[0]));

  uint32_t t0 = millis();
  int code = http.POST(POLL_BODY);
  uint32_t elapsed = millis() - t0;

  PollOutcome outcome;
  if (code == 200) {
    dump_headers_once(http);
    UsageData d = parse_anthropic_headers(
        http.header(RL_5H_UTIL).c_str(), http.header(RL_7D_UTIL).c_str(),
        http.header(RL_5H_RESET).c_str(), http.header(RL_7D_RESET).c_str(),
        poller_time_now());
    if (d.status == UsageData::OK) {
      *out = d;
      outcome = POLL_OK;
    } else {
      // 200, but the rate-limit headers were missing or unparseable.
      Serial.println("[poll] 200 OK but rate-limit headers missing/bad");
      outcome = POLL_API_UNREACHABLE;
    }
  } else if (code > 0) {
    outcome = classify_error(code, http.getString());
  } else {
    // Negative codes are HTTPClient transport errors (TLS handshake, DNS, ...).
    Serial.printf("[poll] transport error: %s\n",
                  http.errorToString(code).c_str());
    outcome = POLL_API_UNREACHABLE;
  }

  Serial.printf("[poll] code=%d t=%ums heap=%u key=%s\n",
                code, elapsed, ESP.getFreeHeap(), secret_redactor(api_key));

  http.end();
  client.stop();
  return outcome;
}
