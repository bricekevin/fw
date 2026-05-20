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
//   poller.ino        — Anthropic HTTPS poll + NTP time
//   ui.ino            — M5.Lcd screen drawing
// Pure-logic modules (parse_headers, format_helpers, state_machine) are plain
// .cpp/.h pairs. Shared constants and cross-tab prototypes live in config.h.

#include <M5Stack.h>
#include <WiFi.h>
#include <esp_wifi.h>           // esp_wifi_get_config — read the stored SSID
#include <WiFiClientSecure.h>   // poller.ino — TLS to api.anthropic.com
#include <HTTPClient.h>         // poller.ino — HTTPS POST
#include <SPIFFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>        // poller.ino — error-body parsing
#include "Free_Fonts.h"
#include "WiFiManager.h"
#include "config.h"
#include "ota.h"

// --- Globals (defined here, visible to every tab) --------------------------
Preferences          preferences;
WiFiManager          wifiManager;
WiFiManagerParameter tokenField;           // captive-portal WiFi page — Claude token field
WiFiManagerParameter tokenHelp;            // label-only — "run claude setup-token" hint

enum Screen { SCREEN_SPLASH, SCREEN_USAGE, SCREEN_STATUS, SCREEN_COUNT };
static Screen currentScreen     = SCREEN_SPLASH;
static bool   backlightOn           = true;
static bool   resetConfirmShown     = false;
static bool   reonboardConfirmShown = false;
static bool   otaConfirmShown       = false;   // hold-B-to-install on Status
static bool   statusHeld            = false;   // button A held -> Status peek
static bool   displayInverted       = false;   // button A tap -> light/dark

// Screen-brightness levels cycled by a button-B tap (M5.Lcd 0..255).
static const uint8_t BRIGHTNESS_LEVELS[] = {40, 110, 180, 255};
static const int     BRIGHTNESS_COUNT    = 4;
static int           brightnessIdx       = 2;   // boot at ~180

// --- Poll-loop state (visible to ui.ino) -----------------------------------
UsageData       g_usage      = {};   // latest usage; status UNKNOWN until first poll
PollState       g_pollState  = {};   // poll-outcome history + backoff schedule
String          g_apiKey;            // OAuth access token, cached at boot / refresh
uint32_t        g_lastPollMs = 0;    // millis() of the last successful poll (0 = none)
static uint32_t nextPollAtMs = 0;    // millis() deadline for the next poll

// --- Credential state (visible to ui.ino) ----------------------------------
bool            g_reauthRequired = false;     // set if the stored token is dead;
                                              // the UI then prompts a re-pair

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;

// --- WiFi station mode ------------------------------------------------------
// Connect using the credentials saved during onboarding. Hardened (Phase 3
// Task 3.3, folding in the Phase 1/2 carry-over bug where the first
// post-onboarding boot needed a manual power-cycle): a short settle delay
// before WiFi.begin() lets the radio recover from the AP/portal teardown, and
// the connect is retried up to three times before giving up.
// Map a wl_status_t to a short human reason for the serial log.
static const char *wifi_status_str(int s) {
  switch (s) {
    case WL_NO_SSID_AVAIL:   return "SSID not visible (out of range / 5 GHz-only)";
    case WL_CONNECT_FAILED:  return "auth rejected (wrong password?)";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED:    return "disconnected / still trying";
    case WL_IDLE_STATUS:     return "idle";
    case WL_CONNECTED:       return "connected";
    default:                 return "unknown";
  }
}

// The home-WiFi SSID saved during onboarding. WiFi.SSID() only reports a name
// once associated, so read the stored station config directly.
static String stored_ssid() {
  wifi_config_t conf;
  if (esp_wifi_get_config(ESP_IF_WIFI_STA, &conf) == ESP_OK) {
    return String(reinterpret_cast<const char *>(conf.sta.ssid));
  }
  return String();
}

// On a total connect failure, scan the band and report whether the saved
// network is even in range — the fastest way to tell "wrong password" from
// "wrong band / out of range" without an LCD.
static void wifi_diagnose_failure() {
  String want = stored_ssid();
  Serial.printf("[wifi] giving up — saved SSID '%s'. Scanning...\n",
                want.c_str());
  // Stop the pending connect first — a scan launched mid-association just
  // fails (returns a negative code) and looks like an empty band.
  WiFi.disconnect(false, false);
  delay(300);
  int n = WiFi.scanNetworks();
  if (n < 0) {                             // WIFI_SCAN_FAILED / _RUNNING
    Serial.printf("[wifi] scan returned %d, retrying once\n", n);
    delay(800);
    n = WiFi.scanNetworks();
  }
  Serial.printf("[wifi] scan found %d network(s):\n", n < 0 ? 0 : n);
  bool seen = false;
  for (int i = 0; i < n; ++i) {
    bool match = want.length() && WiFi.SSID(i) == want;
    seen |= match;
    Serial.printf("[wifi]   %-24s ch%-2d %4ddBm%s\n",
                  WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i),
                  match ? "  <-- saved network" : "");
  }
  WiFi.scanDelete();
  if (want.length() == 0) {
    Serial.println("[wifi] no SSID stored — onboarding may not have saved it");
  } else if (seen) {
    Serial.println("[wifi] saved network IS in range -> likely a wrong password");
  } else {
    Serial.println("[wifi] saved network NOT in range -> off, out of range, or "
                   "a 5 GHz-only band (ESP32 is 2.4 GHz only)");
  }
}

