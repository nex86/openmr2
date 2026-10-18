#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cat src/g_menu_head.inc src/g_menu_mid.inc src/g_menu_tail.inc > src/g_menu.inc
${CXX:-g++} -std=c++17 -O1 -fno-math-errno -Wno-unused-result tests/regression.cpp src/media.cpp -o build/regression_linux
cd build
./regression_linux
cat regression-results.txt
cp regression-results.txt regression-linux-results.txt
