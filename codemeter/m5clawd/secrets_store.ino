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

// True once a Claude token has been stored (via the /cred pairing route, or
// the legacy key). Onboarding is complete only when this and
// secrets_is_configured() are both true (ADR 010).
bool secrets_has_token() {
  preferences.begin(NVS_NAMESPACE, true);
  bool has = preferences.getString(NVS_KEY_OAUTH_AT, "").length() > 0 ||
             preferences.getString(NVS_KEY_API_KEY, "").length() > 0;
  preferences.end();
  return has;
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

// WiFiManager setSaveConfigCallback (Stage 1) — fired once the home WiFi
// credentials are saved and the connection succeeds. The credentials live in
// the ESP32's own WiFi NVS; this only records that first-boot WiFi setup is
// done, so the next boot skips Stage 1 and goes to station mode.
void onWifiSaved() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBool(NVS_KEY_CONFIGURED, true);
  preferences.end();
  Serial.println("[portal] WiFi credentials saved");
}

// WiFiManager setSaveParamsCallback (Stage 2) — fired when the user submits the
// OAuth portal page. Exchange the pasted one-time code for tokens: on success
// flag onboarding complete (the Stage 2 loop then reboots); on failure leave a
// clear on-screen hint and keep the portal open for another attempt.
void oauthCodeSaveCallback() {
  String code = getParam(PARAM_ID_OAUTH_CODE);
  code.trim();
  if (code.length() == 0) return;                  // page saved, field empty

  switch (oauth_exchange_code(code)) {
    case EXCHANGE_OK:
      Serial.println("[portal] OAuth code accepted");
      g_oauth_onboarded = true;
      break;
    case EXCHANGE_BAD_CODE:
      ui_portal_hint("Code rejected - check it and paste again");
      break;
    case EXCHANGE_RATE_LIMITED:
      ui_portal_hint("Rate limited - wait, then log in again");
      break;
    case EXCHANGE_NET_ERROR:
    default:
      ui_portal_hint("Network error - try the code again");
      break;
  }
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

// Clear only the OAuth credential record (access + refresh + expiry, plus the
// legacy key), leaving the WiFi credentials and the configured flag intact.
// The "change credential" path (Task 3.4): the next boot skips Stage 1 and,
// finding CRED_NONE, re-enters Stage 2 OAuth onboarding.
void secrets_clear_oauth() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.remove(NVS_KEY_OAUTH_AT);
  preferences.remove(NVS_KEY_OAUTH_RT);
  preferences.remove(NVS_KEY_OAUTH_EXP);
  preferences.remove(NVS_KEY_API_KEY);
  preferences.end();
  Serial.println("[secrets] OAuth credential cleared (WiFi kept)");
}