// Boot-time C-hold-to-reset watchdog. station_connect() can block for up to
// ~90 s before loop() (and the full handleButtons) takes over; without this
// the device is unresponsive on the "Connecting to WiFi" screen and the user
// has no way out of a stuck connect. Same hold-then-confirm shape as the
// in-app gesture: RESET_HOLD_MS to arm the red confirm screen, +2 s to commit.
static void boot_reset_watch() {
  static bool armed = false;
  if (M5.BtnC.pressedFor(RESET_HOLD_MS) && !armed) {
    armed = true;
    ui_show_reset_confirm();
  }
  if (armed && M5.BtnC.pressedFor(RESET_HOLD_MS + 2000)) {
    Serial.println("[reset] boot reset gesture -> wiping NVS");
    secrets_reset();
    delay(300);
    ESP.restart();
  }
  if (armed && M5.BtnC.wasReleased()) {
    armed = false;
    ui_show_connecting();                  // released early -> back to status
  }
}

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
      boot_reset_watch();                  // escape hatch before loop() starts
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) return true;
    int st = WiFi.status();
    Serial.printf("[wifi] connect attempt %d/3 timed out — status %d (%s)\n",
                  attempt, st, wifi_status_str(st));
  }
  wifi_diagnose_failure();
  return false;
}

void setup() {
  M5.begin(true, false, true);             // LCD on, SD off, Serial on
  M5.Lcd.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
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
  ota_begin();                             // mark image valid; schedule first check
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
  M5.Lcd.setBrightness(backlightOn ? BRIGHTNESS_LEVELS[brightnessIdx] : 0);
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
  // Easter egg — hold all three buttons together for ~0.8 s.
  if (M5.BtnA.pressedFor(800) && M5.BtnB.pressedFor(800) &&
      M5.BtnC.pressedFor(800)) {
    ui_easter_snake();
    do { M5.update(); delay(20); }              // wait out the held buttons
    while (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnC.isPressed());
    M5.update();                                // consume the release edges
    statusHeld = resetConfirmShown = reonboardConfirmShown = otaConfirmShown = false;
    currentScreen = SCREEN_USAGE;
    show_current_screen();
    return;
  }

  // Button A — a short press toggles light/dark (display inversion); a
  // press-and-hold (0.7 s) peeks the Status screen, releasing returns to Usage.
  if (M5.BtnA.pressedFor(700) && !statusHeld) {
    statusHeld = true;
    currentScreen = SCREEN_STATUS;
    show_current_screen();
  }
  if (M5.BtnA.wasReleased()) {
    if (statusHeld) {
      statusHeld = false;
      currentScreen = SCREEN_USAGE;
      show_current_screen();
    } else {
      displayInverted = !displayInverted;
      M5.Lcd.invertDisplay(displayInverted);
    }
  }

  // Button B — context-aware long-press:
  //   - On the Status screen with an OTA update ready -> install firmware
  //   - Otherwise -> re-run OAuth onboarding (Task 3.4 "change credential")
  // Tap always cycles screen brightness, in either context.
  bool b_install_mode =
      (currentScreen == SCREEN_STATUS && g_ota.phase == OtaState::AVAILABLE);

  if (b_install_mode) {
    if (M5.BtnB.pressedFor(RESET_HOLD_MS) && !otaConfirmShown) {
      otaConfirmShown = true;
      ui_show_ota_install_confirm(g_ota.latestVersion);
    }
    if (otaConfirmShown && M5.BtnB.pressedFor(RESET_HOLD_MS + 2000)) {
      Serial.printf("[ota] user confirmed install of %s\n",
                    g_ota.latestVersion);
      otaConfirmShown = false;
      ota_apply_update_now();
      show_current_screen();                  // back to Status, now downloading
    }
  } else {
    if (M5.BtnB.pressedFor(RESET_HOLD_MS) && !reonboardConfirmShown) {
      reonboardConfirmShown = true;
      ui_show_reonboard_confirm();
    }
    if (reonboardConfirmShown &&
        M5.BtnB.pressedFor(RESET_HOLD_MS + 2000)) {
      Serial.println("[reonboard] clearing OAuth credential, keeping WiFi");
      secrets_clear_oauth();
      delay(300);
      ESP.restart();
    }
  }

  if (M5.BtnB.wasReleased()) {
    if (otaConfirmShown || reonboardConfirmShown) {
      otaConfirmShown = reonboardConfirmShown = false;   // released early
      show_current_screen();
    } else {
      // Tap -> next brightness level, with a brief on-screen readout.
      brightnessIdx = (brightnessIdx + 1) % BRIGHTNESS_COUNT;
      backlightOn = true;
      M5.Lcd.setBrightness(BRIGHTNESS_LEVELS[brightnessIdx]);
      ui_show_brightness(brightnessIdx, BRIGHTNESS_COUNT);
      delay(700);
      show_current_screen();
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

// --- Poll loop --------------------------------------------------------------
// Run one poll, fold the outcome into the state machine, and schedule the next
// poll. The poll blocks for its TLS round-trip (a few seconds); between polls
// loop() runs normally so buttons stay responsive. The device holds a
// long-lived token (ADR 010) and does not refresh it.
static void do_poll() {
  UsageData fresh = {};
  PollOutcome outcome = poller_poll(g_apiKey, &fresh);

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

  // OTA work — non-blocking. Drives in-flight downloads forward in small
  // chunks and runs the scheduled GitHub poll. Reboots into the new image
  // when an install finishes.
  ota_tick();

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
