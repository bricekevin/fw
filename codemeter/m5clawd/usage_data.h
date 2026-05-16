// usage_data.h — M5Clawd shared data model (pure, Arduino-header-free).
//
// UsageData is the single data shape in the firmware: the poller produces it,
// the UI consumes it. This header deliberately includes ONLY <stdint.h> so the
// host unit tests (m5clawd/test/run.sh) can compile the pure-logic modules
// that operate on it without an Arduino runtime.
//
// See docs/2_ARCHITECTURE.md "Data Model".

#pragma once

#include <stdint.h>

struct UsageData {
  uint8_t  session_pct;      // 0..100 — 5-hour rate-limit utilization
  uint8_t  weekly_pct;       // 0..100 — weekly rate-limit utilization
  uint32_t session_reset_s;  // seconds until the 5h window resets
  uint32_t weekly_reset_s;   // seconds until the weekly window resets
  uint32_t last_poll_unix;   // epoch (or millis-derived stamp) of the last good poll
  bool     stale;            // true = last-known-good values, not a fresh poll
  enum Status {
    UNKNOWN,                 // no poll has succeeded yet
    OK,                      // fresh, valid data
    WIFI_DOWN,               // not connected to WiFi
    API_UNREACHABLE,         // WiFi up, but the Anthropic call failed
    AUTH_FAILED,             // the access token was rejected
    RATE_LIMITED,            // Anthropic returned a rate-limit error
    REAUTH_REQUIRED,         // OAuth refresh failed / no refresh token — the
                             // user must re-onboard (ADR 007/008)
  } status;
};
