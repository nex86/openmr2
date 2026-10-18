$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$fixtures=(Resolve-Path (Join-Path $root '../..')).Path
$results=@()
foreach($case in @('English','French','German','CLI')) {
    $language=if($case -eq 'CLI'){'English'}else{$case}
    $run=Join-Path $root ('build/console-startup/'+$case)
    New-Item -ItemType Directory -Force (Join-Path $run 'revived_config') | Out-Null
    $exe=Join-Path $run 'OpenMR2.exe'
    Copy-Item -LiteralPath (Join-Path $root 'build/OpenMR2.exe') -Destination $exe
    $game=Join-Path $fixtures ('Megarace 2 '+$language)
    $flag=if($case -eq 'CLI'){0}else{1}
    [IO.File]::WriteAllText((Join-Path $run 'revived_config/openmr2_setup.cfg'),('game_directory = '+$game+"`ndebug_console = $flag`nsound_effects = 0`nrace_music = 0`nmenu_music = 0`nlance_boyle_videos = 0`n"))
    $arguments='--screen menu --size 640x480 --shot console-menu.bmp'
    if($case -eq 'CLI'){$arguments+=' --debug'}
    $p=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit(60000)){$p.Kill();throw "$case console startup timed out"}
    $p.Refresh()
    $log=[IO.File]::ReadAllText((Join-Path $run 'revived_config/openmr2.log'))
    $pass=$p.ExitCode -eq 0 -and ($log.Contains('Debug console enabled') -or ($case -eq 'CLI' -and $log.Contains('Debug console (asset loads'))) -and !$log.Contains('[Status] FPS') -and $log.Contains('[load]') -and $log.Contains('Main menu') -and !$log.Contains('[Zustand]') -and !$log.Contains('Fehler in Rot')
    $results+=[PSCustomObject]@{case=$case;language=$language;saved_console_flag=$flag;pass=$pass}
    $results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $root 'build/console-startup-results.json')
    if(!$pass){throw "$case did not produce the expected English console status"}
    "$case : PASS actual game startup with English console diagnostics"
}
