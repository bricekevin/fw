// m5clawd.ino — M5Clawd firmware entry point.
//
// Standalone WiFi Claude-usage display for M5Stack Core Basic (ESP32).
// Captive-portal onboarding, WiFi station mode, and the Anthropic usage poller.
//
// Copy-and-strip origin (ADR 006): the WiFiManager onboarding pattern is
// inherited from the Crypto_Coin_TickerUS_Stock reference; all crypto/stock/
// chart/sensor/SD code has been stripped.
//
// The sketch is split into .ino tabs (Arduino concatenates them):
//   m5clawd.ino       — globals, setup(), loop(), button handling, poll loop
//   wifi_portal.ino   — WiFiManager onboarding (Stage 1 WiFi + Stage 2 OAuth)
//   secrets_store.ino — NVS persistence of the OAuth credential record
//   oauth.ino         — OAuth PKCE onboarding + access-token refresh
//   poller.ino        — Anthropic HTTPS poll + NTP time
//   ui.ino            — M5.Lcd screen drawing
// Pure-logic modules (parse_headers, format_helpers, state_machine,
// refresh_policy, oauth_pkce) are plain .cpp/.h pairs. Shared constants and
// cross-tab prototypes live in config.h.

#include <M5Stack.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>   // poller.ino — TLS to api.anthropic.com
#include <HTTPClient.h>         // poller.ino — HTTPS POST
#include <SPIFFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>        // poller.ino — error-body parsing
#include "Free_Fonts.h"
#include "WiFiManager.h"
#include "config.h"

// --- Globals (defined here, visible to every tab) --------------------------
Preferences          preferences;
WiFiManager          wifiManager;
WiFiManagerParameter oauthLoginField;      // Stage 2 portal — "Log in" HTML block
WiFiManagerParameter oauthCodeField;       // Stage 2 portal — paste-back code field
String               oauthLoginHtml;      // backing buffer for oauthLoginField
bool                 g_oauth_onboarded = false;  // set on a good Stage 2 exchange

enum Screen { SCREEN_SPLASH, SCREEN_USAGE, SCREEN_STATUS, SCREEN_COUNT };
static Screen currentScreen     = SCREEN_SPLASH;
static bool   backlightOn           = true;
static bool   resetConfirmShown     = false;
static bool   reonboardConfirmShown = false;

// --- Poll-loop state (visible to ui.ino) -----------------------------------
UsageData       g_usage      = {};   // latest usage; status UNKNOWN until first poll
PollState       g_pollState  = {};   // poll-outcome history + backoff schedule
String          g_apiKey;            // OAuth access token, cached at boot / refresh
uint32_t        g_lastPollMs = 0;    // millis() of the last successful poll (0 = none)
static uint32_t nextPollAtMs = 0;    // millis() deadline for the next poll

// --- OAuth credential state (visible to ui.ino) ----------------------------
CredState       g_credState     = CRED_NONE;  // what credential the device holds
uint32_t        g_expiresAt     = 0;          // access-token expiry, epoch seconds
bool            g_reauthRequired = false;     // true once the credential is dead
                                              // (no/invalid refresh token) — polling
                                              // stops; the UI prompts re-onboarding
static uint8_t  g_refreshFails   = 0;         // consecutive transient refresh failures
static uint32_t nextRefreshAtMs  = 0;         // millis() gate after a transient failure

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;

// --- WiFi station mode ------------------------------------------------------
// Connect using the credentials saved during onboarding. Hardened (Phase 3
// Task 3.3, folding in the Phase 1/2 carry-over bug where the first
// post-onboarding boot needed a manual power-cycle): a short settle delay
// before WiFi.begin() lets the radio recover from the AP/portal teardown, and
// the connect is retried up to three times before giving up.
static bool station_connect() {
  WiFi.mode(WIFI_STA);
  for (int attempt = 1; attempt <= 3; ++attempt) {
    if (attempt > 1) {
      WiFi.disconnect();
      delay(300);
    }
    delay(200);                            // settle the radio after AP teardown
    WiFi.begin();                          // use the stored credentials
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      M5.update();
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.printf("[wifi] connect attempt %d/3 timed out\n", attempt);
  }
  return false;
}

