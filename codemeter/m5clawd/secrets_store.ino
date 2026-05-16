// secrets_store.ino — NVS persistence for the OAuth credential record.
//
// Phase 3 (ADR 008) replaced the single Phase-2 credential string with a
// record of three values in the Preferences namespace from config.h:
//   oauth_at   access token   (String)
//   oauth_rt   refresh token  (String)
//   oauth_exp  access-token expiry, epoch seconds (uint32)
// The legacy `anthropic_key` is read only to detect a pre-Phase-3 device
// (CRED_LEGACY) so it can be steered into the re-onboard flow.
//
// Neither token is ever printed in the clear: secret_redactor() guards every
// log line.

// True once first-boot onboarding has completed (WiFi credentials saved). The
// credential check below (secrets_cred_state) is separate — a configured
// device can still need to re-onboard its OAuth credential.
bool secrets_is_configured() {
  preferences.begin(NVS_NAMESPACE, true);          // read-only
  bool configured = preferences.getBool(NVS_KEY_CONFIGURED, false);
  preferences.end();
  return configured;
}

// Which credential the device holds. A non-empty string is treated as present
// (getString's default "" stands in for a missing key — isKey() is not in the
// pinned ESP32 core 1.0.4).
CredState secrets_cred_state() {
  preferences.begin(NVS_NAMESPACE, true);
  bool hasAccess  = preferences.getString(NVS_KEY_OAUTH_AT, "").length() > 0;
  bool hasRefresh = preferences.getString(NVS_KEY_OAUTH_RT, "").length() > 0;
  bool hasLegacy  = preferences.getString(NVS_KEY_API_KEY, "").length() > 0;
  preferences.end();

  if (hasAccess && hasRefresh) return CRED_OAUTH;   // can self-refresh
  if (hasAccess || hasLegacy)  return CRED_LEGACY;  // a token, but no refresh
  return CRED_NONE;
}

// The OAuth access token used as the poller's Bearer credential. Falls back to
// the legacy `anthropic_key` for a not-yet-migrated device (whose poll will
// 401 once that token expires, steering it to re-onboard).
String secrets_get_access_token() {
  preferences.begin(NVS_NAMESPACE, true);
  String token = preferences.getString(NVS_KEY_OAUTH_AT, "");
  if (token.length() == 0) {
    token = preferences.getString(NVS_KEY_API_KEY, "");   // legacy fallback
  }
  preferences.end();
  return token;
}

// The refresh token, or "" if none is stored.
String secrets_get_refresh_token() {
  preferences.begin(NVS_NAMESPACE, true);
  String token = preferences.getString(NVS_KEY_OAUTH_RT, "");
  preferences.end();
  return token;
}

// Access-token expiry as Unix epoch seconds, or 0 if unknown.
uint32_t secrets_get_expires_at() {
  preferences.begin(NVS_NAMESPACE, true);
  uint32_t expires_at = preferences.getUInt(NVS_KEY_OAUTH_EXP, 0);
  preferences.end();
  return expires_at;
}

// Persist a freshly obtained credential — from onboarding (Epic 3) or a token
// refresh (oauth.ino). NVS has no cross-key transaction; the refresh token is
// written first so a power loss mid-write degrades to a refreshable or
// re-onboardable state, never a wedged one (see Epic 4.3).
void secrets_save_tokens(const String &access, const String &refresh,
                         uint32_t expires_at) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_KEY_OAUTH_RT, refresh);
  preferences.putUInt(NVS_KEY_OAUTH_EXP, expires_at);
  preferences.putString(NVS_KEY_OAUTH_AT, access);
  preferences.end();
  Serial.printf("[secrets] tokens saved (at=%s rt=%s exp=%u)\n",
                secret_redactor(access), secret_redactor(refresh),
                expires_at);
}

// Redact a token for logging — never emit the raw secret. Covers both OAuth
// token types: access (sk-ant-oat...) and refresh (sk-ant-ort...) tokens both
// share the sk-ant- prefix.
const char *secret_redactor(const String &k) {
  if (k.length() == 0)         return "(none)";
  if (k.startsWith("sk-ant-")) return "sk-ant-***";
  return "***";
}

// WiFiManager save callback: validate the submitted key, persist it on success.
// NOTE: this is the Phase-2 API-key portal field; Epic 3 replaces it with the
// OAuth onboarding flow (ADR 007).
void saveParamCallback() {
  String key = getParam(PARAM_ID_ANTHROPIC_KEY);
  key.trim();

  if (!key.startsWith("sk-ant-") || key.length() < 24) {
    Serial.println("[portal] API key rejected: must start with sk-ant-");
    ui_portal_hint("Invalid key - must start with sk-ant-");
    return;                                        // configured flag stays off
  }

  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_KEY_API_KEY, key);
  preferences.putBool(NVS_KEY_CONFIGURED, true);
  preferences.end();

  Serial.printf("[portal] API key saved (%s)\n", secret_redactor(key));
}

// Wipe all stored configuration — both our namespace and WiFiManager's creds.
// preferences.clear() drops the whole namespace, so the OAuth credential
// record (oauth_at / oauth_rt / oauth_exp) is wiped here too (ADR 008).
void secrets_reset() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  wifiManager.resetSettings();
}
