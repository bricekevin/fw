// parse_headers_test.cpp — host unit tests for the rate-limit header parser.

#include "test_util.h"
#include "parse_headers.h"

static void test_parse_utilization() {
  uint8_t p = 255;

  // The header value is a 0.0..1.0 fraction; percent is round(value * 100).
  CHECK(parse_utilization("0", &p));     CHECK_EQ_INT(p, 0);
  CHECK(parse_utilization("0.42", &p));  CHECK_EQ_INT(p, 42);
  CHECK(parse_utilization("0.5", &p));   CHECK_EQ_INT(p, 50);
  CHECK(parse_utilization("1", &p));     CHECK_EQ_INT(p, 100);  // 1.0 -> 100%
  CHECK(parse_utilization("1.0", &p));   CHECK_EQ_INT(p, 100);
  CHECK(parse_utilization("0.123", &p)); CHECK_EQ_INT(p, 12);   // rounds down
  CHECK(parse_utilization("0.855", &p)); CHECK_EQ_INT(p, 86);   // rounds up
  CHECK(parse_utilization("0.999", &p)); CHECK_EQ_INT(p, 100);  // rounds up
  CHECK(parse_utilization("0.001", &p)); CHECK_EQ_INT(p, 0);    // rounds down

  // Clamping (a value above 1.0 should not occur, but must be safe).
  CHECK(parse_utilization("1.5", &p));   CHECK_EQ_INT(p, 100);

  // Invalid inputs leave p untouched and return false.
  p = 77;
  CHECK(!parse_utilization(nullptr, &p));  CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("", &p));       CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("abc", &p));    CHECK_EQ_INT(p, 77);
  CHECK(!parse_utilization("-0.1", &p));   CHECK_EQ_INT(p, 77);
}

static void test_parse_reset() {
  // Absolute Unix timestamps (>= threshold) resolve against now_unix.
  CHECK_EQ_INT(parse_reset("2000000000", 1900000000UL), 100000000UL);
  CHECK_EQ_INT(parse_reset("1747008040", 1747000000UL), 8040);
  CHECK_EQ_INT(parse_reset("1500000000", 2000000000UL), 0);  // already past
  CHECK_EQ_INT(parse_reset("2000000000", 0), 0);             // no clock

  // Defensive fallback: a small (already-relative) value passes through.
  CHECK_EQ_INT(parse_reset("3600", 0), 3600);
  CHECK_EQ_INT(parse_reset("0", 0), 0);

  // Invalid inputs -> 0.
  CHECK_EQ_INT(parse_reset(nullptr, 0), 0);
  CHECK_EQ_INT(parse_reset("", 0), 0);
  CHECK_EQ_INT(parse_reset("abc", 0), 0);
  CHECK_EQ_INT(parse_reset("-100", 0), 0);
}

static void test_parse_anthropic_headers() {
  // All four headers valid -> status OK, fields populated.
  // Utilization is a fraction; reset is an absolute Unix timestamp.
  const uint32_t now = 1747000000UL;
  UsageData d = parse_anthropic_headers("0.42", "0.18",
                                        "1747008040", "1747370800", now);
  CHECK_EQ_INT(d.status, UsageData::OK);
  CHECK_EQ_INT(d.session_pct, 42);
  CHECK_EQ_INT(d.weekly_pct, 18);
  CHECK_EQ_INT(d.session_reset_s, 8040);     // 1747008040 - now
  CHECK_EQ_INT(d.weekly_reset_s, 370800);    // 1747370800 - now
  CHECK_EQ_INT(d.last_poll_unix, now);
  CHECK(!d.stale);

  // A missing utilization header -> status UNKNOWN.
  UsageData m1 = parse_anthropic_headers(nullptr, "0.18",
                                         "1747008040", "1747370800", now);
  CHECK_EQ_INT(m1.status, UsageData::UNKNOWN);

  UsageData m2 = parse_anthropic_headers("0.42", "",
                                         "1747008040", "1747370800", now);
  CHECK_EQ_INT(m2.status, UsageData::UNKNOWN);

  // Bad reset headers do not by themselves fail the parse (status stays OK).
  UsageData m3 = parse_anthropic_headers("0.42", "0.18", "junk", "", now);
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
