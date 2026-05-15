// wifi_portal.ino — WiFiManager captive-portal glue.
//
// The structure here is inherited verbatim from the Crypto_Coin_TickerUS_Stock
// reference (ADR 006): getParam(), WiFiEvent(), onConfigModeCallback(), and the
// setMenu / setClass("invert") / portal-start sequence. The one difference is
// the parameter set — M5Clawd registers exactly one custom field (the Anthropic
// API key) where the crypto ticker registered ~52.

// AP SSID: "M5Clawd-" + last 6 hex digits of the MAC, e.g. "M5Clawd-A1B2C3".
String ap_ssid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(WIFI_AP_SSID_PREFIX) + "-" + suffix;
}

// Read a submitted portal field from the WiFiManager web server.
String getParam(String name) {
  String value;
  if (wifiManager.server->hasArg(name)) {
    value = wifiManager.server->arg(name);
  }
  return value;
}

// Swap the LCD art as phones join / leave the soft-AP.
void WiFiEvent(WiFiEvent_t event, system_event_info_t info) {
  switch (event) {
    case SYSTEM_EVENT_AP_STACONNECTED:
      ui_portal_client_connected();
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ui_show_provisioning();
      break;
    default:
      break;
  }
}

// Fired by WiFiManager when the configuration portal opens.
void onConfigModeCallback(WiFiManager *myWiFiManager) {
  ui_show_provisioning();
}

// Run the captive portal until a well-formed API key is saved, then reboot
// cleanly into station mode. startConfigPortal() is used instead of
// autoConnect() so the portal opens even when WiFi credentials already exist
// (e.g. re-onboarding after a rejected key).
void wifi_portal_begin() {
  new (&apiKeyField) WiFiManagerParameter(
      PARAM_ID_ANTHROPIC_KEY, "Claude Code OAuth token", "", 200,
      "placeholder=\"sk-ant-oat01-...\" type=\"password\"");
  wifiManager.addParameter(&apiKeyField);

  wifiManager.setSaveParamsCallback(saveParamCallback);
  wifiManager.setAPCallback(onConfigModeCallback);
  wifiManager.setTitle("M5CLAWD SETUP");
  wifiManager.setClass("invert");                  // dark theme
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setMenu(wm_menu);

  WiFi.onEvent(WiFiEvent);

  // saveParamCallback() sets the configured flag only for a valid key, so a
  // rejected key leaves the loop running and reopens the portal.
  while (!secrets_is_configured()) {
    wifiManager.startConfigPortal(ap_ssid().c_str());
  }

  Serial.println("[portal] provisioned -> restarting into station mode");
  delay(500);
  ESP.restart();
}
