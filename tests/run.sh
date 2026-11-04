#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_build_dir=$(mktemp -d "${TMPDIR:-/tmp}/lizard-tests.XXXXXX")
trap 'rm -rf "$test_build_dir"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -pedantic tests/control_logic_test.cpp -o "$test_build_dir/control"
"$test_build_dir/control"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -pedantic tests/temperature_regulation_test.cpp -o "$test_build_dir/regulation"
"$test_build_dir/regulation"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/controller_loop_test.cpp -o "$test_build_dir/controller-loop"
"$test_build_dir/controller-loop"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -pedantic tests/history_test.cpp -o "$test_build_dir/history"
"$test_build_dir/history"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/history_storage_test.cpp -o "$test_build_dir/history-storage"
"$test_build_dir/history-storage"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/history_persistence_test.cpp -o "$test_build_dir/history-persistence"
"$test_build_dir/history-persistence"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/web_test.cpp -o "$test_build_dir/web"
"$test_build_dir/web" "$test_build_dir"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/network_test.cpp -o "$test_build_dir/network"
"$test_build_dir/network"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/firmware_test.cpp -o "$test_build_dir/firmware"
"$test_build_dir/firmware"
"${CXX:-c++}" -std=c++17 -Itests/stubs tests/config_test.cpp -o "$test_build_dir/config"
"$test_build_dir/config"
if command -v node >/dev/null 2>&1; then node tests/ajax_test.js; node tests/history_client_test.js; node tests/firmware_client_test.js; fi
python3 - "$test_build_dir" <<'PY'
import json,sys
from pathlib import Path
root=Path(sys.argv[1])
for name in ['status-empty','status','connection','config','history','history-full']:
    json.loads((root/(name+'.json')).read_text())
assert json.loads((root/'status-empty.json').read_text())['temperatures']==[None,None]
assert json.loads((root/'config.json').read_text())['targets']==[[30,24],[20,18]]
history=json.loads((root/'history.json').read_text())
assert len(history['samples'])==2 and history['intervalSeconds']==60
assert history['samples'][0][1:]==[30,24,60,0,30,24,20,18,None,None]
assert history['samples'][1][1:]==[None,46,60,0,30,24,20,18,None,None]
assert history['samples'][0][0]-history['samples'][1][0]==60
full_response=json.loads((root/'history-full.json').read_text())
assert full_response['retentionSeconds']==86400
full=full_response['samples']
assert 1440<=len(full)<=1441 and all(len(s)==11 for s in full)
assert full[0][0]<=86400 and full[-1][0]<60
pages=json.loads((root/'history-pages.json').read_text())
assert all(0<len(page['samples'])<=120 for page in pages)
absolute=lambda page: [[page['nowEpoch']-s[0],*s[1:]] for s in page['samples']]
combined=[s for page in reversed(pages) for s in absolute(page)]
assert combined==absolute(full_response)
assert pages[-1]['nextBefore']==0
assert all(page['nextBefore']==absolute(page)[0][0] for page in pages[:-1])
print('API JSON responses parse correctly, including missing readings and day/night targets.')
PY
