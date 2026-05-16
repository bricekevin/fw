// refresh_policy_test.cpp — host unit tests for the OAuth refresh-policy logic.

#include "test_util.h"
#include "refresh_policy.h"

// A representative "now" — Unix epoch seconds, well past the 1.6e9 sanity gate.
static const uint32_t NOW = 1778900000u;

static void test_should_refresh_timing() {
  const uint32_t margin = 1800;  // 30-minute head-start

  // Token expires far in the future — no refresh.
  CHECK(!should_refresh(NOW + 7200, NOW, margin));

  // Just outside the margin — still no refresh.
  CHECK(!should_refresh(NOW + margin + 1, NOW, margin));

  // Exactly at the margin boundary — refresh (>= is inclusive).
  CHECK(should_refresh(NOW + margin, NOW, margin));

  // Inside the margin — refresh.
  CHECK(should_refresh(NOW + 60, NOW, margin));

  // Token expires exactly now — refresh.
  CHECK(should_refresh(NOW, NOW, margin));

  // Token already expired — refresh.
  CHECK(should_refresh(NOW - 3600, NOW, margin));
}

static void test_should_refresh_clock_not_synced() {
  // now == 0 means NTP has not synced — never refresh, whatever expires_at is.
  CHECK(!should_refresh(NOW + 7200, 0, 1800));
  CHECK(!should_refresh(0, 0, 1800));
  CHECK(!should_refresh(NOW - 3600, 0, 1800));
}

static void test_should_refresh_unknown_expiry() {
  // expires_at == 0 (expiry unknown) with a valid clock — treat as expired so
  // a refresh is attempted rather than the token silently going stale.
  CHECK(should_refresh(0, NOW, 1800));
  CHECK(should_refresh(0, NOW, 0));
}

static void test_should_refresh_zero_margin() {
  // With no margin, refresh only once the token has actually expired.
  CHECK(!should_refresh(NOW + 1, NOW, 0));
  CHECK(should_refresh(NOW, NOW, 0));
  CHECK(should_refresh(NOW - 1, NOW, 0));
}

static void test_retry_delay_backoff() {
  // Exponential: base << fails, capped.
  CHECK_EQ_INT(refresh_retry_delay_s(0, 60, 900), 60);
  CHECK_EQ_INT(refresh_retry_delay_s(1, 60, 900), 120);
  CHECK_EQ_INT(refresh_retry_delay_s(2, 60, 900), 240);
  CHECK_EQ_INT(refresh_retry_delay_s(3, 60, 900), 480);
  CHECK_EQ_INT(refresh_retry_delay_s(4, 60, 900), 900);  // 960 capped
  CHECK_EQ_INT(refresh_retry_delay_s(5, 60, 900), 900);
}

static void test_retry_delay_saturation() {
  // A long failure streak must stay capped and never overflow the shift.
  CHECK_EQ_INT(refresh_retry_delay_s(20, 60, 900), 900);
  CHECK_EQ_INT(refresh_retry_delay_s(255, 60, 900), 900);
  // A cap below base_s still clamps to the cap.
  CHECK_EQ_INT(refresh_retry_delay_s(0, 600, 300), 300);
}

int main() {
  test_should_refresh_timing();
  test_should_refresh_clock_not_synced();
  test_should_refresh_unknown_expiry();
  test_should_refresh_zero_margin();
  test_retry_delay_backoff();
  test_retry_delay_saturation();
  TEST_SUMMARY("refresh_policy");
}
