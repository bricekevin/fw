// state_machine.cpp — see state_machine.h for the behavior contract.

#include "state_machine.h"

namespace {
// Saturation ceiling for the failure counter. Past this the backoff is already
// pinned at cap_s, so there is nothing to gain from counting higher — and it
// keeps the left-shift in sm_next_delay_s() well clear of undefined behavior.
const uint8_t MAX_FAILS = 16;

UsageData::Status status_for(PollOutcome outcome) {
  switch (outcome) {
    case POLL_OK:              return UsageData::OK;
    case POLL_WIFI_DOWN:       return UsageData::WIFI_DOWN;
    case POLL_API_UNREACHABLE: return UsageData::API_UNREACHABLE;
    case POLL_AUTH_FAIL:       return UsageData::AUTH_FAILED;
    case POLL_RATE_LIMITED:    return UsageData::RATE_LIMITED;
    case POLL_NO_KEY:          return UsageData::UNKNOWN;
  }
  return UsageData::UNKNOWN;
}
}  // namespace

void sm_advance(PollState *st, PollOutcome outcome) {
  if (st == nullptr) return;

  if (outcome == POLL_OK) {
    st->consecutive_fails = 0;
  } else if (st->consecutive_fails < MAX_FAILS) {
    st->consecutive_fails++;
  }
  st->status = status_for(outcome);
}

uint32_t sm_next_delay_s(const PollState *st, uint32_t base_s, uint32_t cap_s) {
  if (st == nullptr) return base_s;

  uint8_t shift = st->consecutive_fails;
  if (shift > MAX_FAILS) shift = MAX_FAILS;

  uint64_t delay = static_cast<uint64_t>(base_s) << shift;
  if (delay > cap_s) return cap_s;
  return static_cast<uint32_t>(delay);
}

bool sm_data_is_stale(const PollState *st) {
  return st != nullptr && st->consecutive_fails > 0;
}
