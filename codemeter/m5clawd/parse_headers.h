// parse_headers.h — Anthropic rate-limit header parsing (pure, host-testable).
//
// Turns the four `anthropic-ratelimit-unified-*` response-header values into
// the utilization + reset fields of a UsageData. Arduino-header-free so the
// host unit tests can exercise it; the poller passes header values straight
// from HTTPClient::header(...).c_str().
//
// HEADER FORMATS — confirmed 2026-05-15 against the Clawdmeter reference
// daemon (Clawdmeter/daemon/claude_usage_daemon.py), which polls the same
// live API. The four headers (note "7d", not "week"):
//
//   anthropic-ratelimit-unified-5h-utilization
//   anthropic-ratelimit-unified-5h-reset
//   anthropic-ratelimit-unified-7d-utilization
//   anthropic-ratelimit-unified-7d-reset
//
//   *-utilization : a fraction in [0.0, 1.0]. The daemon computes the percent
//                   as round(value * 100) unconditionally — so "0.42" is 42%.
//                   parse_utilization() does the same and clamps to 0..100.
//   *-reset       : an absolute Unix timestamp (epoch seconds). Seconds-until-
//                   reset is value - now_unix, so the device MUST know the
//                   current time (NTP) for the reset fields to be meaningful;
//                   with now_unix == 0 a reset resolves to 0. parse_reset()
//                   also still accepts an already-relative small value as a
//                   defensive fallback (see UNIX_EPOCH_THRESHOLD).
//
// A missing (nullptr / empty) or unparseable utilization header marks that
// window invalid; parse_anthropic_headers() then returns status UNKNOWN.

#pragma once

#include <stdint.h>
#include "usage_data.h"

// Values at or above this are treated as absolute Unix timestamps; smaller
// values are treated as an already-relative second count (defensive fallback —
// the observed format is always an absolute timestamp).
static const uint32_t UNIX_EPOCH_THRESHOLD = 1000000000UL;  // 2001-09-09

// Parse a utilization header value (a 0.0..1.0 fraction) into a 0..100
// percentage. Returns true and writes *out_pct on success; returns false if
// `s` is null/empty/unparseable/negative (leaves *out_pct untouched).
bool parse_utilization(const char *s, uint8_t *out_pct);

// Parse a reset header value into seconds-until-reset.
// The value is an absolute Unix timestamp; `now_unix` is the current epoch
// (pass 0 if the device has no clock — the reset then resolves to 0). A reset
// already in the past, or any null/empty/unparseable value, yields 0.
uint32_t parse_reset(const char *s, uint32_t now_unix);

// Parse all four headers into a UsageData. session_pct/weekly_pct and the two
// reset fields are populated; last_poll_unix is set to now_unix; stale is
// false. status is OK when both utilization headers parsed, UNKNOWN otherwise.
//   util_5h / reset_5h   <- the -5h- headers
//   util_7d / reset_7d   <- the -7d- headers (UsageData calls this "weekly")
UsageData parse_anthropic_headers(const char *util_5h,
                                  const char *util_7d,
                                  const char *reset_5h,
                                  const char *reset_7d,
                                  uint32_t now_unix);
