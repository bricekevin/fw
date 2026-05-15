// format_helpers.h — human-readable string formatting (pure, host-testable).
//
// Small, Arduino-header-free formatters for the Usage screen. They return
// std::string (available in the ESP32 Arduino toolchain and on the host), so
// the host unit tests can check them directly. The UI passes the result to
// M5.Lcd.drawString via .c_str().

#pragma once

#include <stdint.h>
#include <string>

// Seconds-until-reset -> a compact countdown.
//   0..59      -> "<1m"
//   60..86399  -> "2h 14m"   (hours + minutes)
//   >= 1 day   -> "4d 7h"    (days + hours)
std::string format_reset_countdown(uint32_t seconds);

// Seconds-since-last-poll -> a compact "ago" stamp for the status bar.
//   0..4       -> "now"
//   5..59      -> "42s ago"
//   60..3599   -> "3m ago"
//   3600..86399-> "5h ago"
//   >= 1 day   -> "2d ago"
std::string format_relative_time(uint32_t seconds_ago);
