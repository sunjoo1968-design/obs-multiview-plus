$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $cmakeText = Get-Content CMakeLists.txt -Raw
    if ($cmakeText -notmatch 'project\(obs-multiview-plus VERSION ([0-9.]+)') { throw 'Version missing' }
    $version = $Matches[1]
    $destination = Join-Path $projectRoot "release/$version"
    New-Item -ItemType Directory -Force $destination | Out-Null
    Compress-Archive -Path dist/obs-plugins,dist/data -DestinationPath "$destination/obs-multiview-plus-$version-windows-x64.zip" -Force
    Copy-Item -LiteralPath dist/obs-plugins/64bit/obs-multiview-plus.dll -Destination $destination -Force
    Copy-Item -LiteralPath README.md,LICENSE -Destination $destination -Force
    Compress-Archive -Path src,tests,scripts,docs,CMakeLists.txt,README.md,LICENSE -DestinationPath "$destination/obs-multiview-plus-$version-source.zip" -Force
    Get-ChildItem -LiteralPath $destination -File | Where-Object Name -ne SHA256SUMS.txt | Sort-Object Name | ForEach-Object {
        $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
        "$($hash.Hash.ToLowerInvariant())  $($_.Name)"
    } | Set-Content -LiteralPath "$destination/SHA256SUMS.txt" -Encoding ascii
    Write-Output "Release: $destination"
} finally { Pop-Location }
