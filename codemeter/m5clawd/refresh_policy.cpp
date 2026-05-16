// refresh_policy.cpp — see refresh_policy.h for the behavior contract.

#include "refresh_policy.h"

namespace {
// Saturation ceiling for the failure counter — past this the backoff is
// already pinned at cap_s, and it keeps the left-shift clear of UB. Mirrors
// state_machine.cpp's MAX_FAILS.
const uint8_t MAX_FAILS = 16;
}  // namespace

bool should_refresh(uint32_t expires_at, uint32_t now, uint32_t margin_s) {
  if (now == 0) return false;  // wall clock not NTP-synced yet — cannot decide

  // now + margin_s in 64-bit so the addition cannot wrap, then compare. The
  // token is due when the clock (plus the head-start margin) reaches expiry.
  uint64_t deadline = static_cast<uint64_t>(now) + margin_s;
  return deadline >= expires_at;
}

uint32_t refresh_retry_delay_s(uint8_t fails, uint32_t base_s, uint32_t cap_s) {
  uint8_t shift = fails > MAX_FAILS ? MAX_FAILS : fails;
  uint64_t delay = static_cast<uint64_t>(base_s) << shift;
  return delay > cap_s ? cap_s : static_cast<uint32_t>(delay);
}
