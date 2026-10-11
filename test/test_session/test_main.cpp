#include <unity.h>
#include "session.h"
void setUp() {}
void tearDown() {}
void retry_does_not_extend() {
  RentalSession s;
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("start", "a", 1, 60, 100));
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("start", "a", 1, 60, 30100));
  TEST_ASSERT_EQUAL_UINT32(30, s.remaining(30100));
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("start", "a", 1, 90, 30100));
}
void stale_update_and_stop_rejected() {
  RentalSession s;
  s.apply("start", "a", 1, 60, 0);
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("update", "a", 3, 120, 10000));
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("update", "a", 2, 200, 20000));
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("stop", "a", 2, 0, 20000));
  TEST_ASSERT_TRUE(s.active);
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("stop", "a", 4, 0, 20000));
  TEST_ASSERT_FALSE(s.active);
}
void expired_retry_does_not_restart() {
  RentalSession s;
  s.apply("start", "a", 1, 1, 0);
  s.tick(1000);
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("start", "a", 1, 1, 2000));
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("update", "a", 2, 60, 2000));
  TEST_ASSERT_EQUAL(RentalSession::OK, s.apply("start", "b", 1, 60, 2000, 1));
  s.tick(62000);
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("start", "a", 1, 1, 63000, 0));
}
void millis_rollover() {
  RentalSession s;
  s.apply("start", "a", 1, 2, 0xffffff00u);
  TEST_ASSERT_EQUAL_UINT32(1, s.remaining(0x000002e8u));
  s.tick(0x000006d0u);
  TEST_ASSERT_FALSE(s.active);
}
void session_conflicts_and_bounds() {
  RentalSession s;
  TEST_ASSERT_EQUAL(RentalSession::INVALID, s.apply("start", "a", 1, 0, 0));
  TEST_ASSERT_EQUAL(RentalSession::INVALID, s.apply("start", "a", 1, 86401, 0));
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("start", "a", 2, 60, 0));
  s.apply("start", "a", 1, 86400, 0);
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("start", "b", 1, 60, 1));
  TEST_ASSERT_EQUAL(RentalSession::CONFLICT, s.apply("stop", "b", 2, 0, 1));
  s.tick(86400000);
  TEST_ASSERT_FALSE(s.active);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(retry_does_not_extend);
  RUN_TEST(stale_update_and_stop_rejected);
  RUN_TEST(expired_retry_does_not_restart);
  RUN_TEST(millis_rollover);
  RUN_TEST(session_conflicts_and_bounds);
  return UNITY_END();
}
