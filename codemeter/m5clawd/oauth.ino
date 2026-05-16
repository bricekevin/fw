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
#include "oauth_pkce.h"
#include <mbedtls/sha256.h>
#include <esp_system.h>          // esp_fill_random()

// --- Onboarding: PKCE session state (ADR 007) ------------------------------
// One onboarding "session" is one code_verifier / state pair. They live in RAM
// only: a reboot mid-onboarding discards them and the user restarts the login
// step. The verifier is the client's PKCE proof for the Epic 3.2 code exchange;
// `state` is the anti-CSRF value echoed back in the redirect.
static String s_pkce_verifier;   // 43-char base64url code_verifier
static String s_oauth_state;     // 22-char base64url anti-CSRF state
static String s_authorize_url;   // the assembled "Log in with Claude" URL

// SHA-256(in), base64url-encoded without padding — the PKCE "S256" transform.
// Applied to the verifier to derive the code_challenge sent in the authorize
// URL. mbedtls ships with Arduino-ESP32; the _ret variants are the non-
// deprecated API on core 1.0.4's mbedtls 2.16.
static String pkce_s256_challenge(const String &verifier) {
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);          // 0 = SHA-256, not SHA-224
  mbedtls_sha256_update_ret(&ctx, (const unsigned char *)verifier.c_str(),
                            verifier.length());
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  return String(pkce_base64url(digest, sizeof(digest)).c_str());
}

// Mint a fresh PKCE verifier + challenge + `state` and assemble the authorize
// URL for one onboarding session. The hardware RNG (esp_fill_random) seeds both
// secrets. Call once when the "Log in with Claude" step begins.
void oauth_pkce_begin() {
  uint8_t verifier_bytes[32];                  // -> 43-char base64url verifier
  uint8_t state_bytes[16];                     // -> 22-char base64url state
  esp_fill_random(verifier_bytes, sizeof(verifier_bytes));
  esp_fill_random(state_bytes, sizeof(state_bytes));

  s_pkce_verifier =
      String(pkce_base64url(verifier_bytes, sizeof(verifier_bytes)).c_str());
  s_oauth_state =
      String(pkce_base64url(state_bytes, sizeof(state_bytes)).c_str());

  String challenge = pkce_s256_challenge(s_pkce_verifier);
  s_authorize_url = String(
      oauth_build_authorize_url(OAUTH_AUTHORIZE_URL, OAUTH_CLIENT_ID,
                                OAUTH_REDIRECT_URI, OAUTH_SCOPE,
                                challenge.c_str(), s_oauth_state.c_str())
          .c_str());

  // Never log the verifier — secret_redactor() guards it; the URL carries only
  // the (non-secret) challenge so it is safe to log for onboarding diagnostics.
  Serial.printf("[oauth] PKCE session ready (verifier=%s, url len=%u)\n",
                secret_redactor(s_pkce_verifier), s_authorize_url.length());
}

String oauth_authorize_url() { return s_authorize_url; }
String oauth_pkce_verifier() { return s_pkce_verifier; }
String oauth_state_token()   { return s_oauth_state; }

// --- Onboarding: authorization-code exchange (Epic 3.2, ADR 007) -----------

// Code-exchange request body — the OAuth `authorization_code` grant. `code`,
// `verifier` and `state` are URL-safe strings and redirect_uri is a fixed
// literal, so none need JSON escaping (oauth_exchange_code() rejects a code
// carrying a quote or backslash before this is called). `state` is echoed back
// in the grant to match the claude CLI's exchange request.
static String oauth_exchange_body(const String &code, const String &verifier,
                                  const String &state) {
  String body = "{\"grant_type\":\"authorization_code\",\"code\":\"";
  body += code;
  body += "\",\"client_id\":\"" OAUTH_CLIENT_ID "\",\"redirect_uri\":\"";
  body += OAUTH_REDIRECT_URI;
  body += "\",\"code_verifier\":\"";
  body += verifier;
  body += "\",\"state\":\"";
  body += state;
  body += "\"}";
  return body;
}

