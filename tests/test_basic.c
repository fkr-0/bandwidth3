/*
 * Basic sanity checks for bandwidth3.
 * Compile standalone; needs only src/net_stats.c for avg_rate().
 */
#include "../src/bandwidth3.h"
#include <assert.h>
#include <string.h>

/* 1 ───────────────── Version string is what we ship */
static void test_version(void) {
  assert(strcmp(BANDWIDTH3_VERSION, "0.1.3") == 0);
}

/* 2 ───────────────── avg_rate maths */
static void test_avg_rate(void) {
  /* 6000-4000 over 2 s → 1000 B/s */
  assert(avg_rate(6000, 4000, 2) == 1000.0);
  /* Zero delta-time must never segfault or divide-by-zero */
  assert(avg_rate(10, 10, 0) == 0.0);
}

/* 3 ───────────────── Enum uniqueness (connected ≠ disconnected …) */
static void test_enum_distinct(void) {
  assert(IFSTATE_CONNECTED != IFSTATE_DISCONNECTED);
  assert(IFSTATE_CONNECTED != IFSTATE_DISABLED);
  assert(IFSTATE_DISCONNECTED != IFSTATE_DISABLED);
}

/* ─────────────────────────────────────────────────────────────── */
int main(void) {
  test_version();
  test_avg_rate();
  test_enum_distinct();
  return 0; /* any assert() failure aborts non-zero */
}
