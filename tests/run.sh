#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_build_dir=$(mktemp -d "${TMPDIR:-/tmp}/lizard-tests.XXXXXX")
trap 'rm -rf "$test_build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -pedantic tests/control_logic_test.cpp -o "$test_build_dir/control"
"$test_build_dir/control"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/web_test.cpp -o "$test_build_dir/web"
"$test_build_dir/web" "$test_build_dir"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/network_test.cpp -o "$test_build_dir/network"
"$test_build_dir/network"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/config_test.cpp -o "$test_build_dir/config"
"$test_build_dir/config"
if command -v node >/dev/null 2>&1; then node tests/ajax_test.js; fi
python3 - "$test_build_dir" <<'PY'
import json,sys
from pathlib import Path
root=Path(sys.argv[1])
for name in ['status-empty','status','connection','config']:
    json.loads((root/(name+'.json')).read_text())
assert json.loads((root/'status-empty.json').read_text())['temperatures']==[None,None]
assert json.loads((root/'config.json').read_text())['targets']==[[30,24],[20,18]]
print('API JSON responses parse correctly, including missing readings and day/night targets.')
PY
