param(
    [string]$ObsPath = 'C:\Program Files\obs-studio',
    [switch]$SkipDownload
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    if (-not $SkipDownload) {
        & node scripts/prepare-deps.mjs
        if ($LASTEXITCODE -ne 0) { throw 'Dependency preparation failed' }
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) { throw 'Visual Studio C++ x64 Build Tools are required' }
    $toolset = Get-ChildItem (Join-Path $vsRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $toolDir = Join-Path $toolset.FullName 'bin\Hostx64\x64'
    $imports = Join-Path $projectRoot '.deps\obs-imports'
    New-Item -ItemType Directory -Force $imports | Out-Null
    foreach ($module in @('obs', 'obs-frontend-api')) {
        $dll = Join-Path $ObsPath "bin\64bit\$module.dll"
        if (-not (Test-Path -LiteralPath $dll)) { throw "Missing OBS library: $dll" }
        $exports = & (Join-Path $toolDir 'dumpbin.exe') /nologo /exports $dll
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $dll" }
        $names = @($exports | ForEach-Object {
            if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') { $Matches[1] }
        })
        if ($names.Count -lt 10) { throw "No exports found for $module" }
        $defPath = Join-Path $imports "$module.def"
        @("LIBRARY $module.dll", 'EXPORTS') + $names | Set-Content -LiteralPath $defPath -Encoding ascii
        & (Join-Path $toolDir 'lib.exe') /nologo "/def:$defPath" /machine:x64 "/out:$(Join-Path $imports "$module.lib")"
        if ($LASTEXITCODE -ne 0) { throw "Import library failed: $module" }
    }
    & cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_GENERATOR_INSTANCE=$vsRoot"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & cmake --build build --config Release --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    & cmake --install build --config Release --prefix dist
    if ($LASTEXITCODE -ne 0) { throw 'Package staging failed' }
    & "$PSScriptRoot/package.ps1"
} finally { Pop-Location }
