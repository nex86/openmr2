$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$fixtures=(Resolve-Path (Join-Path $root '../..')).Path
$results=@()
foreach($language in @('English','French','German')){
    $run=Join-Path $root ('build/retro-windows/'+$language)
    New-Item -ItemType Directory -Force (Join-Path $run 'revived_config') | Out-Null
    $exe=Join-Path $run 'OpenMR2.exe'
    Copy-Item -LiteralPath (Join-Path $root 'build/OpenMR2.exe') -Destination $exe
    $cfg=Join-Path $run 'revived_config/openmr2_setup.cfg'
    [IO.File]::WriteAllText($cfg,"graphics_mode = 1`ntexture_filter = 0`nmsaa = 8`nfxaa = 1`nframe_rate = 60fps`ndebug_console = 1`nsound_effects = 0`nrace_music = 0`nmenu_music = 0`nlance_boyle_videos = 0`n")
    foreach($case in @('gfx','shop','race')){
        $game=Join-Path $fixtures ('Megarace 2 '+$language)
        $argsLine='--game "'+$game+'" --size 960x720 --shot '+$case+'.bmp'
        if($case -eq 'race'){$argsLine+=' --track 1 --frame 150 --shot-after 40 --shotfps 25'}else{$argsLine+=' --screen '+$case}
        $p=Start-Process -FilePath $exe -ArgumentList $argsLine -WindowStyle Hidden -PassThru -Wait
        $text=[IO.File]::ReadAllText($cfg)
        $log=[IO.File]::ReadAllText((Join-Path $run 'revived_config/openmr2.log'))
        $pass=$p.ExitCode -eq 0 -and (Test-Path -LiteralPath (Join-Path $run ($case+'.bmp'))) -and $text.Contains('msaa = 0') -and $text.Contains('fxaa = 0') -and $text.Contains('frame_rate = original') -and $text.Contains('revived_msaa = 8') -and $text.Contains('revived_frame_rate = 1') -and !$log.Contains('shader compile error') -and !$log.Contains('program link error') -and !$log.Contains('[OpenGL error]')
        if($case -eq 'race'){$pass=$pass -and $log.Contains('Render target: 320x400; Retro Mode; MSAA/FXAA disabled; Original framerate.')}else{$pass=$pass -and $log.Contains('Render target: 640x480; Retro Mode')}
        $results+=[PSCustomObject]@{language=$language;case=$case;pass=$pass}
        $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build/windows-retro-results.json')
        if(!$pass){throw "$language Retro $case failed"}
    }
    "$language : PASS real Retro graphics, shop and race; saved Revived preferences retained"
}
