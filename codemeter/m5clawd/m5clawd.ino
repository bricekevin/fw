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
//   wifi_portal.ino   — WiFiManager captive-portal glue
//   secrets_store.ino — NVS persistence of the Anthropic API key
//   poller.ino        — Anthropic HTTPS poll + NTP time
//   ui.ino            — M5.Lcd screen drawing
// Pure-logic modules (parse_headers, format_helpers, state_machine) are plain
// .cpp/.h pairs. Shared constants and cross-tab prototypes live in config.h.

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
WiFiManagerParameter apiKeyField;          // the single custom portal field

enum Screen { SCREEN_SPLASH, SCREEN_STATUS };
static Screen currentScreen     = SCREEN_SPLASH;
static bool   backlightOn       = true;
static bool   resetConfirmShown = false;

// --- Poll-loop state (visible to ui.ino) -----------------------------------
UsageData       g_usage      = {};   // latest usage; status UNKNOWN until first poll
PollState       g_pollState  = {};   // poll-outcome history + backoff schedule
String          g_apiKey;            // Anthropic API key, cached at boot
static uint32_t nextPollAtMs = 0;    // millis() deadline for the next poll

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;

// --- WiFi station mode ------------------------------------------------------
// Connect using the credentials WiFiManager persisted on first-boot setup.
static bool station_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    M5.update();
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

void setup() {
  M5.begin(true, false, true);             // LCD on, SD off, Serial on
  M5.Lcd.setBrightness(150);
  Serial.begin(115200);
  Serial.println();
  Serial.printf("M5Clawd v%s booting\n", FW_VERSION);

  SPIFFS.begin(true);

  // Boot-mode selection: no stored config -> provisioning, else station.
  if (!secrets_is_configured()) {
    Serial.println("[boot] not configured -> provisioning mode");
    ui_show_provisioning();
    wifi_portal_begin();                   // blocks; restarts on success
  }

  Serial.println("[boot] configured -> station mode");
  g_apiKey = secrets_get_api_key();
  ui_show_splash();
  delay(600);
  ui_show_connecting();

  WiFi.setAutoReconnect(true);             // keep the link up across blips
  bool connected = station_connect();
  if (!connected) {
    Serial.println("[wifi] timed out, retrying once");
    WiFi.disconnect();
    connected = station_connect();
  }

  poller_begin();                          // start NTP (syncs once WiFi is up)
  nextPollAtMs = millis();                 // first poll as soon as loop() runs

  if (connected) {
    Serial.printf("[wifi] connected, IP %s\n",
                  WiFi.localIP().toString().c_str());
    currentScreen = SCREEN_STATUS;
    ui_show_status();
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
  if (currentScreen == SCREEN_SPLASH) ui_show_splash();
  else                                ui_show_status();
}

static void buttons_poll() {
  // Button A — cycle Splash <-> Status.
  if (M5.BtnA.wasPressed()) {
    currentScreen = (currentScreen == SCREEN_SPLASH) ? SCREEN_STATUS
                                                     : SCREEN_SPLASH;
    show_current_screen();
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

// --- Poll loop --------------------------------------------------------------
// Run one poll, fold the outcome into the state machine, schedule the next.
// A poll blocks for its TLS round-trip (a few seconds); between polls loop()
// runs normally so buttons stay responsive.
static void do_poll() {
  UsageData fresh = {};
  PollOutcome outcome = poller_poll(g_apiKey, &fresh);
  sm_advance(&g_pollState, outcome);

  if (outcome == POLL_OK) {
    fresh.stale = false;
    g_usage = fresh;
  } else {
    // Keep the last-known-good numbers on screen; refresh only status/stale.
    g_usage.status = g_pollState.status;
    g_usage.stale  = sm_data_is_stale(&g_pollState);
  }

  uint32_t delay_s =
      sm_next_delay_s(&g_pollState, POLL_INTERVAL_S, POLL_BACKOFF_MAX_S);
  nextPollAtMs = millis() + delay_s * 1000UL;

  Serial.printf("[poll] outcome=%d status=%d session=%u%% weekly=%u%% "
                "stale=%d next=%us\n",
                outcome, g_usage.status, g_usage.session_pct,
                g_usage.weekly_pct, g_usage.stale, delay_s);

  if (currentScreen == SCREEN_STATUS && !resetConfirmShown) ui_show_status();
}

void loop() {
  M5.update();
  buttons_poll();

  // Poll Anthropic when the (backoff-adjusted) interval has elapsed.
  if (g_apiKey.length() > 0 && !resetConfirmShown &&
      (int32_t)(millis() - nextPollAtMs) >= 0) {
    do_poll();
  }

  // Refresh the live status fields periodically.
  static unsigned long lastStatusRefresh = 0;
  if (currentScreen == SCREEN_STATUS && !resetConfirmShown &&
      millis() - lastStatusRefresh >= 3000) {
    lastStatusRefresh = millis();
    ui_show_status();
  }
  delay(20);
}
