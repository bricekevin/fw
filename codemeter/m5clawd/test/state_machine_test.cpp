// state_machine_test.cpp — host unit tests for the poll-state machine.

#include "test_util.h"
#include "state_machine.h"

static void test_transitions() {
  PollState st{};  // zero-init: fails 0, status UNKNOWN
  CHECK_EQ_INT(st.consecutive_fails, 0);
  CHECK_EQ_INT(st.status, UsageData::UNKNOWN);

  // A success clears failures and sets OK.
  sm_advance(&st, POLL_OK);
  CHECK_EQ_INT(st.consecutive_fails, 0);
  CHECK_EQ_INT(st.status, UsageData::OK);

  // Each failure increments the counter and maps to its status.
  sm_advance(&st, POLL_WIFI_DOWN);
  CHECK_EQ_INT(st.consecutive_fails, 1);
  CHECK_EQ_INT(st.status, UsageData::WIFI_DOWN);

  sm_advance(&st, POLL_API_UNREACHABLE);
  CHECK_EQ_INT(st.consecutive_fails, 2);
  CHECK_EQ_INT(st.status, UsageData::API_UNREACHABLE);

  // A success in the middle of a failure streak resets the count.
  sm_advance(&st, POLL_OK);
  CHECK_EQ_INT(st.consecutive_fails, 0);
  CHECK_EQ_INT(st.status, UsageData::OK);

  sm_advance(&st, POLL_AUTH_FAIL);
  CHECK_EQ_INT(st.status, UsageData::AUTH_FAILED);

  sm_advance(&st, POLL_RATE_LIMITED);
  CHECK_EQ_INT(st.status, UsageData::RATE_LIMITED);

  sm_advance(&st, POLL_NO_KEY);
  CHECK_EQ_INT(st.status, UsageData::UNKNOWN);
}

static void test_backoff_schedule() {
  PollState st{};

  // Healthy loop: base interval.
  st.consecutive_fails = 0;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 60);

  // Exponential backoff, capped.
  st.consecutive_fails = 1;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 120);
  st.consecutive_fails = 2;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 240);
  st.consecutive_fails = 3;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 300);  // 480 capped
  st.consecutive_fails = 4;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 300);
  st.consecutive_fails = 10;
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 300);
}

static void test_stale_flag() {
  PollState st{};
  st.consecutive_fails = 0;
  CHECK(!sm_data_is_stale(&st));
  st.consecutive_fails = 1;
  CHECK(sm_data_is_stale(&st));
  st.consecutive_fails = 5;
  CHECK(sm_data_is_stale(&st));
}

static void test_saturation() {
  // A long failure streak must not overflow the counter or the backoff shift.
  PollState st{};
  for (int i = 0; i < 50; ++i) sm_advance(&st, POLL_API_UNREACHABLE);
  CHECK(st.consecutive_fails > 0);
  CHECK(st.consecutive_fails <= 16);                  // saturated
  CHECK_EQ_INT(sm_next_delay_s(&st, 60, 300), 300);   // still capped, no UB
  CHECK(sm_data_is_stale(&st));
}

static void test_null_safety() {
  // Null pointers must be tolerated, not crash.
  sm_advance(nullptr, POLL_OK);
  CHECK_EQ_INT(sm_next_delay_s(nullptr, 60, 300), 60);
  CHECK(!sm_data_is_stale(nullptr));
}

int main() {
  test_transitions();
  test_backoff_schedule();
  test_stale_flag();
  test_saturation();
  test_null_safety();
  TEST_SUMMARY("state_machine");
}
