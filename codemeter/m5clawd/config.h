// config.h — M5Clawd compile-time configuration.
//
// Shared declarations and constants for the M5Clawd sketch. Included by every
// .ino tab. See docs/2_ARCHITECTURE.md and docs/3_DESIGN_SYSTEM.md.

#pragma once

#include <stdint.h>

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
// Shared data model — the only data shape in the firmware.
// See docs/2_ARCHITECTURE.md "Data Model".
// ---------------------------------------------------------------------------
struct UsageData {
  uint8_t  session_pct;      // 0..100 — 5-hour rate-limit utilization
  uint8_t  weekly_pct;       // 0..100 — weekly rate-limit utilization
  uint32_t session_reset_s;  // seconds until the 5h window resets
  uint32_t weekly_reset_s;   // seconds until the weekly window resets
  uint32_t last_poll_unix;   // epoch of the last successful poll
  enum Status {
    UNKNOWN,
    OK,
    WIFI_DOWN,
    API_UNREACHABLE,
    AUTH_FAILED,
    RATE_LIMITED,
  } status;
};
