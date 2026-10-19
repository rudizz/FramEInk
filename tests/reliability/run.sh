#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
# Override for another ArduinoJson installation (version used by firmware: 7.4.3).
frameink_json_dir="${ARDUINOJSON_DIR:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}"
frameink_test_dir="$(mktemp -d "${TMPDIR:-/tmp}/frameink-reliability.XXXXXX")"
trap 'rm -rf "$frameink_test_dir"' EXIT
frameink_cxx="${CXX:-clang++}"
flags=(-std=c++17 -Wall -Wextra -Werror -Wno-unknown-pragmas -fsanitize=address,undefined)
"$frameink_cxx" "${flags[@]}" -Itests/calendar/mocks -IFramEink.Core/src -I"$frameink_json_dir" \
    tests/reliability/http_weather_test.cpp -o "$frameink_test_dir/http-weather"
"$frameink_test_dir/http-weather"
"$frameink_cxx" "${flags[@]}" -DARDUINO=10800 -Itests/calendar/mocks -IFramEink.Core/src \
    tests/reliability/calendar_guard_test.cpp FramEink.Core/src/CommonFunctions.cpp -o "$frameink_test_dir/calendar"
"$frameink_test_dir/calendar"
"$frameink_cxx" "${flags[@]}" -Itests/reliability/mocks -Itests/photo_upload/mocks -I"$frameink_json_dir" \
    -Itests/settings/mocks -IFramEink.Core/src tests/reliability/network_cycle_test.cpp \
    FramEink.Core/src/Network.cpp FramEink.Core/src/Calendar/CalendarService.cpp \
    FramEink.Core/src/CommonFunctions.cpp FramEink.Core/src/App/ApplicationController.cpp -o "$frameink_test_dir/cycle"
"$frameink_test_dir/cycle"
"$frameink_cxx" "${flags[@]}" -DFRAMEINK_DIAGNOSTICS_TEST -Itests/diagnostics/mocks -Itests/photo_upload/mocks \
    -IFramEink.Core/src tests/diagnostics/diagnostics_test.cpp FramEink.Core/src/Diagnostics/DiagnosticLog.cpp \
    -o "$frameink_test_dir/diagnostics"
"$frameink_test_dir/diagnostics"
