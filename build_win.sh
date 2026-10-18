#!/bin/sh
# Cross-compiles OpenMR2.exe (Windows x64) with clang + lld, without any Windows SDK or C runtime.
set -e
cd "$(dirname "$0")"
mkdir -p build/obj
# g_menu.inc is generated from its three parts (edit the parts, not g_menu.inc)
cat src/g_menu_head.inc src/g_menu_mid.inc src/g_menu_tail.inc > src/g_menu.inc
CXX=${CXX:-clang++}
LLD_LINK=${LLD_LINK:-lld-link}
CFLAGS="--target=x86_64-pc-windows-msvc -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-math-errno -ffreestanding \
 -fno-stack-protector -mstack-probe-size=1048576 -fno-builtin -msse2 -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable"
"$LLD_LINK" /lib /nologo /def:winlib/kernel32.def /machine:x64 /out:winlib/kernel32.lib
"$LLD_LINK" /lib /nologo /def:winlib/user32.def /machine:x64 /out:winlib/user32.lib
"$LLD_LINK" /lib /nologo /def:winlib/gdi32.def /machine:x64 /out:winlib/gdi32.lib
"$CXX" $CFLAGS -c src/core.cpp -o build/obj/core.obj
"$CXX" $CFLAGS -c src/win32_main.cpp -o build/obj/win32_main.obj
"$CXX" $CFLAGS -c src/media.cpp -o build/obj/media.obj
"$LLD_LINK" /nologo /nodefaultlib /entry:WinMainCRTStartup /subsystem:windows /machine:x64 /out:build/OpenMR2.exe \
  /stack:4194304,4194304 build/obj/core.obj build/obj/media.obj build/obj/win32_main.obj winlib/kernel32.lib winlib/user32.lib winlib/gdi32.lib
ls -la build/OpenMR2.exe
