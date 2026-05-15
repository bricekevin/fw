// secrets_store.ino — NVS persistence for the Anthropic API key.
//
// The key lives in the Preferences namespace defined in config.h. It is never
// printed to serial in the clear: secret_redactor() guards every log line.

// True once a well-formed API key has been saved.
bool secrets_is_configured() {
  preferences.begin(NVS_NAMESPACE, true);          // read-only
  bool configured = preferences.getBool(NVS_KEY_CONFIGURED, false);
  preferences.end();
  return configured;
}

// The stored API key, or "" if none. Used by the Phase 2 poller.
String secrets_get_api_key() {
  preferences.begin(NVS_NAMESPACE, true);
  String key = preferences.getString(NVS_KEY_API_KEY, "");
  preferences.end();
  return key;
}

// Redact a key for logging — never emit the raw secret.
const char *secret_redactor(const String &k) {
  return k.startsWith("sk-ant-") ? "sk-ant-***" : "***";
}

// WiFiManager save callback: validate the submitted key, persist it on success.
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
void secrets_reset() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  wifiManager.resetSettings();
}
