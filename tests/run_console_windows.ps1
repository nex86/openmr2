param([string]$LLVM='C:\Program Files\LLVM\bin')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
Push-Location $root
try {
    & (Join-Path $LLVM 'clang++.exe') --target=x86_64-pc-windows-msvc -std=c++17 -O1 -ffreestanding -fno-builtin -fno-math-errno -fno-exceptions -fno-rtti -fno-stack-protector -mstack-probe-size=1048576 -Wno-unused-value -c tests/console_windows.cpp -o build/obj/console_windows.obj
    if($LASTEXITCODE -ne 0){throw 'Console test compilation failed'}
    & (Join-Path $LLVM 'lld-link.exe') /nologo /nodefaultlib /entry:WinMainCRTStartup /subsystem:windows /machine:x64 /stack:4194304,4194304 /out:build/console_windows.exe build/obj/console_windows.obj build/obj/media.obj winlib/kernel32.lib winlib/user32.lib winlib/gdi32.lib
    if($LASTEXITCODE -ne 0){throw 'Console test link failed'}
    $run=Join-Path $root 'build/console-test'
    New-Item -ItemType Directory -Force $run | Out-Null
    $p=Start-Process -FilePath (Join-Path $root 'build/console_windows.exe') -WorkingDirectory $run -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit(30000)){$p.Kill();throw 'Console test timed out'}
    $p.Refresh()
    Copy-Item -LiteralPath (Join-Path $run 'console-results.txt') -Destination 'build/console-windows-results.txt'
    Get-Content 'build/console-windows-results.txt'
    if($p.ExitCode -ne 0){throw 'Console test failed'}
} finally { Pop-Location }
