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

// A Claude token (sk-ant-...) is base64url-ish and never contains whitespace.
// A copy-paste can smuggle in stray spaces or newlines — e.g. when the token
// is copied from a line-wrapped terminal display. Strip every whitespace
// character so the stored/used credential is exactly the token.
static String token_clean(const String &raw) {
  String out;
  out.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') out += c;
  }
  return out;
}

// The OAuth access token used as the poller's Bearer credential. Falls back to
// the legacy `anthropic_key` for a not-yet-migrated device. Whitespace is
// stripped on read too, so a credential stored before token_clean() existed
// (or otherwise smudged) is still used correctly.
String secrets_get_access_token() {
  preferences.begin(NVS_NAMESPACE, true);
  String token = preferences.getString(NVS_KEY_OAUTH_AT, "");
  if (token.length() == 0) {
    token = preferences.getString(NVS_KEY_API_KEY, "");   // legacy fallback
  }
  preferences.end();
  return token_clean(token);
}

// Persist a freshly obtained credential — from onboarding (Epic 3) or a token
// refresh (oauth.ino). NVS has no cross-key transaction; the refresh token is
// written first so a power loss mid-write degrades to a refreshable or
// re-onboardable state, never a wedged one (see Epic 4.3).
void secrets_save_tokens(const String &access, const String &refresh,
                         uint32_t expires_at) {
  String cleanAccess  = token_clean(access);
  String cleanRefresh = token_clean(refresh);
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_KEY_OAUTH_RT, cleanRefresh);
  preferences.putUInt(NVS_KEY_OAUTH_EXP, expires_at);
  preferences.putString(NVS_KEY_OAUTH_AT, cleanAccess);
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

// WiFiManager setSaveConfigCallback — fired once the home WiFi credentials are
// saved and the connection succeeds. Records that WiFi setup is done, and —
// since the Claude-token field lives on the same page — also stores the token
// the user pasted there (doParamSave() has already copied it into tokenField).
void onWifiSaved() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBool(NVS_KEY_CONFIGURED, true);
  preferences.end();
  Serial.println("[portal] WiFi credentials saved");

  String token = String(tokenField.getValue());
  token.trim();
  if (token.length() > 0) {
    secrets_save_tokens(token, "", 0);   // long-lived token; no refresh / expiry
    Serial.printf("[portal] token saved from the WiFi page (%s)\n",
                  secret_redactor(token));
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
