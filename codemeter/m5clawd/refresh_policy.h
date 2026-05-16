// refresh_policy.h — OAuth token-refresh timing decisions (pure, host-testable).
//
// The device holds a Claude Code OAuth access token that expires (~24 h) and a
// refresh token it uses to mint a new one. This module owns the two pure
// decisions around that — "is it time to refresh?" and "how long to wait after
// a failed refresh?" — kept free of Arduino / network headers so the host unit
// tests (m5clawd/test/run.sh) can exercise them. The actual refresh call lives
// in oauth.ino; the scheduling that consumes these decisions lives in
// m5clawd.ino. See docs/Phase 3 PHASE_IMP.md Task 2.4 and ADR 008.

#pragma once

#include <stdint.h>

// True when the access token should be refreshed now: it expires within
// margin_s seconds, or has already expired. All times are Unix epoch seconds.
//
// Returns false when now == 0 — poller_time_now() reports 0 until NTP has
// synced the wall clock, and refreshing against an unknown clock could trigger
// a refresh storm. An expires_at of 0 (expiry unknown) is treated as "expired"
// once the clock is valid, so a refresh is attempted rather than skipped.
bool should_refresh(uint32_t expires_at, uint32_t now, uint32_t margin_s);

// Seconds to wait before re-attempting a refresh after a transient (network)
// failure: exponential backoff base_s << fails, capped at cap_s. `fails` is the
// count of consecutive failed refresh attempts; callers only consult this when
// fails > 0 (fails == 0 returns base_s for completeness). The shift saturates
// so a long failure streak cannot overflow.
uint32_t refresh_retry_delay_s(uint8_t fails, uint32_t base_s, uint32_t cap_s);
