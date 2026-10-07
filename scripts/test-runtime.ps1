param([string]$ObsPath = 'C:\Program Files\obs-studio', [switch]$WithSceneAnchor, [switch]$WithSourceSwitcher,
      [string]$PluginDllPath = 'build/Release/obs-multiview-plus.dll',
      [ValidateRange(0,100)][int]$ExpectedMemoryLeaks = 0)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$runRoot = Join-Path $projectRoot ('.local\runtime\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtimeRoot = Join-Path $runRoot 'obs'
$resultsRoot = Join-Path $runRoot 'results'
New-Item -ItemType Directory -Force $runtimeRoot,$resultsRoot | Out-Null
Push-Location $projectRoot
try {
    & cmake -S . -B build -DMV_BUILD_SMOKE_DRIVER=ON
    if ($LASTEXITCODE -ne 0) { throw 'Configure failed; run build.ps1 first' }
    & cmake --build build --config Release --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    foreach ($directory in @('bin','data')) {
        & robocopy (Join-Path $ObsPath $directory) (Join-Path $runtimeRoot $directory) /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1
        if ($LASTEXITCODE -ge 8) { throw "Runtime copy failed: $directory" }
    }
    $pluginPath = Join-Path $runtimeRoot 'obs-plugins\64bit'
    New-Item -ItemType Directory -Force $pluginPath | Out-Null
    foreach ($dll in @('image-source','obs-transitions','obs-filters','obs-text','rtmp-services','obs-ffmpeg','obs-outputs','obs-x264')) {
        Copy-Item -LiteralPath (Join-Path $ObsPath "obs-plugins\64bit\$dll.dll") -Destination $pluginPath
    }
    Copy-Item -LiteralPath $PluginDllPath -Destination (Join-Path $pluginPath 'obs-multiview-plus.dll')
    Copy-Item build/Release/obs-multiview-smoke.dll -Destination $pluginPath
    if ($WithSceneAnchor) {
        Copy-Item -LiteralPath (Join-Path $ObsPath 'obs-plugins\64bit\scene-anchor.dll') -Destination $pluginPath
    }
    if ($WithSourceSwitcher) {
        Copy-Item -LiteralPath (Join-Path $ObsPath 'obs-plugins\64bit\source-switcher.dll') -Destination $pluginPath
    }
    $configRoot = Join-Path $runtimeRoot 'config\obs-studio'
    New-Item -ItemType Directory -Force (Join-Path $configRoot 'basic\scenes'),(Join-Path $configRoot 'basic\profiles') | Out-Null
    Set-Content -LiteralPath (Join-Path $configRoot 'global.ini') -Value "[General]`nLastVersion=537001986`nEnableAutoUpdates=false`n"
    Set-Content -LiteralPath (Join-Path $configRoot 'user.ini') -Value "[General]`nFirstRun=true`n"
    $env:MV_SMOKE_OUTPUT = $resultsRoot
    $process = Start-Process -FilePath (Join-Path $runtimeRoot 'bin\64bit\obs64.exe') -ArgumentList '--portable','--multi','--disable-shutdown-check','--disable-updater' -WorkingDirectory (Join-Path $runtimeRoot 'bin\64bit') -WindowStyle Hidden -PassThru
    Remove-Item Env:\MV_SMOKE_OUTPUT
    if (-not $process.WaitForExit(45000)) {
        Stop-Process -Id $process.Id
        throw "Isolated runtime test timed out: $runRoot"
    }
    $result = Get-Content -LiteralPath (Join-Path $resultsRoot 'result.json') -Raw | ConvertFrom-Json
    if (-not $result.passed) { throw "Runtime checks failed: $($result.failures -join ', ')" }
    $log = Get-ChildItem (Join-Path $configRoot 'logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $resultsRoot 'obs.log')
    $logText = Get-Content -LiteralPath $log.FullName -Raw
    if ($logText -match 'Double destroy|source\(s\) were remaining') { throw "Source lifetime error: $resultsRoot" }
    if ($logText -notmatch 'Number of memory leaks: (\d+)') { throw "Missing shutdown leak report: $resultsRoot" }
    $actualLeaks = [int]$Matches[1]
    if ($actualLeaks -ne $ExpectedMemoryLeaks) { throw "Shutdown leaks $actualLeaks; expected $ExpectedMemoryLeaks : $resultsRoot" }
    if ($actualLeaks -gt 0) { Write-Warning "OBS reported $actualLeaks memory leak(s), matching the explicitly supplied comparison baseline. This is not a zero-leak shutdown." }
    Write-Output "Runtime checks passed: $resultsRoot"
} finally {
    if (Test-Path Env:\MV_SMOKE_OUTPUT) { Remove-Item Env:\MV_SMOKE_OUTPUT }
    Pop-Location
}
