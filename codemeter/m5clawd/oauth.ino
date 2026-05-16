// oauth.ino — Claude Code OAuth access-token refresh.
//
// The device holds an OAuth access token (~24 h life) and a long-lived refresh
// token. When the access token nears expiry — or a poll returns 401 — the
// firmware POSTs a refresh-grant to the token endpoint and persists the new
// credential. This is the standalone-device equivalent of what the `claude`
// CLI does in the background; see ADR 007 / ADR 008 and the Epic 1 spike in
// docs/Phase 3/PHASE_IMP.md.
//
// Refresh-token rotation is unproven (Task 1.2 was deferred to on-device — see
// PHASE_IMP) so it is handled defensively: if the response carries a new
// refresh token it replaces the stored one, otherwise the existing one is kept.
//
// Heap discipline mirrors poller.ino: WiFiClientSecure and HTTPClient are
// stack-scoped so their TLS buffers are freed on every return path.
//
// WiFi.h / WiFiClientSecure.h / HTTPClient.h / ArduinoJson.h are included from
// m5clawd.ino (the primary tab), so the types are visible where Arduino
// auto-generates this tab's prototypes.

#include "certs.h"

// Refresh-grant request body. JSON content-type, per the Epic 1 spike. The
// refresh token is a well-formed OAuth token (sk-ant-ort...) — alphanumeric
// and dashes only — so it needs no JSON escaping.
static String oauth_refresh_body(const String &refresh_token) {
  String body = "{\"grant_type\":\"refresh_token\",\"refresh_token\":\"";
  body += refresh_token;
  body += "\",\"client_id\":\"" OAUTH_CLIENT_ID "\"}";
  return body;
}

// Parse a 200 token response and persist the new credential. Returns
// REFRESH_OK on success, REFRESH_NET_ERROR if the body is missing/garbled (a
// transient-looking failure — safer to retry than to force a re-onboard).
static RefreshResult oauth_store_response(const String &payload,
                                          const String &old_refresh_token) {
  // Two ~108-char tokens plus a few scalar fields — 1024 B is ample headroom.
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println("[oauth] 200 but response body did not parse");
    return REFRESH_NET_ERROR;
  }

  const char *access  = doc["access_token"]  | "";
  const char *refresh = doc["refresh_token"] | "";   // absent if no rotation
  uint32_t    expires_in = doc["expires_in"] | 0u;

  if (access[0] == '\0') {
    Serial.println("[oauth] 200 but no access_token in response");
    return REFRESH_NET_ERROR;
  }

  // expires_in is relative; store an absolute epoch-seconds expiry. If the
  // wall clock is not yet NTP-synced, store 0 (unknown) — should_refresh()
  // treats that as "refresh again once the clock is valid".
  uint32_t now = poller_time_now();
  uint32_t expires_at = (now != 0 && expires_in != 0) ? now + expires_in : 0;

  // Rotation handled defensively: keep the old refresh token if none returned.
  String new_refresh = (refresh[0] != '\0') ? String(refresh)
                                            : old_refresh_token;
  if (refresh[0] != '\0') {
    Serial.println("[oauth] refresh token rotated");
  }

  secrets_save_tokens(String(access), new_refresh, expires_at);
  return REFRESH_OK;
}

// Exchange the stored refresh token for a fresh access token and persist it.
// Outcomes (see RefreshResult in config.h): REFRESH_OK, REFRESH_NO_TOKEN (no
// refresh token — re-onboard), REFRESH_INVALID_GRANT (endpoint rejected it —
// re-onboard), REFRESH_NET_ERROR (transient — retry with backoff).
RefreshResult oauth_refresh() {
  String refresh_token = secrets_get_refresh_token();
  if (refresh_token.length() == 0) {
    Serial.println("[oauth] no refresh token stored -> re-onboard required");
    return REFRESH_NO_TOKEN;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[oauth] WiFi not connected -- refresh deferred");
    return REFRESH_NET_ERROR;
  }

  WiFiClientSecure client;                 // stack-scoped: TLS buffers freed on return
  client.setCACert(OAUTH_ROOT_CA);         // ISRG Root X1 — see certs.h

  HTTPClient http;
  http.setTimeout(REFRESH_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.setUserAgent(ANTHROPIC_USER_AGENT);
  if (!http.begin(client, OAUTH_TOKEN_URL)) {
    Serial.println("[oauth] http.begin() failed");
    return REFRESH_NET_ERROR;
  }
  http.addHeader("content-type", "application/json");

  uint32_t t0 = millis();
  int code = http.POST(oauth_refresh_body(refresh_token));
  uint32_t elapsed = millis() - t0;

  RefreshResult result;
  if (code == 200) {
    result = oauth_store_response(http.getString(), refresh_token);
  } else if (code >= 400 && code < 500) {
    // A 4xx will not fix itself on retry — the refresh token is revoked,
    // expired, or otherwise rejected. Fail closed to the re-onboard state.
    StaticJsonDocument<512> doc;
    const char *etype = "";
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      etype = doc["error"] | "";
    }
    Serial.printf("[oauth] refresh rejected: code=%d error=%s\n",
                  code, etype[0] ? etype : "(none)");
    result = REFRESH_INVALID_GRANT;
  } else if (code > 0) {
    Serial.printf("[oauth] refresh server error: HTTP %d\n", code);
    result = REFRESH_NET_ERROR;           // 5xx — transient, retry
  } else {
    Serial.printf("[oauth] transport error: %s\n",
                  http.errorToString(code).c_str());
    result = REFRESH_NET_ERROR;
  }

  Serial.printf("[oauth] refresh code=%d t=%ums heap=%u result=%d\n",
                code, elapsed, ESP.getFreeHeap(), (int)result);

  http.end();
  client.stop();
  return result;
}
