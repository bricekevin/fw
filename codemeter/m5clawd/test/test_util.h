// test_util.h — minimal assertion helpers for the host unit tests.
//
// No framework: each *_test.cpp is a standalone main() that CHECKs facts and
// ends with TEST_SUMMARY, which returns non-zero if anything failed so
// run.sh can detect a failing suite.

#pragma once

#include <cstdio>
#include <string>

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_checks;                                                           \
    if (!(cond)) {                                                        \
      ++g_fails;                                                          \
      printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                     \
  } while (0)

#define CHECK_EQ_INT(actual, expected)                                    \
  do {                                                                    \
    ++g_checks;                                                           \
    long _a = (long)(actual);                                             \
    long _e = (long)(expected);                                           \
    if (_a != _e) {                                                       \
      ++g_fails;                                                          \
      printf("  FAIL %s:%d  %s == %s  (got %ld, want %ld)\n",             \
             __FILE__, __LINE__, #actual, #expected, _a, _e);             \
    }                                                                     \
  } while (0)

#define CHECK_EQ_STR(actual, expected)                                    \
  do {                                                                    \
    ++g_checks;                                                           \
    std::string _a = (actual);                                            \
    std::string _e = (expected);                                          \
    if (_a != _e) {                                                       \
      ++g_fails;                                                          \
      printf("  FAIL %s:%d  %s == %s  (got \"%s\", want \"%s\")\n",        \
             __FILE__, __LINE__, #actual, #expected,                      \
             _a.c_str(), _e.c_str());                                     \
    }                                                                     \
  } while (0)

#define TEST_SUMMARY(name)                                                \
  do {                                                                    \
    printf("%-16s %3d checks, %d failed\n", (name), g_checks, g_fails);    \
    return g_fails ? 1 : 0;                                               \
  } while (0)
