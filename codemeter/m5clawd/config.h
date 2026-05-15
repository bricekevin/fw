// config.h — M5Clawd compile-time configuration.
//
// Shared declarations and constants for the M5Clawd sketch. Included by every
// .ino tab. See docs/2_ARCHITECTURE.md and docs/3_DESIGN_SYSTEM.md.

#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "usage_data.h"      // the shared UsageData struct (pure, host-testable)
#include "state_machine.h"   // PollState / PollOutcome (pure, host-testable)
#include "format_helpers.h"  // countdown / relative-time formatters (pure)

// ---------------------------------------------------------------------------
// Firmware identity
// ---------------------------------------------------------------------------
#define FW_VERSION "0.0.1"

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
// Seconds between Anthropic usage polls. Overridable at runtime via NVS in a
// future phase; this is the default / first-boot value.
#define POLL_INTERVAL_S 60

// Backoff ceiling (seconds) after repeated poll failures.
#define POLL_BACKOFF_MAX_S 300

// HTTP/TLS timeout for a single Anthropic poll (milliseconds).
#define POLL_HTTP_TIMEOUT_MS 15000

// Free-heap warning threshold (bytes). A poll that ends below this logs a
// warning — an early signal of a TLS-buffer leak before the 24 h uptime test.
#define POLL_HEAP_FLOOR 90000

// NTP servers — the device needs wall-clock epoch time because the rate-limit
// reset headers are absolute Unix timestamps (see parse_headers.h).
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

// ---------------------------------------------------------------------------
// WiFi provisioning (captive portal)
// ---------------------------------------------------------------------------
// Soft-AP SSID base. The last 6 hex digits of the MAC are appended at runtime,
// e.g. "M5Clawd-A1B2C3".
#define WIFI_AP_SSID_PREFIX "M5Clawd"

// WiFiManagerParameter id for the Anthropic API key field.
#define PARAM_ID_ANTHROPIC_KEY "anthropic_key"

// ---------------------------------------------------------------------------
// Persistent storage (Preferences / NVS)
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE "m5clawd"
#define NVS_KEY_API_KEY "anthropic_key"
#define NVS_KEY_CONFIGURED "wifi_configured"
#define NVS_KEY_POLL_INTERVAL "poll_interval_s"
// Last-known-good UsageData blob, restored on boot (NVS key <= 15 chars).
#define NVS_KEY_LKG_USAGE "lkg_usage"

// ---------------------------------------------------------------------------
// Anthropic API
// ---------------------------------------------------------------------------
#define ANTHROPIC_HOST "api.anthropic.com"
#define ANTHROPIC_MESSAGES_URL "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_API_VERSION "2023-06-01"
// Cheapest model — we only want the rate-limit response headers.
#define ANTHROPIC_POLL_MODEL "claude-haiku-4-5"

// ---------------------------------------------------------------------------
// UI — colors (RGB565, what M5.Lcd draw calls expect)
// Source hex values and rationale: docs/3_DESIGN_SYSTEM.md
// ---------------------------------------------------------------------------
#define COLOR_PRIMARY   0xDBAA  // Claude orange   #DA7756
#define COLOR_BG        0x18E3  // deep charcoal   #1A1815
#define COLOR_SURFACE   0x21A3  // panel surface   #26221E
#define COLOR_TEXT      0xFF9D  // warm white      #F5F0E8
#define COLOR_TEXT_DIM  0x8C50  // warm gray       #8A8780
#define COLOR_SUCCESS   0x6D2F  // calm green      #6EA47A
#define COLOR_WARNING   0xE52A  // amber           #E0A050
#define COLOR_ERROR     0xC2AA  // muted red       #C25450

// ---------------------------------------------------------------------------
// UI — layout (320x240 panel)
// ---------------------------------------------------------------------------
#define PAD_EDGE     8   // outer margin from panel edge
#define PAD_SECTION  16  // between major UI sections
#define PAD_ITEM     4   // between adjacent labels/values
#define STATUSBAR_H  18  // top status bar height

// ---------------------------------------------------------------------------
// Buttons — long-press threshold for the NVS-reset gesture (button C)
// ---------------------------------------------------------------------------
#define RESET_HOLD_MS 5000

// ---------------------------------------------------------------------------
// Shared data model
//
// The UsageData struct moved to usage_data.h (included above) so the pure-logic
// modules and their host unit tests can use it without Arduino headers.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Cross-tab function prototypes
//
// The sketch is split into .ino tabs that Arduino concatenates into one
// translation unit. Declaring the public functions here lets any tab call
// into another regardless of concatenation order.
// ---------------------------------------------------------------------------

// wifi_portal.ino
String ap_ssid();
String getParam(String name);
void   wifi_portal_begin();

// secrets_store.ino
bool        secrets_is_configured();
String      secrets_get_api_key();
void        secrets_reset();
void        saveParamCallback();
const char *secret_redactor(const String &k);

// poller.ino
void        poller_begin();
PollOutcome poller_poll(const String &api_key, UsageData *out);
uint32_t    poller_time_now();

// usage_store.ino
void usage_store_save(const UsageData &d);
bool usage_store_load(UsageData *out);

// ui.ino
void ui_show_splash();
void ui_show_connecting();
void ui_show_status();
void ui_show_usage(const UsageData &d);
void ui_update_usage(const UsageData &d);
void ui_show_provisioning();
void ui_show_wifi_error();
void ui_show_reset_confirm();
void ui_portal_hint(const char *msg);
void ui_portal_client_connected();
