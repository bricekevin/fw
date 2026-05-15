// m5clawd.ino — M5Clawd firmware entry point.
//
// Standalone WiFi Claude-usage display for M5Stack Core Basic (ESP32).
// Phase 1: captive-portal onboarding, WiFi station mode, diagnostic screens.
// The Anthropic poller and the Usage screen arrive in Phase 2.
//
// Copy-and-strip origin (ADR 006): the WiFiManager onboarding pattern is
// inherited from the Crypto_Coin_TickerUS_Stock reference; all crypto/stock/
// chart/sensor/SD code has been stripped.
//
// The sketch is split into .ino tabs (Arduino concatenates them):
//   m5clawd.ino       — globals, setup(), loop(), button handling
//   wifi_portal.ino   — WiFiManager captive-portal glue
//   secrets_store.ino — NVS persistence of the Anthropic API key
//   ui.ino            — M5.Lcd screen drawing
// Shared constants and cross-tab prototypes live in config.h.

#include <M5Stack.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>   // unused in Phase 1; kept for the Phase 2 poller
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
  ui_show_splash();
  delay(600);
  ui_show_connecting();

  if (!station_connect()) {
    Serial.println("[wifi] timed out, retrying once");
    WiFi.disconnect();
    if (!station_connect()) {
      Serial.println("[wifi] connect failed");
      ui_show_wifi_error();
      return;                              // loop() still polls buttons
    }
  }

  Serial.printf("[wifi] connected, IP %s\n",
                WiFi.localIP().toString().c_str());
  currentScreen = SCREEN_STATUS;
  ui_show_status();
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

void loop() {
  M5.update();
  buttons_poll();

  // Refresh the live status fields periodically.
  static unsigned long lastStatusRefresh = 0;
  if (currentScreen == SCREEN_STATUS && !resetConfirmShown &&
      millis() - lastStatusRefresh >= 3000) {
    lastStatusRefresh = millis();
    ui_show_status();
  }
  delay(20);
}