void setup() {
  M5.begin(true, false, true);             // LCD on, SD off, Serial on
  M5.Lcd.setBrightness(150);
  Serial.begin(115200);
  Serial.println();
  Serial.printf("M5Clawd v%s booting\n", FW_VERSION);

  SPIFFS.begin(true);

  // Onboarding (ADR 010) — single stage. The soft-AP captive portal collects
  // the home-WiFi credentials and ingests the Claude token (scanned from the
  // host pairing helper's QR via the /cred route). It reboots once it holds
  // both. No on-device OAuth.
  if (!(secrets_is_configured() && secrets_has_token())) {
    Serial.println("[boot] not paired -> onboarding (WiFi + token)");
    ui_show_provisioning();
    wifi_portal_onboard();                 // blocks; reboots when paired
  }

  Serial.println("[boot] paired -> station mode");

  ui_show_splash();
  delay(600);
  ui_show_connecting();

  WiFi.setAutoReconnect(true);             // keep the link up across blips
  bool connected = station_connect();

  g_apiKey = secrets_get_access_token();   // long-lived token, cached for the poller

  // Restore last-known-good usage so the screen shows real numbers (flagged
  // stale) instead of "--" before the first poll of this boot lands.
  UsageData restored;
  if (usage_store_load(&restored)) {
    restored.status = UsageData::UNKNOWN;   // no fresh poll yet this boot
    restored.stale  = true;
    g_usage = restored;
    Serial.printf("[boot] restored usage: session=%u%% weekly=%u%%\n",
                  g_usage.session_pct, g_usage.weekly_pct);
  }
  if (g_reauthRequired) {
    g_usage.status = UsageData::REAUTH_REQUIRED;
    g_usage.stale  = true;
  }

  poller_begin();                          // start NTP (syncs once WiFi is up)
  nextPollAtMs = millis();                 // first poll as soon as loop() runs

  if (connected) {
    Serial.printf("[wifi] connected, IP %s\n",
                  WiFi.localIP().toString().c_str());
    currentScreen = SCREEN_USAGE;          // Usage view; "--" until the first poll
    ui_show_usage(g_usage);
  } else {
    Serial.println("[wifi] connect failed -- loop will keep retrying");
    ui_show_wifi_error();                  // loop() still polls + retries WiFi
  }
}

// --- Buttons ----------------------------------------------------------------
static void backlight_toggle() {
  backlightOn = !backlightOn;
  M5.Lcd.setBrightness(backlightOn ? 150 : 0);
}

static void show_current_screen() {
  // A dead credential locks the display to the re-onboard prompt — polling has
  // stopped, so the Usage/Status views would only show frozen data (Task 4.1).
  if (g_reauthRequired) {
    ui_show_reauth_required();
    return;
  }
  switch (currentScreen) {
    case SCREEN_USAGE:  ui_show_usage(g_usage); break;
    case SCREEN_STATUS: ui_show_status();       break;
    default:            ui_show_splash();       break;
  }
}

static void buttons_poll() {
  // Button A — cycle Splash -> Usage -> Status.
  if (M5.BtnA.wasPressed()) {
    currentScreen = (Screen)((currentScreen + 1) % SCREEN_COUNT);
    show_current_screen();
  }

  // Button B — short press forces an immediate poll; a long-press (5 s, then
  // +2 s to commit — mirroring C) re-runs OAuth onboarding without wiping the
  // WiFi credentials (Task 3.4, the "change credential" gesture).
  if (M5.BtnB.pressedFor(RESET_HOLD_MS) && !reonboardConfirmShown) {
    reonboardConfirmShown = true;
    ui_show_reonboard_confirm();
  }
  if (reonboardConfirmShown && M5.BtnB.pressedFor(RESET_HOLD_MS + 2000)) {
    Serial.println("[reonboard] clearing OAuth credential, keeping WiFi");
    secrets_clear_oauth();
    delay(300);
    ESP.restart();
  }
  if (M5.BtnB.wasReleased()) {
    if (reonboardConfirmShown) {
      reonboardConfirmShown = false;         // released early -> aborted
      show_current_screen();
    } else {
      Serial.println("[poll] manual refresh (button B)");
      nextPollAtMs = millis();
    }
  }

  // Button C — long-press (5 s) arms the NVS-wipe; +2 s more commits it.
  if (M5.BtnC.pressedFor(RESET_HOLD_MS) && !resetConfirmShown) {
    resetConfirmShown = true;
    ui_show_reset_confirm();
  }
  if (resetConfirmShown && M5.BtnC.pressedFor(RESET_HOLD_MS + 2000)) {
    Serial.println("[reset] wiping NVS + WiFi credentials");
    secrets_reset();
    delay(300);
    ESP.restart();
  }
  if (M5.BtnC.wasReleased()) {
    if (resetConfirmShown) {
      resetConfirmShown = false;           // released early -> aborted
      show_current_screen();
    } else {
      backlight_toggle();                  // short press -> backlight
    }
  }
}

