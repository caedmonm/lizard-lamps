#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_build_dir=$(mktemp -d "${TMPDIR:-/tmp}/lizard-tests.XXXXXX")
trap 'rm -rf "$test_build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -pedantic tests/control_logic_test.cpp -o "$test_build_dir/control"
"$test_build_dir/control"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/web_test.cpp -o "$test_build_dir/web"
"$test_build_dir/web" "$test_build_dir"
