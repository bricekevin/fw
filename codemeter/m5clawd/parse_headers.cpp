// parse_headers.cpp — see parse_headers.h for the format contract.

#include "parse_headers.h"

#include <cstdlib>
#include <cmath>

namespace {

// Parse `s` as a double. Returns true and writes *out on success; false if
// `s` is null, empty, or has no leading numeric content.
bool parse_double(const char *s, double *out) {
  if (s == nullptr || *s == '\0') return false;
  char *end = nullptr;
  double v = strtod(s, &end);
  if (end == s) return false;        // no digits consumed
  *out = v;
  return true;
}

}  // namespace

bool parse_utilization(const char *s, uint8_t *out_pct) {
  double v;
  if (!parse_double(s, &v)) return false;
  if (v < 0.0) return false;

  // The header value is a 0.0..1.0 fraction; the percentage is value * 100
  // (matches the Clawdmeter daemon). Round half-up and clamp to 0..100.
  double rounded = std::floor(v * 100.0 + 0.5);
  if (rounded > 100.0) rounded = 100.0;

  *out_pct = static_cast<uint8_t>(rounded);
  return true;
}

uint32_t parse_reset(const char *s, uint32_t now_unix) {
  double v;
  if (!parse_double(s, &v)) return 0;
  if (v < 0.0) return 0;

  if (v >= static_cast<double>(UNIX_EPOCH_THRESHOLD)) {
    // Absolute Unix timestamp — convert to seconds remaining.
    if (now_unix == 0) return 0;                       // no clock: cannot resolve
    if (v <= static_cast<double>(now_unix)) return 0;  // already past
    return static_cast<uint32_t>(v - static_cast<double>(now_unix));
  }
  // Defensive fallback: an already-relative seconds count.
  return static_cast<uint32_t>(v);
}

UsageData parse_anthropic_headers(const char *util_5h,
                                  const char *util_7d,
                                  const char *reset_5h,
                                  const char *reset_7d,
                                  uint32_t now_unix) {
  UsageData d{};
  d.last_poll_unix = now_unix;
  d.stale = false;

  bool session_ok = parse_utilization(util_5h, &d.session_pct);
  bool weekly_ok = parse_utilization(util_7d, &d.weekly_pct);

  d.session_reset_s = parse_reset(reset_5h, now_unix);
  d.weekly_reset_s = parse_reset(reset_7d, now_unix);

  d.status = (session_ok && weekly_ok) ? UsageData::OK : UsageData::UNKNOWN;
  return d;
}
