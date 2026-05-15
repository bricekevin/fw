// parse_headers.h — Anthropic rate-limit header parsing (pure, host-testable).
//
// Turns the four `anthropic-ratelimit-unified-*` response-header values into
// the utilization + reset fields of a UsageData. Arduino-header-free so the
// host unit tests can exercise it; the poller passes header values straight
// from HTTPClient::header(...).c_str().
//
// HEADER FORMAT ASSUMPTIONS (open question in PHASE_PRD — confirm against a
// real 200 response in Task 2.3, then revise this file if they differ):
//
//   *-utilization : a number. A decimal value <= 1.0 is read as a FRACTION
//                   ("0.42" -> 42%); any other number is read as an
//                   already-scaled PERCENT ("42" -> 42%, "85.5" -> 85%).
//                   The result is rounded and clamped to 0..100.
//   *-reset       : a number of seconds. A value >= UNIX_EPOCH_THRESHOLD is
//                   treated as an absolute Unix timestamp and `now_unix` is
//                   subtracted to get seconds-until-reset; a smaller value is
//                   treated as an already-relative seconds count. A reset in
//                   the past, or any unparseable value, yields 0.
//
// A missing (nullptr / empty) or unparseable utilization header marks that
// window invalid; parse_anthropic_headers() then returns status UNKNOWN.

#pragma once

#include <stdint.h>
#include "usage_data.h"

// Values at or above this are assumed to be absolute Unix timestamps rather
// than relative second counts (a relative window tops out at one week).
static const uint32_t UNIX_EPOCH_THRESHOLD = 1000000000UL;  // 2001-09-09

// Parse a single utilization header value into a 0..100 percentage.
// Returns true on success and writes *out_pct; returns false if `s` is
// null/empty/unparseable (leaves *out_pct untouched).
bool parse_utilization(const char *s, uint8_t *out_pct);

// Parse a single reset header value into seconds-until-reset.
// `now_unix` is the current epoch (pass 0 if the device has no clock — an
// absolute-timestamp reset then degrades to 0). Returns 0 on a null/empty/
// unparseable value or a reset already in the past.
uint32_t parse_reset(const char *s, uint32_t now_unix);

// Parse all four headers into a UsageData. session_pct/weekly_pct and the two
// reset fields are populated; last_poll_unix is set to now_unix; stale is
// false. status is OK when both utilization headers parsed, UNKNOWN otherwise.
UsageData parse_anthropic_headers(const char *util_5h,
                                  const char *util_week,
                                  const char *reset_5h,
                                  const char *reset_week,
                                  uint32_t now_unix);
