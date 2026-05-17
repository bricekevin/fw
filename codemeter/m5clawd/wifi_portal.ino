// wifi_portal.ino — WiFiManager onboarding, single soft-AP stage (ADR 010).
//
// Onboarding runs once, on the soft-AP captive portal:
//   - WiFiManager collects the home-WiFi credentials (its built-in wifi page).
//   - A custom /cred route ingests the Claude token, which the phone scans
//     from the host pairing helper's QR (pairing/m5clawd-pair.py).
// The device reboots into station mode once it holds both. There is no
// on-device OAuth — ADR 010 supersedes the ADR 007 two-stage login flow.
//
// The soft-AP / captive-portal mechanics (ap_ssid, getParam, WiFiEvent,
// onConfigModeCallback, setClass("invert"), setMenu) are inherited from the
// Crypto_Coin_TickerUS_Stock reference (ADR 006).

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

// --- /cred route — token ingest from the pairing-helper QR -----------------
// A small dark-themed result page for the phone (the soft-AP has no internet,
// so it is fully self-contained).
static void cred_reply(int code, const char *heading, const char *body) {
  String html =
      "<!doctype html><meta name=viewport content='width=device-width,"
      "initial-scale=1'><body style='background:#1A1815;color:#F5F0E8;"
      "font-family:-apple-system,Segoe UI,Roboto,sans-serif;text-align:center;"
      "padding:48px 24px'><h2 style='color:#DA7756'>";
  html += heading;
  html += "</h2><p>";
  html += body;
  html += "</p></body>";
  wifiManager.server->send(code, "text/html", html);
}

// GET /cred?t=<token> — hit when the user scans the pairing helper's QR with
// their phone camera. Saves the long-lived Claude token; if WiFi is already
// configured the device is fully paired and restarts, otherwise it prompts
// for the remaining WiFi step.
static void handleCredRoute() {
  String token = wifiManager.server->arg("t");
  token.trim();
  if (token.length() == 0) {
    cred_reply(400, "No token",
               "The pairing QR carried no token. Re-run the helper.");
    return;
  }

  secrets_save_tokens(token, "", 0);   // long-lived token; no refresh / expiry
  Serial.printf("[portal] token received via /cred (%s)\n",
                secret_redactor(token));

  if (secrets_is_configured()) {       // WiFi already done -> fully paired
    cred_reply(200, "All set",
               "M5Clawd is restarting and will start showing your usage.");
    delay(900);
    ESP.restart();
  } else {
    cred_reply(200, "Token saved",
               "Now open the setup page and enter your home WiFi to finish.");
  }
}

// Registered via setWebServerCallback, which fires before WiFiManager's own
// routes are bound — so /cred is served by the captive-portal web server.
static void registerCredRoute() {
  wifiManager.server->on("/cred", handleCredRoute);
}

// --- Onboarding — single soft-AP stage (ADR 010) ---------------------------
// Blocking captive portal. WiFiManager collects the home-WiFi credentials;
// the /cred route ingests the token. startConfigPortal() returns when WiFi is
// saved — if the token is not in yet the loop re-opens the portal so the user
// can still scan it; the /cred handler restarts directly when the token is
// what completes the pair. Either order (WiFi-first or token-first) works.
void wifi_portal_onboard() {
  wifiManager.setSaveConfigCallback(onWifiSaved);
  wifiManager.setAPCallback(onConfigModeCallback);
  wifiManager.setWebServerCallback(registerCredRoute);   // adds GET /cred
  wifiManager.setTitle("M5CLAWD SETUP");
  wifiManager.setClass("invert");                  // dark theme
  wifiManager.setCustomHeadElement(PORTAL_CSS);    // device-matched styling
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setMenu(wm_menu);

  WiFi.onEvent(WiFiEvent);

  while (!(secrets_is_configured() && secrets_has_token())) {
    wifiManager.startConfigPortal(ap_ssid().c_str());
  }

  Serial.println("[portal] paired (WiFi + token) -> restarting");
  delay(500);
  ESP.restart();
}
