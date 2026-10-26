param(
 [string]$ObsPath='C:/Program Files/obs-studio',
 [string]$SceneAnchorDll='',
 [string]$SourceSwitcherDll='',
 [string]$SourceSwitcherData=''
)
$ErrorActionPreference='Stop'
$testRoot=Split-Path $PSScriptRoot -Parent
if((Get-Item "$ObsPath/bin/64bit/obs.dll").VersionInfo.FileVersion -notmatch '^33\.'){throw 'OBS33 required'}
$runRoot=Join-Path $testRoot ('.local/runtime-obs33/run-'+[guid]::NewGuid().ToString('N'))
$runtimeRoot=Join-Path $runRoot 'obs'
$resultRoot=Join-Path $runRoot 'results'
New-Item -ItemType Directory -Force $runtimeRoot,$resultRoot | Out-Null
foreach($name in @('bin','core','data')){Copy-Item -LiteralPath "$ObsPath/$name" -Destination $runtimeRoot -Recurse -Force}
function StagePlugin($name,$dll,$data=''){
 $target=Join-Path $runtimeRoot "plugins/$name"
 New-Item -ItemType Directory -Force "$target/data" | Out-Null
 Copy-Item -LiteralPath $dll -Destination "$target/$name.dll" -Force
 if($data){Copy-Item "$data/*" "$target/data" -Recurse -Force}
}
StagePlugin 'obs-multiview-plus' "$testRoot/build-obs33/Release/obs-multiview-plus.dll"
StagePlugin 'obs-multiview-smoke' "$testRoot/build-obs33/Release/obs-multiview-smoke.dll"
if($SceneAnchorDll){StagePlugin 'scene-anchor' $SceneAnchorDll (Join-Path (Split-Path (Split-Path (Split-Path $SceneAnchorDll -Parent) -Parent) -Parent) 'data')}
if($SourceSwitcherDll){StagePlugin 'source-switcher' $SourceSwitcherDll $SourceSwitcherData}
$configRoot=Join-Path $runtimeRoot 'config/obs-studio'
New-Item -ItemType Directory -Force $configRoot | Out-Null
Set-Content "$configRoot/global.ini" "[General]`nEnableAutoUpdates=false`nLanguage=en-US`n"
Set-Content "$configRoot/user.ini" "[General]`nFirstRun=true`nLanguage=en-US`n[BasicWindow]`nWarnBeforeExit=false`n"
$env:MV_SMOKE_OUTPUT=$resultRoot
$env:MV_TEST_SKIP_CONTROLLER='1'
$testProcess=$null
try{
 $testProcess=Start-Process -FilePath "$runtimeRoot/bin/64bit/obs64.exe" -ArgumentList '--portable','--multi','--disable-shutdown-check','--disable-updater','--disable-missing-files-check' -WorkingDirectory "$runtimeRoot/bin/64bit" -WindowStyle Hidden -PassThru
 if(-not $testProcess.WaitForExit(55000)){throw 'Owned OBS33 UI test timeout'}
 if($testProcess.ExitCode -ne 0){throw "Owned OBS33 exited $($testProcess.ExitCode)"}
 $results=Get-Content "$resultRoot/result.json" -Raw | ConvertFrom-Json
 if(-not $results.passed){throw ('UI assertions failed: '+($results.failures -join ', '))}
 $log=Get-ChildItem "$configRoot/logs" -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
 Copy-Item $log.FullName "$resultRoot/obs.log"
 $logText=Get-Content $log.FullName -Raw
 if($logText -match 'Double destroy|source\(s\) were remaining'){throw 'Source lifetime error'}
 if($logText -match 'Number of memory leaks: (\d+)'){Write-Output ('Observed whole-OBS shutdown allocations: '+$Matches[1])}
 Write-Output "PASS OBS33 UI/settings/reopen/scene-tree: $resultRoot"
}finally{
 if($testProcess -and -not $testProcess.HasExited){Stop-Process -Id $testProcess.Id}
 Remove-Item Env:/MV_SMOKE_OUTPUT -ErrorAction SilentlyContinue
 Remove-Item Env:/MV_TEST_SKIP_CONTROLLER -ErrorAction SilentlyContinue
}
