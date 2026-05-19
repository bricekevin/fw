// wifi_portal.ino — WiFiManager onboarding, single soft-AP stage (ADR 010).
//
// Onboarding runs once, on the soft-AP captive portal:
//   - WiFiManager collects the home-WiFi credentials (its built-in wifi page).
//   - A custom /cred route ingests the Claude token, which the phone scans
//     from the pairing page at encinitas3d.com/m5clawd.
// The device reboots into station mode once it holds both. There is no
// on-device OAuth — ADR 010 supersedes the ADR 007 two-stage login flow.
//
// The soft-AP / captive-portal mechanics (ap_ssid, getParam, WiFiEvent,
// onConfigModeCallback, setClass("invert"), setMenu) are inherited from the
// Crypto_Coin_TickerUS_Stock reference (ADR 006).

// AP SSID: WIFI_AP_SSID_PREFIX + last 6 hex digits of the MAC,
// e.g. "codeMeter-A1B2C3".
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

// Fired by WiFiManager when the Stage 1 configuration portal opens. The radio
// is now in soft-AP mode, so this is the point to pin TX power to maximum —
// a stronger beacon is easier to see and join in a noisy desk environment.
void onConfigModeCallback(WiFiManager *myWiFiManager) {
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
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

// Welcome / instructions block injected into the captive-portal menu page
// (setCustomMenuHTML). The first thing a new owner reads — it spells out the
// whole flow, including that a computer is needed to mint the token, so they
// are never left guessing what to do next.
static const char MENU_HTML[] =
    "<div style='text-align:left;background:#232019;border:1px solid #332f28;"
    "border-radius:10px;padding:14px 16px;margin:12px 0'>"
    "<h3 style='margin:0 0 6px'>Let's set up your codeMeter</h3>"
    "<p style='margin:0 0 10px;color:#bdb7ad'>About 2 minutes &mdash; you'll "
    "need a computer too.</p>"
    "<p style='margin:6px 0'><b>1.</b> On a computer, open "
    "<b>" WEB_HELPER_URL "</b> to get your Claude token.</p>"
    "<p style='margin:6px 0'><b>2.</b> Tap <b>Setup Device</b> &mdash; enter "
    "your home WiFi, paste the token, and Save.</p>"
    "</div>";

// Instructions injected just above the Claude-token input on the setup page.
// Spells out the exact terminal command that mints the token, so the user is
// never left guessing what to paste (registered as a label-only parameter).
static const char TOKEN_HELP_HTML[] =
    "<div style='text-align:left;background:#232019;border:1px solid #332f28;"
    "border-radius:10px;padding:12px 14px;margin:14px 0 4px'>"
    "<p style='margin:0 0 8px;color:#bdb7ad'>To get your Claude token, run "
    "this in a terminal on your computer:</p>"
    "<code style='display:block;background:#1A1815;border:1px solid #3a352c;"
    "border-radius:6px;padding:9px 11px;color:#DA7756;font-size:1.05em'>"
    "claude setup-token</code>"
    "<p style='margin:8px 0 0;color:#bdb7ad'>Copy what it prints, then paste "
    "it into the field below.</p>"
    "</div>";

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
               "Your codeMeter is restarting and will start "
               "showing your usage.");
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

// --- Soft-AP channel selection ---------------------------------------------
// ESP32's soft-AP "auto" channel is not a real clear-channel scan — it just
// lands on channel 1. In a crowded 2.4 GHz environment (neighbouring routers,
// plus the USB-3 and monitor noise around a desk) that leaves the onboarding
// AP hard to see and slow to join. So scan the band first and pick the
// quietest of the three non-overlapping channels (1 / 6 / 11).
//
// 2.4 GHz channels sit 5 MHz apart but are ~22 MHz wide, so a neighbour bleeds
// onto channels up to 4 away. Each nearby AP is scored against every candidate
// with a falloff by channel distance, weighted by signal strength so a strong
// neighbour counts for more than a faint one; the lowest total wins.
//
// The pick is made once per portal session, not hopped live: a phone that is
// mid-join must not have the AP move channels underneath it. Re-running this
// each time the onboarding loop re-opens the portal is enough to adapt.
static int pick_clear_channel() {
  const int candidates[3] = {1, 6, 11};
  // Overlap weight indexed by |ap_channel - candidate|, 0..4; 0 beyond that.
  static const float overlap[5] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
  float score[3] = {0.0f, 0.0f, 0.0f};

  WiFi.mode(WIFI_STA);                       // scan needs the station radio
  int found = WiFi.scanNetworks();
  for (int i = 0; i < found; i++) {
    int ch = WiFi.channel(i);
    float strength = WiFi.RSSI(i) + 100.0f;  // -100 dBm -> 0, -30 dBm -> 70
    if (strength < 0.0f) strength = 0.0f;
    for (int c = 0; c < 3; c++) {
      int dist = abs(ch - candidates[c]);
      if (dist <= 4) score[c] += strength * overlap[dist];
    }
  }
  WiFi.scanDelete();

  int best = 1;                              // ties / empty scan -> channel 6
  for (int c = 0; c < 3; c++) {
    if (score[c] < score[best]) best = c;
  }
  Serial.printf("[portal] channel scan: %d AP(s)  ch1=%.0f ch6=%.0f ch11=%.0f"
                "  -> ch%d\n",
                found, score[0], score[1], score[2], candidates[best]);
  return candidates[best];
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
  wifiManager.setTitle("codeMeter");
  wifiManager.setClass("invert");                  // dark theme
  wifiManager.setCustomHeadElement(PORTAL_CSS);    // device-matched styling
  wifiManager.setCustomMenuHTML(MENU_HTML);        // first-run instructions
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setMenu(wm_menu);

  // Claude-token field — rendered on the WiFi page (_paramsInWifi is on by
  // default), so the user enters WiFi and the token in one step (ADR 010).
  // A label-only parameter above it carries the "run claude setup-token"
  // instructions; parameters render in registration order.
  new (&tokenHelp) WiFiManagerParameter(TOKEN_HELP_HTML);
  wifiManager.addParameter(&tokenHelp);

  new (&tokenField) WiFiManagerParameter(
      PARAM_ID_TOKEN, "Claude token", "", 256,
      "placeholder=\"paste your claude setup-token output\"");
  wifiManager.addParameter(&tokenField);

  WiFi.onEvent(WiFiEvent);

  while (!(secrets_is_configured() && secrets_has_token())) {
    wifiManager.setWiFiAPChannel(pick_clear_channel());   // quietest of 1/6/11
    wifiManager.startConfigPortal(ap_ssid().c_str());
  }

  Serial.println("[portal] paired (WiFi + token) -> restarting");
  delay(500);
  ESP.restart();
}
