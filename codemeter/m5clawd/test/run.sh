#!/usr/bin/env bash
# run.sh — host-compile and run the M5Clawd pure-logic unit tests.
#
# The pure-logic modules (parse_headers, format_helpers, state_machine) are
# deliberately free of Arduino headers, so a host g++ can build and run them
# with no ESP32 toolchain. Each *_test.cpp is a standalone main(); this script
# compiles each suite against its module and reports an aggregate result.
#
# Usage:  m5clawd/test/run.sh        (exits non-zero if any suite fails)
#         CXX=clang++ m5clawd/test/run.sh

set -u
cd "$(dirname "$0")"

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -Wall -Wextra -O0 -I.."
BIN_DIR="$(mktemp -d)"
trap 'rm -rf "$BIN_DIR"' EXIT

fails=0

run_suite() {
  local name="$1"; shift
  local bin="$BIN_DIR/$name"
  if ! $CXX $CXXFLAGS "$@" -o "$bin"; then
    echo "BUILD FAILED: $name"
    fails=$((fails + 1))
    return
  fi
  if ! "$bin"; then
    fails=$((fails + 1))
  fi
}

run_suite parse_headers  parse_headers_test.cpp  ../parse_headers.cpp
run_suite format_helpers format_helpers_test.cpp ../format_helpers.cpp
run_suite state_machine  state_machine_test.cpp  ../state_machine.cpp
run_suite refresh_policy refresh_policy_test.cpp ../refresh_policy.cpp
run_suite oauth_pkce     oauth_pkce_test.cpp     ../oauth_pkce.cpp

echo "---"
if [ "$fails" -ne 0 ]; then
  echo "FAILED: $fails suite(s)"
  exit 1
fi
echo "ALL SUITES PASSED"
