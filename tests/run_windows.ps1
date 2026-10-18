param([string]$LLVM = 'C:\Program Files\LLVM\bin')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot)
try {
    $compiler = Join-Path $LLVM 'clang++.exe'
    & $compiler --target=x86_64-pc-windows-msvc -std=c++17 -O1 -ffreestanding -fno-builtin -fno-math-errno -fno-exceptions -fno-rtti -fno-stack-protector -mstack-probe-size=1048576 -Wno-unused-value -c tests/regression.cpp -o build/obj/regression.obj
    if ($LASTEXITCODE -ne 0) { throw 'Regression compilation failed' }
    & (Join-Path $LLVM 'lld-link.exe') /nologo /nodefaultlib /entry:WinMainCRTStartup /subsystem:console /machine:x64 /stack:4194304,4194304 /out:build/regression.exe build/obj/regression.obj build/obj/media.obj winlib/kernel32.lib winlib/user32.lib winlib/gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Regression link failed' }
    Push-Location build
    try { & './regression.exe'; $testExit = $LASTEXITCODE; Get-Content regression-results.txt; Copy-Item -LiteralPath 'regression-results.txt' -Destination 'regression-windows-results.txt';if ($testExit -ne 0) { throw 'Regression tests failed' } } finally { Pop-Location }
} finally { Pop-Location }