// --- Token refresh ----------------------------------------------------------
// True when no transient-failure backoff is currently blocking a refresh.
static bool refresh_gate_open() {
  return (int32_t)(millis() - nextRefreshAtMs) >= 0;
}

// Attempt an OAuth token refresh and fold the result into the credential
// state. Returns true if the device still has a usable credential afterwards
// (refresh succeeded, or failed only transiently so the old access token may
// still work); false if the credential is dead and re-onboarding is required.
static bool do_refresh() {
  // Surface a transient "refreshing" badge — oauth_refresh() blocks for its TLS
  // round-trip. Only the badge slot is touched, so stale card data stays
  // visible meanwhile (Task 4.1).
  if (currentScreen == SCREEN_USAGE) ui_show_refreshing();

  switch (oauth_refresh()) {
    case REFRESH_OK:
      g_apiKey       = secrets_get_access_token();
      g_expiresAt    = secrets_get_expires_at();
      g_refreshFails = 0;
      Serial.printf("[refresh] ok, new access-token expiry=%u\n", g_expiresAt);
      return true;

    case REFRESH_NET_ERROR: {
      // Transient — back off, but keep polling: a proactive refresh runs ahead
      // of expiry, so the current access token is likely still valid.
      if (g_refreshFails < 255) g_refreshFails++;
      uint32_t backoff_s = refresh_retry_delay_s(
          g_refreshFails, REFRESH_BACKOFF_BASE_S, REFRESH_BACKOFF_MAX_S);
      nextRefreshAtMs = millis() + backoff_s * 1000UL;
      Serial.printf("[refresh] transient failure #%u, next attempt in %us\n",
                    g_refreshFails, backoff_s);
      return true;
    }

    case REFRESH_NO_TOKEN:
    case REFRESH_INVALID_GRANT:
    default:
      // Definitive — the refresh token is missing or rejected. Stop polling
      // and prompt the user to re-onboard (ADR 007/008).
      Serial.println("[refresh] credential is dead -> re-onboard required");
      g_reauthRequired = true;
      g_usage.status   = UsageData::REAUTH_REQUIRED;
      g_usage.stale    = true;
      return false;
  }
}

