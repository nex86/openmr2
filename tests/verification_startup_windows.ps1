$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$run=Join-Path $root 'build/verification-local'
$fixture=Join-Path (Split-Path (Split-Path $root)) 'Megarace 2 German'
New-Item -ItemType Directory -Force (Join-Path $run 'revived_config') | Out-Null
# Read-only hard links avoid copying the CD image. Never alter linked game files.
$manifest=[IO.File]::ReadAllText((Join-Path $root 'src/gog_manifest.h'))
$names=[regex]::Matches($manifest,'{"([^"]+)", {') | ForEach-Object {$_.Groups[1].Value}
foreach($name in $names){
    $target=Join-Path $run $name
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    if(!(Test-Path -LiteralPath $target)){New-Item -ItemType HardLink -Path $target -Target (Join-Path $fixture $name) | Out-Null}
}
$exe=Join-Path $run 'OpenMR2.exe'
Copy-Item -LiteralPath (Join-Path $root 'build/OpenMR2.exe') -Destination $exe
$cfg=Join-Path $run 'revived_config/openmr2_setup.cfg'
[IO.File]::WriteAllText($cfg,"lance_boyle_videos = 0`nmenu_music = 0`nsound_effects = 0`ndebug_console = 0`n")
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class VerificationWindow {
 [DllImport("user32.dll",CharSet=CharSet.Ansi)]public static extern IntPtr FindWindow(string c,string title);
}
'@
$results=@()
foreach($launch in 1..3){
    if($launch -eq 3){[IO.File]::WriteAllText($cfg,[IO.File]::ReadAllText($cfg).Replace('Files_OK = true','Files_OK = false'))}
    $clock=[Diagnostics.Stopwatch]::StartNew();$sawProgress=$false
    $p=Start-Process -FilePath $exe -ArgumentList '--screen menu --size 640x480 --shot verification.bmp' -WindowStyle Hidden -PassThru
    while(!$p.HasExited){
        if([VerificationWindow]::FindWindow('OpenMR2Verification','OpenMR2 - one-time game data verification') -ne [IntPtr]::Zero){$sawProgress=$true}
        if($clock.Elapsed.TotalSeconds -gt 90){$p.Kill();throw 'Verification startup timed out'}
        Start-Sleep -Milliseconds 20
    }
    $p.WaitForExit();$p.Refresh();$text=[IO.File]::ReadAllText($cfg)
    $expectedProgress=$launch -ne 2
    $pass=$p.ExitCode -eq 0 -and $sawProgress -eq $expectedProgress -and $text.Contains('Files_OK = true') -and $text.Contains('verified_language = 2') -and $text.Contains('game_directory = '+$run)
    $results+=[PSCustomObject]@{launch=$launch;local_data=$true;observed_progress_dialog=$sawProgress;seconds=$clock.Elapsed.TotalSeconds;pass=$pass}
    $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build/verification-startup-results.json')
    if(!$pass){throw "Local verification/cache launch $launch failed"}
    "Local launch $launch : PASS (progress=$sawProgress, seconds=$($clock.Elapsed.TotalSeconds.ToString('F2')))"
}
