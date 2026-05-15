// parse_headers_test.cpp — host unit tests for the rate-limit header parser.

#include "test_util.h"
#include "parse_headers.h"

static void test_parse_utilization() {
  uint8_t p = 255;

  // Integer percent form.
  CHECK(parse_utilization("42", &p));   CHECK_EQ_INT(p, 42);
  CHECK(parse_utilization("0", &p));    CHECK_EQ_INT(p, 0);
  CHECK(parse_utilization("100", &p));  CHECK_EQ_INT(p, 100);
  CHECK(parse_utilization("1", &p));    CHECK_EQ_INT(p, 1);   // not 100%

  // Decimal fraction form (<= 1.0 with a dot -> x100).
  CHECK(parse_utilization("0.42", &p));  CHECK_EQ_INT(p, 42);
  CHECK(parse_utilization("0.5", &p));   CHECK_EQ_INT(p, 50);
  CHECK(parse_utilization("1.0", &p));   CHECK_EQ_INT(p, 100);
  CHECK(parse_utilization("0.999", &p)); CHECK_EQ_INT(p, 100); // rounds up

  // Decimal percent form (> 1.0 with a dot -> already percent).
  CHECK(parse_utilization("85.5", &p));  CHECK_EQ_INT(p, 86);  // rounds up
  CHECK(parse_utilization("12.3", &p));  CHECK_EQ_INT(p, 12);

  // Clamping.
  CHECK(parse_utilization("150", &p));   CHECK_EQ_INT(p, 100);

  // Invalid inputs leave p untouched and return false.
  p = 77;
  CHECK(!parse_utilization(nullptr, &p));  CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("", &p));       CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("abc", &p));    CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("-5", &p));     CHECK_EQ_INT(p, 77);
}

static void test_parse_reset() {
  // Relative seconds (below the epoch threshold).
  CHECK_EQ_INT(parse_reset("3600", 0), 3600);
  CHECK_EQ_INT(parse_reset("0", 0), 0);
  CHECK_EQ_INT(parse_reset("604800", 0), 604800);  // one week, still relative

  // Absolute Unix timestamps (>= threshold) resolve against now_unix.
  CHECK_EQ_INT(parse_reset("2000000000", 1900000000UL), 100000000UL);
  CHECK_EQ_INT(parse_reset("1500000000", 2000000000UL), 0);  // already past
  CHECK_EQ_INT(parse_reset("2000000000", 0), 0);             // no clock

  // Invalid inputs -> 0.
  CHECK_EQ_INT(parse_reset(nullptr, 0), 0);
  CHECK_EQ_INT(parse_reset("", 0), 0);
  CHECK_EQ_INT(parse_reset("abc", 0), 0);
  CHECK_EQ_INT(parse_reset("-100", 0), 0);
}

static void test_parse_anthropic_headers() {
  // All four headers valid -> status OK, fields populated.
  UsageData d = parse_anthropic_headers("42", "18", "8040", "370800", 12345);
  CHECK_EQ_INT(d.status, UsageData::OK);
  CHECK_EQ_INT(d.session_pct, 42);
  CHECK_EQ_INT(d.weekly_pct, 18);
  CHECK_EQ_INT(d.session_reset_s, 8040);
  CHECK_EQ_INT(d.weekly_reset_s, 370800);
  CHECK_EQ_INT(d.last_poll_unix, 12345);
  CHECK(!d.stale);

  // A missing utilization header -> status UNKNOWN.
  UsageData m1 = parse_anthropic_headers(nullptr, "18", "8040", "370800", 0);
  CHECK_EQ_INT(m1.status, UsageData::UNKNOWN);

  UsageData m2 = parse_anthropic_headers("42", "", "8040", "370800", 0);
  CHECK_EQ_INT(m2.status, UsageData::UNKNOWN);

  // Bad reset headers do not by themselves fail the parse (status stays OK).
  UsageData m3 = parse_anthropic_headers("42", "18", "junk", "", 0);
  CHECK_EQ_INT(m3.status, UsageData::OK);
  CHECK_EQ_INT(m3.session_reset_s, 0);
  CHECK_EQ_INT(m3.weekly_reset_s, 0);
}

int main() {
  test_parse_utilization();
  test_parse_reset();
  test_parse_anthropic_headers();
  TEST_SUMMARY("parse_headers");
}
