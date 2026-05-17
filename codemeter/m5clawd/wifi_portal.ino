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

// Stage 1 only — track the soft-AP client: a phone joining advances the LCD
// from step 1 (join the AP) to step 2 (open the portal); a phone leaving
// returns it to step 1 so it (or another phone) can rejoin.
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

// Injected into every WiFiManager page's <head> so the portal matches the
// device — Anthropic-orange accents on the warm-dark background, replacing the
// library's default blue. Loaded after the library's own stylesheet, so these
// rules win the cascade.
static const char PORTAL_CSS[] =
    "<style>"
    "body.invert,body.invert a,body.invert h1{background:#1A1815;color:#fff}"
    "h1,h3{color:#DA7756}"
    "button,input[type='button'],input[type='submit']"
    "{background-color:#DA7756;border-radius:.4rem}"
    "input{border-radius:.4rem}"
    "a:hover{color:#DA7756}"
    ".msg.P{border-left-color:#DA7756}.msg.P h4{color:#DA7756}"
    "</style>";

// --- Stage 1 — WiFi credentials over the soft-AP captive portal ------------
// startConfigPortal() blocks until the portal exits; onWifiSaved() sets the
// configured flag once WiFi credentials are saved and the connection succeeds,
// so the loop ends and the device reboots into station mode. No custom field
// here — the ADR 003 API-key field is gone (superseded by OAuth, ADR 007).
//
// This stage is intentionally BLOCKING. A non-blocking portal was tried so the
// button-C reset could be serviced here, but blocking startConfigPortal()
// retries a flaky first connect internally (hardware logs show the first STA
// connect often fails, then succeeds) — the non-blocking loop lost that retry
// and got stuck "connected but not saved". The reset gesture is not needed in
// Stage 1 anyway: no credentials are stored yet, so there is nothing to wipe.
void wifi_portal_wifi_stage() {
  wifiManager.setSaveConfigCallback(onWifiSaved);
  wifiManager.setAPCallback(onConfigModeCallback);
  wifiManager.setTitle("M5CLAWD SETUP");
  wifiManager.setClass("invert");                  // dark theme
  wifiManager.setCustomHeadElement(PORTAL_CSS);    // device-matched styling
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
      String("<br/><b>Step 1.</b> Tap to log in (opens a new tab):"
             "<a href='") + authUrl +
      "' target='_blank' style='display:block;padding:14px;margin:8px 0;"
      "background:#DA7756;color:#fff;text-align:center;border-radius:6px;"
      "text-decoration:none;font-weight:bold'>Log in with Claude</a>"
      "<b>Step 2.</b> Claude shows a one-time code &mdash; copy it.<br/>"
      "<b>Step 3.</b> Return to <i>this</i> tab, paste the code below, "
      "then tap Save.<br/>";
  new (&oauthLoginField) WiFiManagerParameter(oauthLoginHtml.c_str());
  new (&oauthCodeField) WiFiManagerParameter(
      PARAM_ID_OAUTH_CODE, "One-time code", "", 256,
      "placeholder=\"paste the code from Claude\"");
  wifiManager.addParameter(&oauthLoginField);
  wifiManager.addParameter(&oauthCodeField);

  wifiManager.setSaveParamsCallback(oauthCodeSaveCallback);
  wifiManager.setTitle("M5CLAWD - LOG IN");
  wifiManager.setClass("invert");
  wifiManager.setCustomHeadElement(PORTAL_CSS);    // device-matched styling
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"param", "exit"};
  wifiManager.setMenu(wm_menu);

  // /param so the QR / link lands straight on the login + paste page,
  // skipping the WiFiManager menu's "Setup" click.
  String portalUrl =
      String("http://") + WiFi.localIP().toString() + "/param";
  ui_show_oauth_login(portalUrl);

  // Non-blocking web portal: pump process() so the LCD/buttons stay live.
  // The main loop() does not run during onboarding, so the button-C reset
  // gesture is handled here too — hold 5 s to arm, +2 s to wipe NVS + WiFi.
  wifiManager.startWebPortal();
  bool resetArmed = false;
  while (!g_oauth_onboarded) {
    wifiManager.process();
    M5.update();

    if (M5.BtnC.pressedFor(RESET_HOLD_MS) && !resetArmed) {
      resetArmed = true;
      ui_show_reset_confirm();
    }
    if (resetArmed && M5.BtnC.pressedFor(RESET_HOLD_MS + 2000)) {
      Serial.println("[reset] wiping NVS + WiFi credentials (Stage 2)");
      secrets_reset();
      delay(300);
      ESP.restart();
    }
    if (resetArmed && M5.BtnC.wasReleased()) {
      resetArmed = false;                  // released early -> aborted
      ui_show_oauth_login(portalUrl);      // repaint the step-3 screen
    }

    delay(20);
  }
  wifiManager.stopWebPortal();

  Serial.println("[portal] OAuth onboarded -> restarting");
  delay(500);
  ESP.restart();
}
