param([string]$LLVM = 'C:\Program Files\LLVM\bin')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force 'build/obj' | Out-Null
    $parts = 'src/g_menu_head.inc', 'src/g_menu_mid.inc', 'src/g_menu_tail.inc'
    $menu = ($parts | ForEach-Object { [IO.File]::ReadAllText((Join-Path $PSScriptRoot $_)) }) -join ''
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'src/g_menu.inc'), $menu, [Text.UTF8Encoding]::new($false))
    $compiler = Join-Path $LLVM 'clang++.exe'
    $linker = Join-Path $LLVM 'lld-link.exe'
    & $linker /lib /nologo /def:winlib/kernel32.def /machine:x64 /out:winlib/kernel32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Import library generation failed' }
    & $linker /lib /nologo /def:winlib/user32.def /machine:x64 /out:winlib/user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'User32 import library generation failed' }
    & $linker /lib /nologo /def:winlib/gdi32.def /machine:x64 /out:winlib/gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Gdi32 import library generation failed' }
    $flags = @('--target=x86_64-pc-windows-msvc', '-std=c++17', '-O2', '-fno-exceptions', '-fno-rtti', '-fno-math-errno', '-ffreestanding', '-fno-stack-protector', '-mstack-probe-size=1048576', '-fno-builtin', '-msse2', '-Wall', '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable', '-Wno-unused-but-set-variable')
    foreach ($unit in @('core', 'media', 'win32_main')) {
        & $compiler @flags -c "src/$unit.cpp" -o "build/obj/$unit.obj"
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $unit" }
    }
    & $linker /nologo /nodefaultlib /entry:WinMainCRTStartup /subsystem:windows /machine:x64 /stack:4194304,4194304 /out:build/OpenMR2.exe build/obj/core.obj build/obj/media.obj build/obj/win32_main.obj winlib/kernel32.lib winlib/user32.lib winlib/gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Link failed: OpenMR2' }
    Get-Item 'build/OpenMR2.exe' | Select-Object Name, Length
} finally { Pop-Location }
