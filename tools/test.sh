#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
c++ -std=c++11 -Wall -Wextra -Werror -pedantic -fsanitize=undefined,address -fno-omit-frame-pointer tests/test_core.cpp -o tests/build/core
ASAN_OPTIONS=detect_leaks=0 tests/build/core
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined,address -fno-omit-frame-pointer -DSPRIT_HOST_TEST -DCONFIG_SPRIT_HARDWARE_CONFIRMED=1 tests/test_controller.cpp main/arduino_comm.cpp -o tests/build/controller
ASAN_OPTIONS=detect_leaks=0 tests/build/controller
: "${CJSON_SOURCE:?Set CJSON_SOURCE to the cJSON source directory (managed_components/espressif__cjson/cJSON)}"
cc -c "$CJSON_SOURCE/cJSON.c" -o tests/build/cjson.o
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Itests/fakes -I"$CJSON_SOURCE" tests/test_storage.cpp main/recipes.cpp main/pump_settings.cpp main/bar_settings.cpp tests/build/cjson.o -o tests/build/storage
tests/build/storage
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Itests/fakes tests/test_access.cpp main/access_control.cpp -lcrypto -o tests/build/access
tests/build/access
