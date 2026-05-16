// wifi_portal.ino — WiFiManager onboarding, two staged portals (ADR 007/009).
//
// Onboarding is two independent WiFiManager sessions with a reboot between:
//
//   Stage 1  wifi_portal_wifi_stage()  — soft-AP captive portal. Collects the
//            home-WiFi credentials, then reboots into station mode.
//   Stage 2  wifi_portal_oauth_stage() — web portal on the home-LAN IP (no
//            soft-AP). Shows the "Log in with Claude" authorize URL and a
//            paste-back field for the one-time code; runs the code exchange.
//
// The soft-AP / captive-portal mechanics (ap_ssid, getParam, WiFiEvent,
// onConfigModeCallback, setClass("invert"), setMenu) are inherited from the
// Crypto_Coin_TickerUS_Stock reference (ADR 006). See ADR 009 for why the two
// stages live on two different network surfaces.

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

// Stage 1 only — swap the LCD art as phones join / leave the soft-AP. A joined
// phone advances onboarding step 1 (join the AP) to step 2 (open the portal).
void WiFiEvent(WiFiEvent_t event, system_event_info_t info) {
  switch (event) {
    case SYSTEM_EVENT_AP_STACONNECTED:
      ui_portal_client_connected();   // step 2 — "open the portal" QR
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ui_show_provisioning();         // step 1 — "join the AP" QR
      break;
    default:
      break;
  }
}

// Fired by WiFiManager when the Stage 1 configuration portal opens.
void onConfigModeCallback(WiFiManager *myWiFiManager) {
  ui_show_provisioning();
}

// --- Stage 1 — WiFi credentials over the soft-AP captive portal ------------
// startConfigPortal() blocks until the portal exits; onWifiSaved() sets the
// configured flag once WiFi credentials are saved and the connection succeeds,
// so the loop ends and the device reboots into station mode. No custom field
// here — the ADR 003 API-key field is gone (superseded by OAuth, ADR 007).
void wifi_portal_wifi_stage() {
  wifiManager.setSaveConfigCallback(onWifiSaved);
  wifiManager.setAPCallback(onConfigModeCallback);
  wifiManager.setTitle("M5CLAWD SETUP");
  wifiManager.setClass("invert");                  // dark theme
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setMenu(wm_menu);

  WiFi.onEvent(WiFiEvent);

  while (!secrets_is_configured()) {
    wifiManager.startConfigPortal(ap_ssid().c_str());
  }

  Serial.println("[portal] WiFi saved -> restarting into station mode");
  delay(500);
  ESP.restart();
}

// --- Stage 2 — OAuth onboarding, web portal on the home LAN ----------------
// Runs after the device is on the home network but still has no OAuth
// credential. Serves the WiFiManager web portal (no soft-AP, no captive DNS)
// on the station IP — the user's phone, still on home WiFi, reaches both
// claude.com and the device. The LCD shows the authorize-URL QR + the LAN
// portal address (ADR 009). startWebPortal() is non-blocking; process() is
// pumped until oauthCodeSaveCallback() reports a successful exchange.
void wifi_portal_oauth_stage() {
  oauth_pkce_begin();
  String authUrl = oauth_authorize_url();

  // The portal's "Log in with Claude" block — a read-only custom-HTML field.
  // oauthLoginHtml is a global so its buffer outlives the WiFiManagerParameter.
  oauthLoginHtml =
      String("<br/><a href='") + authUrl +
      "' target='_blank' style='display:block;padding:14px;margin-bottom:8px;"
      "background:#DA7756;color:#fff;text-align:center;border-radius:6px;"
      "text-decoration:none;font-weight:bold'>Log in with Claude</a>"
      "<p>Open the link, approve, then paste the one-time code below.</p>";
  new (&oauthLoginField) WiFiManagerParameter(oauthLoginHtml.c_str());
  new (&oauthCodeField) WiFiManagerParameter(
      PARAM_ID_OAUTH_CODE, "One-time code", "", 256,
      "placeholder=\"paste the code from Claude\"");
  wifiManager.addParameter(&oauthLoginField);
  wifiManager.addParameter(&oauthCodeField);

  wifiManager.setSaveParamsCallback(oauthCodeSaveCallback);
  wifiManager.setTitle("M5CLAWD - LOG IN");
  wifiManager.setClass("invert");
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"param", "exit"};
  wifiManager.setMenu(wm_menu);

  String portalUrl = String("http://") + WiFi.localIP().toString();
  ui_show_oauth_login(portalUrl);

  // Non-blocking web portal: pump process() so the LCD/buttons stay live.
  wifiManager.startWebPortal();
  while (!g_oauth_onboarded) {
    wifiManager.process();
    M5.update();
    delay(20);
  }
  wifiManager.stopWebPortal();

  Serial.println("[portal] OAuth onboarded -> restarting");
  delay(500);
  ESP.restart();
}
