// state_machine.h — poll-loop state machine (pure, host-testable).
//
// Owns two concerns, kept out of the network/UI code so they can be unit
// tested on the host:
//   1. mapping a poll outcome to the UsageData::Status the UI should show;
//   2. the exponential-backoff schedule for the next poll.
//
// Arduino-header-free. The poller classifies each attempt into a PollOutcome,
// calls sm_advance(), then asks sm_next_delay_s() when to poll again.

#pragma once

#include <stdint.h>
#include "usage_data.h"

// How one poll attempt turned out, as classified by the poller.
enum PollOutcome {
  POLL_OK,              // HTTP 200, rate-limit headers parsed
  POLL_WIFI_DOWN,       // not connected to WiFi — no attempt made
  POLL_API_UNREACHABLE, // WiFi up, but TLS/HTTP failed or a 5xx came back
  POLL_AUTH_FAIL,       // HTTP 401 — the API key was rejected
  POLL_RATE_LIMITED,    // HTTP 429 / rate_limit_error
  POLL_NO_KEY,          // no API key configured (defensive — unreachable in
                        // normal station mode, where a key always exists)
};

// Poll-loop state. Zero-initialize for a fresh boot (status -> UNKNOWN,
// consecutive_fails -> 0).
struct PollState {
  uint8_t           consecutive_fails;  // 0 right after a POLL_OK
  UsageData::Status status;             // current status for the UI
};

// Advance the state machine by one poll outcome, updating *st in place.
// POLL_OK clears the failure count; any other outcome increments it (the
// counter saturates rather than overflowing).
void sm_advance(PollState *st, PollOutcome outcome);

// Seconds to wait before the next poll: base_s on a healthy loop, then
// exponential backoff (base_s << consecutive_fails) capped at cap_s.
// With base 60 / cap 300 the schedule is 60, 120, 240, 300, 300, ...
uint32_t sm_next_delay_s(const PollState *st, uint32_t base_s, uint32_t cap_s);

// True when the values on screen are last-known-good rather than fresh
// (i.e. at least one poll has failed since the last success).
bool sm_data_is_stale(const PollState *st);
