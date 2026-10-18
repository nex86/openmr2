$ErrorActionPreference='Stop'
$sourceRoot=Split-Path $PSScriptRoot
$fixtureRoot=(Resolve-Path (Join-Path $sourceRoot '../..')).Path
$results=@()
foreach($language in @('English','French','German')) {
    $run=Join-Path $sourceRoot ('build/smoke-windows/'+$language)
    New-Item -ItemType Directory -Force (Join-Path $run 'revived_config') | Out-Null
    $exe=Join-Path $run 'OpenMR2.exe'
    Copy-Item -LiteralPath (Join-Path $sourceRoot 'build/OpenMR2.exe') -Destination $exe
    $game=Join-Path $fixtureRoot ('Megarace 2 '+$language)
    $cases=@('carshop','shop','info','hiscore','save','select','replays','fonts','prize')
    foreach($case in $cases) {
        $mode=if($case.StartsWith('hud-')){$case.Substring(4)}else{'original'}
        $cfgPath=Join-Path $run 'revived_config/openmr2_setup.cfg'
        $receipt=if(Test-Path -LiteralPath $cfgPath){([IO.File]::ReadAllLines($cfgPath) | Where-Object {$_ -match '^(Files_OK|verified_directory|verified_language)\s*='}) -join "`n"}else{''}
        [IO.File]::WriteAllText((Join-Path $run 'revived_config/openmr2_setup.cfg'),"$receipt`nframe_rate = $mode`nsound_effects = 0`nrace_music = 0`nmenu_music = 0`nlance_boyle_videos = 0`n")
        $argsLine='--game "'+$game+'" --size 640x480 --shot '+$case+'.bmp'
        if($case.StartsWith('hud-')){$argsLine+=' --track 1 --frame 150 --shot-after 240 --shotfps 60'}else{$argsLine+=' --screen '+$case}
        $p=Start-Process -FilePath $exe -ArgumentList $argsLine -WindowStyle Hidden -PassThru -Wait
        $pass=$p.ExitCode -eq 0 -and (Test-Path -LiteralPath (Join-Path $run ($case+'.bmp')))
        $results+=[PSCustomObject]@{language=$language;case=$case;exit_code=$p.ExitCode;screenshot=($language+'/'+$case+'.bmp');pass=$pass}
        $results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $sourceRoot 'build/windows-localization-results.json')
        if(!$pass){throw "$language $case graphics smoke failed"}
    }
    "$language : PASS nine additional localization screens"
}