// --- Poll loop --------------------------------------------------------------
// Run one poll — refreshing the access token first if it is near expiry, or
// after a 401 — then fold the outcome into the state machine and schedule the
// next poll. A poll (and any refresh) blocks for its TLS round-trip (a few
// seconds); between polls loop() runs normally so buttons stay responsive.
static void do_poll() {
  // Proactive refresh: top up the access token before it expires so a poll
  // never fails for a stale-but-refreshable credential.
  if (g_credState == CRED_OAUTH && !g_reauthRequired && refresh_gate_open() &&
      should_refresh(g_expiresAt, poller_time_now(), REFRESH_MARGIN_S)) {
    Serial.println("[refresh] access token near expiry");
    do_refresh();
  }

  UsageData fresh = {};
  // Skip the poll itself if the proactive refresh just found the credential
  // dead — treat it as an auth failure so the tail surfaces the re-onboard UI.
  PollOutcome outcome = g_reauthRequired ? POLL_AUTH_FAIL
                                         : poller_poll(g_apiKey, &fresh);

  // Reactive refresh: a 401 means the access token was rejected — refresh once
  // and retry the poll a single time before recording the outcome.
  if (outcome == POLL_AUTH_FAIL && g_credState == CRED_OAUTH &&
      !g_reauthRequired && refresh_gate_open()) {
    Serial.println("[poll] 401 -> token refresh + retry");
    if (do_refresh() && !g_reauthRequired) {
      outcome = poller_poll(g_apiKey, &fresh);
    }
  }

  sm_advance(&g_pollState, outcome);

  // Nudge the WiFi link when it is down — auto-reconnect is on, but a poke
  // each (backoff-spaced) poll speeds recovery. Non-blocking.
  if (outcome == POLL_WIFI_DOWN) {
    Serial.println("[wifi] down -- nudging reconnect");
    WiFi.reconnect();
  }

  if (outcome == POLL_OK) {
    fresh.stale = false;
    g_usage = fresh;
    g_lastPollMs = millis();
    // Persist to NVS only when a percentage actually changed — utilization is
    // stable for long stretches, so this avoids needless NVS write wear.
    static uint8_t saved_session = 255, saved_weekly = 255;
    if (fresh.session_pct != saved_session ||
        fresh.weekly_pct != saved_weekly) {
      usage_store_save(fresh);
      saved_session = fresh.session_pct;
      saved_weekly  = fresh.weekly_pct;
    }
  } else if (g_reauthRequired) {
    // A dead credential overrides the poll status — show the re-onboard
    // prompt and keep the last-known-good numbers on screen.
    g_usage.status = UsageData::REAUTH_REQUIRED;
    g_usage.stale  = true;
  } else {
    // Keep the last-known-good numbers on screen; refresh only status/stale.
    g_usage.status = g_pollState.status;
    g_usage.stale  = sm_data_is_stale(&g_pollState);
  }

  uint32_t delay_s =
      sm_next_delay_s(&g_pollState, POLL_INTERVAL_S, POLL_BACKOFF_MAX_S);
  nextPollAtMs = millis() + delay_s * 1000UL;

  uint32_t heap = ESP.getFreeHeap();
  Serial.printf("[poll] outcome=%d status=%d session=%u%% weekly=%u%% "
                "stale=%d heap=%u next=%us\n",
                outcome, g_usage.status, g_usage.session_pct,
                g_usage.weekly_pct, g_usage.stale, heap, delay_s);
  if (heap < POLL_HEAP_FLOOR) {
    Serial.printf("[warn] low heap: %u bytes (floor %u)\n",
                  heap, (unsigned)POLL_HEAP_FLOOR);
  }

  if (resetConfirmShown || reonboardConfirmShown) return;

  // Credential just died during this poll's refresh — lock to the re-onboard
  // prompt (Task 4.1); polling is now gated off until the user re-onboards.
  if (g_reauthRequired) {
    ui_show_reauth_required();
    return;
  }

  // First good poll after a failed boot-time connect: surface the Usage view.
  if (outcome == POLL_OK && currentScreen == SCREEN_SPLASH) {
    currentScreen = SCREEN_USAGE;
    ui_show_usage(g_usage);
    return;
  }
  // Sustained failure (just crossed >2 in a row): jump to the diagnostic view.
  if (g_pollState.consecutive_fails == 3 && currentScreen == SCREEN_USAGE) {
    currentScreen = SCREEN_STATUS;
    ui_show_status();
    return;
  }
  if (currentScreen == SCREEN_USAGE)       ui_update_usage(g_usage);
  else if (currentScreen == SCREEN_STATUS) ui_show_status();
}

void loop() {
  M5.update();
  buttons_poll();

  // Poll Anthropic when the (backoff-adjusted) interval has elapsed. A device
  // whose credential is dead (g_reauthRequired) stays idle until re-onboarded.
  if (!g_reauthRequired && g_apiKey.length() > 0 && !resetConfirmShown &&
      !reonboardConfirmShown && (int32_t)(millis() - nextPollAtMs) >= 0) {
    do_poll();
  }

  // Refresh the live fields periodically: the Status screen's heap/uptime and
  // the Usage screen's "updated Xs ago" stamp. The Usage refresh is a partial
  // redraw, so it never flickers the cards.
  static unsigned long lastScreenRefresh = 0;
  if (!resetConfirmShown && !reonboardConfirmShown && !g_reauthRequired &&
      millis() - lastScreenRefresh >= 3000) {
    lastScreenRefresh = millis();
    if (currentScreen == SCREEN_STATUS)     ui_show_status();
    else if (currentScreen == SCREEN_USAGE) ui_update_usage(g_usage);
  }
  delay(20);
}
