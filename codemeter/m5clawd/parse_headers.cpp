// parse_headers.cpp — see parse_headers.h for the format contract.

#include "parse_headers.h"

#include <cstdlib>
#include <cstring>
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

// Does the string contain a decimal point? (used by the fraction heuristic)
bool has_dot(const char *s) {
  return s != nullptr && strchr(s, '.') != nullptr;
}

}  // namespace

bool parse_utilization(const char *s, uint8_t *out_pct) {
  double v;
  if (!parse_double(s, &v)) return false;
  if (v < 0.0) return false;

  // A decimal value <= 1.0 is a fraction (0.42 -> 42); otherwise already a
  // percent. The has-dot guard keeps integer "1" meaning 1%, not 100%.
  double pct = (has_dot(s) && v <= 1.0) ? v * 100.0 : v;

  double rounded = std::floor(pct + 0.5);
  if (rounded < 0.0)   rounded = 0.0;
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
    if (now_unix == 0) return 0;                 // no clock: cannot resolve
    if (v <= static_cast<double>(now_unix)) return 0;  // already past
    return static_cast<uint32_t>(v - static_cast<double>(now_unix));
  }
  // Already a relative seconds count.
  return static_cast<uint32_t>(v);
}

UsageData parse_anthropic_headers(const char *util_5h,
                                  const char *util_week,
                                  const char *reset_5h,
                                  const char *reset_week,
                                  uint32_t now_unix) {
  UsageData d{};
  d.last_poll_unix = now_unix;
  d.stale = false;

  bool session_ok = parse_utilization(util_5h, &d.session_pct);
  bool weekly_ok = parse_utilization(util_week, &d.weekly_pct);

  d.session_reset_s = parse_reset(reset_5h, now_unix);
  d.weekly_reset_s = parse_reset(reset_week, now_unix);

  d.status = (session_ok && weekly_ok) ? UsageData::OK : UsageData::UNKNOWN;
  return d;
}