// Exchange the pasted one-time code for an access + refresh token and persist
// them. `pasted` is whatever the user copied from the claude.ai success page:
// it may be a bare "code" or "code#state" (the headless flow appends the state
// for the client to verify). The state, if present, is checked against this
// onboarding session's value. Returns EXCHANGE_OK / EXCHANGE_BAD_CODE
// (user re-pastes) / EXCHANGE_NET_ERROR (user retries). Heap discipline mirrors
// oauth_refresh(): WiFiClientSecure + HTTPClient are stack-scoped.
ExchangeResult oauth_exchange_code(const String &pasted) {
  String code = pasted;
  code.trim();

  // The success page may present "code#state" — split and verify the state.
  int hash = code.indexOf('#');
  if (hash >= 0) {
    String state = code.substring(hash + 1);
    code = code.substring(0, hash);
    if (s_oauth_state.length() > 0 && state != s_oauth_state) {
      Serial.println("[oauth] code rejected: state mismatch");
      return EXCHANGE_BAD_CODE;
    }
  }

  if (code.length() == 0 || code.indexOf('"') >= 0 ||
      code.indexOf('\\') >= 0) {
    Serial.println("[oauth] code rejected: empty or malformed");
    return EXCHANGE_BAD_CODE;
  }
  if (s_pkce_verifier.length() == 0) {
    Serial.println("[oauth] no PKCE session -- oauth_pkce_begin() not called");
    return EXCHANGE_NET_ERROR;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[oauth] WiFi not connected -- code exchange deferred");
    return EXCHANGE_NET_ERROR;
  }

  WiFiClientSecure client;                 // stack-scoped: TLS buffers freed on return
  client.setCACert(OAUTH_ROOT_CA);         // ISRG Root X1 — see certs.h

  HTTPClient http;
  http.setTimeout(REFRESH_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.setUserAgent(ANTHROPIC_USER_AGENT);
  if (!http.begin(client, OAUTH_TOKEN_URL)) {
    Serial.println("[oauth] http.begin() failed");
    return EXCHANGE_NET_ERROR;
  }
  http.addHeader("content-type", "application/json");

  uint32_t t0 = millis();
  int httpCode = http.POST(oauth_exchange_body(code, s_pkce_verifier,
                                               s_oauth_state));
  uint32_t elapsed = millis() - t0;

  ExchangeResult result;
  if (httpCode == 200) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) {
      Serial.println("[oauth] exchange 200 but response body did not parse");
      result = EXCHANGE_NET_ERROR;
    } else {
      const char *access     = doc["access_token"]  | "";
      const char *refresh    = doc["refresh_token"] | "";
      uint32_t    expires_in = doc["expires_in"]    | 0u;
      if (access[0] == '\0' || refresh[0] == '\0') {
        // Onboarding must yield both tokens — without a refresh token the
        // device cannot self-refresh. Unexpected; let the user retry.
        Serial.println("[oauth] exchange 200 but missing access/refresh token");
        result = EXCHANGE_NET_ERROR;
      } else {
        uint32_t now = poller_time_now();
        uint32_t expires_at =
            (now != 0 && expires_in != 0) ? now + expires_in : 0;
        secrets_save_tokens(String(access), String(refresh), expires_at);
        result = EXCHANGE_OK;
      }
    }
  } else if (httpCode >= 400 && httpCode < 500) {
    // The code is wrong, expired, already used, or the verifier mismatched —
    // none fixable by retrying the same code. Ask the user to re-paste.
    StaticJsonDocument<512> doc;
    const char *etype = "";
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      etype = doc["error"] | "";
    }
    Serial.printf("[oauth] exchange rejected: code=%d error=%s\n",
                  httpCode, etype[0] ? etype : "(none)");
    result = EXCHANGE_BAD_CODE;
  } else if (httpCode > 0) {
    Serial.printf("[oauth] exchange server error: HTTP %d\n", httpCode);
    result = EXCHANGE_NET_ERROR;          // 5xx — transient, retry
  } else {
    Serial.printf("[oauth] exchange transport error: %s\n",
                  http.errorToString(httpCode).c_str());
    result = EXCHANGE_NET_ERROR;
  }

  Serial.printf("[oauth] exchange code=%d t=%ums heap=%u result=%d\n",
                httpCode, elapsed, ESP.getFreeHeap(), (int)result);
  http.end();
  client.stop();
  return result;
}

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
