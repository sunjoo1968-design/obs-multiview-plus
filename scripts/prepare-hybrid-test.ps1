param([string]$ObsPath='C:/Program Files/obs-studio')
$ErrorActionPreference='Stop'
$mvRoot=Split-Path $PSScriptRoot -Parent
$mvSandbox=Join-Path $mvRoot '.local/hybrid-runtime/obs'
New-Item -ItemType Directory -Force "$mvSandbox/bin", "$mvSandbox/data", "$mvSandbox/obs-plugins/64bit" | Out-Null
foreach($mvDirectory in @('bin','data')) {
  robocopy (Join-Path $ObsPath $mvDirectory) (Join-Path $mvSandbox $mvDirectory) /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1
  if($LASTEXITCODE -ge 8){throw 'Test runtime copy failed'}
}
foreach($mvPlugin in @('image-source','obs-transitions','obs-filters','obs-text','rtmp-services','obs-ffmpeg','obs-outputs','obs-x264','obs-websocket','frontend-tools','source-switcher','camera-mix-hybrid')) {
  Copy-Item -LiteralPath "$ObsPath/obs-plugins/64bit/$mvPlugin.dll" -Destination "$mvSandbox/obs-plugins/64bit" -Force
}
foreach($mvPlugin in @('obs-multiview-plus','obs-multiview-hybrid-probe')) {
  Copy-Item -LiteralPath (Join-Path $mvRoot "build/Release/$mvPlugin.dll") -Destination "$mvSandbox/obs-plugins/64bit" -Force
}
Set-Content -LiteralPath "$mvSandbox/portable_mode.txt" -Value ''
Write-Output "Prepared isolated Hybrid test: $mvSandbox"
