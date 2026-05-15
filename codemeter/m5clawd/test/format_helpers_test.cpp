// format_helpers_test.cpp — host unit tests for the string formatters.

#include "test_util.h"
#include "format_helpers.h"

static void test_format_reset_countdown() {
  CHECK_EQ_STR(format_reset_countdown(0), "<1m");
  CHECK_EQ_STR(format_reset_countdown(59), "<1m");
  CHECK_EQ_STR(format_reset_countdown(60), "0h 1m");
  CHECK_EQ_STR(format_reset_countdown(3600), "1h 0m");
  CHECK_EQ_STR(format_reset_countdown(8040), "2h 14m");   // 2h14m
  CHECK_EQ_STR(format_reset_countdown(86399), "23h 59m");
  CHECK_EQ_STR(format_reset_countdown(86400), "1d 0h");
  CHECK_EQ_STR(format_reset_countdown(90000), "1d 1h");   // 1d1h
  CHECK_EQ_STR(format_reset_countdown(370800), "4d 7h");  // 4d7h
}

static void test_format_relative_time() {
  CHECK_EQ_STR(format_relative_time(0), "now");
  CHECK_EQ_STR(format_relative_time(4), "now");
  CHECK_EQ_STR(format_relative_time(5), "5s ago");
  CHECK_EQ_STR(format_relative_time(42), "42s ago");
  CHECK_EQ_STR(format_relative_time(59), "59s ago");
  CHECK_EQ_STR(format_relative_time(60), "1m ago");
  CHECK_EQ_STR(format_relative_time(180), "3m ago");
  CHECK_EQ_STR(format_relative_time(3599), "59m ago");
  CHECK_EQ_STR(format_relative_time(3600), "1h ago");
  CHECK_EQ_STR(format_relative_time(7200), "2h ago");
  CHECK_EQ_STR(format_relative_time(86399), "23h ago");
  CHECK_EQ_STR(format_relative_time(86400), "1d ago");
  CHECK_EQ_STR(format_relative_time(172800), "2d ago");
}

int main() {
  test_format_reset_countdown();
  test_format_relative_time();
  TEST_SUMMARY("format_helpers");
}
