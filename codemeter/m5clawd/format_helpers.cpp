// format_helpers.cpp — see format_helpers.h for the format contract.

#include "format_helpers.h"

#include <cstdio>

namespace {
const uint32_t SECS_PER_MIN  = 60;
const uint32_t SECS_PER_HOUR = 3600;
const uint32_t SECS_PER_DAY  = 86400;
}  // namespace

std::string format_reset_countdown(uint32_t seconds) {
  char buf[16];
  if (seconds < SECS_PER_MIN) {
    return "<1m";
  }
  if (seconds < SECS_PER_DAY) {
    uint32_t h = seconds / SECS_PER_HOUR;
    uint32_t m = (seconds % SECS_PER_HOUR) / SECS_PER_MIN;
    snprintf(buf, sizeof(buf), "%uh %um",
             static_cast<unsigned>(h), static_cast<unsigned>(m));
    return buf;
  }
  uint32_t d = seconds / SECS_PER_DAY;
  uint32_t h = (seconds % SECS_PER_DAY) / SECS_PER_HOUR;
  snprintf(buf, sizeof(buf), "%ud %uh",
           static_cast<unsigned>(d), static_cast<unsigned>(h));
  return buf;
}

std::string format_relative_time(uint32_t seconds_ago) {
  char buf[16];
  if (seconds_ago < 5) {
    return "now";
  }
  if (seconds_ago < SECS_PER_MIN) {
    snprintf(buf, sizeof(buf), "%us ago", static_cast<unsigned>(seconds_ago));
  } else if (seconds_ago < SECS_PER_HOUR) {
    snprintf(buf, sizeof(buf), "%um ago",
             static_cast<unsigned>(seconds_ago / SECS_PER_MIN));
  } else if (seconds_ago < SECS_PER_DAY) {
    snprintf(buf, sizeof(buf), "%uh ago",
             static_cast<unsigned>(seconds_ago / SECS_PER_HOUR));
  } else {
    snprintf(buf, sizeof(buf), "%ud ago",
             static_cast<unsigned>(seconds_ago / SECS_PER_DAY));
  }
  return buf;
}
