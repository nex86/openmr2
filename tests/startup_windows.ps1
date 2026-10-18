$ErrorActionPreference='Stop'
$sourceRoot=Split-Path $PSScriptRoot
$fixtureRoot=(Resolve-Path (Join-Path $sourceRoot '../..')).Path
$elsewhere=Join-Path $sourceRoot 'build/startup-unrelated-cwd'
New-Item -ItemType Directory -Force $elsewhere | Out-Null
$results=@()
foreach($language in @('English','French','German')) {
    $run=Join-Path $sourceRoot ('build/startup-windows/'+$language)
    New-Item -ItemType Directory -Force (Join-Path $run 'revived_config') | Out-Null
    $exe=Join-Path $run 'OpenMR2.exe'
    Copy-Item -LiteralPath (Join-Path $sourceRoot 'build/OpenMR2.exe') -Destination $exe
    $game=Join-Path $fixtureRoot ('Megarace 2 '+$language)
    $cfg=Join-Path $run 'revived_config/openmr2_setup.cfg'
    [IO.File]::WriteAllText($cfg,('game_directory = "'+$game+'"'+"`nframe_rate = 60fps`ninterpolation = 0`nsound_effects = 0`nrace_music = 0`nmenu_music = 0`nlance_boyle_videos = 0`n"),[Text.UTF8Encoding]::new($true))
    $logPath=Join-Path $run 'revived_config/openmr2.log'
    if(Test-Path -LiteralPath $logPath){Remove-Item -LiteralPath $logPath}
    foreach($launch in 1..2) {
        $shot=Join-Path $run ("launch$launch.bmp")
        if(Test-Path -LiteralPath $shot){Remove-Item -LiteralPath $shot}
        # No --game argument: exercise the real startup lookup and settings rewrite.
        $p=Start-Process -FilePath $exe -WorkingDirectory $elsewhere -ArgumentList ('--screen menu --size 640x480 --shot "'+$shot+'"') -WindowStyle Hidden -PassThru
        if(!$p.WaitForExit(60000)){$p.Kill();throw "$language launch $launch did not start/exit without a dialog"}
        $p.Refresh()
        $config=[IO.File]::ReadAllText($cfg)
        $noDebugLog=!(Test-Path -LiteralPath $logPath)
        $pass=$p.ExitCode -eq 0 -and (Test-Path -LiteralPath $shot) -and $config.Contains('game_directory = '+$game) -and !$config.Contains('interpolation =') -and $config.Contains('Files_OK = true') -and $config.Contains('verified_language = '+(@('English','French','German').IndexOf($language))) -and $noDebugLog
        $results+=[PSCustomObject]@{language=$language;launch=$launch;without_game_argument=$true;unrelated_working_directory=$true;pass=$pass}
        $results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sourceRoot 'build/windows-startup-results.json')
        if(!$pass){throw "$language launch $launch failed saved-path/language verification"}
    }
    "$language : PASS saved-path startup and restart from unrelated working directory"
}
