# Sunjoo OBS Link / SunjooAn. Packages only explicit release staging directories.
$ErrorActionPreference = 'Stop'
$linkRoot = Split-Path $PSScriptRoot -Parent
if (Test-Path -LiteralPath "$linkRoot/scripts/camera-mix-hybrid.lua") {
    $linkComponent = 'controller'
    $linkText = Get-Content -LiteralPath "$linkRoot/scripts/camera-mix-hybrid.lua" -Raw
    if ($linkText -notmatch "local VERSION, AUTHOR = '([^']+)'") { throw 'Controller version missing' }
    $linkVersion = $Matches[1]
    $linkStage = "$linkRoot/release/camera-mix-hybrid-$linkVersion"
    Copy-Item -LiteralPath "$linkRoot/.local/hybrid/build/Release/camera-mix-hybrid.dll" -Destination "$linkStage/obs-plugins/64bit" -Force
    Copy-Item -LiteralPath "$linkRoot/native/camera-mix-hybrid/output.cpp" -Destination "$linkStage/native/camera-mix-hybrid" -Force
    New-Item -ItemType Directory -Force "$linkStage/docs/04-report" | Out-Null
    Copy-Item -LiteralPath "$PSScriptRoot/package-obs-link.ps1" -Destination "$linkStage/scripts" -Force
    Copy-Item -LiteralPath "$linkRoot/docs/OBS_LINK_SUITE.md" -Destination "$linkStage/docs" -Force
    Copy-Item -LiteralPath "$linkRoot/docs/04-report/obs-link-controller.md" -Destination "$linkStage/docs/04-report" -Force
    $linkLegacyGuide = "$linkRoot/docs/HYBRID_GUIDE.md"
} elseif (Test-Path -LiteralPath "$linkRoot/source/hub/package.json") {
    $linkComponent = 'tally'
    $linkVersion = (Get-Content -LiteralPath "$linkRoot/source/hub/package.json" -Raw | ConvertFrom-Json).version
    $linkStage = "$linkRoot/release/v$linkVersion"
    Copy-Item -LiteralPath "$linkRoot/.build-tools/bridge-build/Release/obs-tally-bridge.dll" -Destination "$linkStage/obs-plugin/obs-plugins/64bit" -Force
    New-Item -ItemType Directory -Force "$linkStage/source/hub" | Out-Null
    Copy-Item -LiteralPath "$linkRoot/source/obs-tally-bridge" -Destination "$linkStage/source" -Recurse -Force
    Copy-Item -LiteralPath "$linkRoot/source/hub/package.json" -Destination "$linkStage/source/hub" -Force
    $linkLegacyGuide = "$linkRoot/docs/OBS_CONTROLLER_COMPAT.md"
} else {
    $linkComponent = 'multiview'
    $linkText = Get-Content -LiteralPath "$linkRoot/CMakeLists.txt" -Raw
    if ($linkText -notmatch 'project\(obs-multiview-plus VERSION ([0-9.]+)') { throw 'Multiview version missing' }
    $linkVersion = $Matches[1]
    $linkStage = "$linkRoot/dist"
    $linkLegacyGuide = "$linkRoot/README.md"
}
$linkGuide = Get-Content -LiteralPath "$linkRoot/docs/OBS_LINK_SUITE.md" -Raw
$linkGuide += "`n`n## 기존 사용법 참고`n아래 문서의 이전 버전 표기는 변경 이력이며 현재 버전은 위 표기를 따릅니다.`n`n"
$linkGuide += Get-Content -LiteralPath $linkLegacyGuide -Raw
[IO.File]::WriteAllText("$linkStage/README.md", $linkGuide, [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath "$linkRoot/docs/04-report/obs-link-$linkComponent.md" -Destination "$linkStage/VALIDATION.md" -Force
$linkFiles = @(Get-ChildItem -LiteralPath $linkStage -Recurse -File | Where-Object Name -ne SHA256SUMS.txt)
$linkRows = foreach ($linkFile in $linkFiles) {
    $linkRelative = [IO.Path]::GetRelativePath($linkStage,$linkFile.FullName).Replace('\','/')
    if ($linkRelative -match '(?i)(wifi-tally\.json|layout\.json|config\.json|LOCAL-RESUME|node_modules|/\.local/)') { throw "Private/unexpected file: $linkRelative" }
    "$((Get-FileHash -LiteralPath $linkFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $linkRelative"
}
[IO.File]::WriteAllText("$linkStage/SHA256SUMS.txt", (($linkRows | Sort-Object) -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
$linkZipPath = "$linkRoot/release/sunjoo-obs-link-$linkComponent-$linkVersion-windows-x64.zip"
Compress-Archive -Path "$linkStage/*" -DestinationPath $linkZipPath -Force
Get-FileHash -LiteralPath $linkZipPath -Algorithm SHA256
