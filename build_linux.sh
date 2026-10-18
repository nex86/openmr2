#!/bin/sh
# Linux/X11 test build (needs clang++, X11 headers). Exits with the compiler's status.
set -e
cd "$(dirname "$0")"
mkdir -p build
# g_menu.inc is generated from its three parts (edit the parts, not g_menu.inc)
cat src/g_menu_head.inc src/g_menu_mid.inc src/g_menu_tail.inc > src/g_menu.inc
${CXX:-g++} -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-math-errno -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -Wno-misleading-indentation \
  src/core.cpp src/media.cpp src/x11_main.cpp -o build/openmr2_linux -lX11 -ldl
