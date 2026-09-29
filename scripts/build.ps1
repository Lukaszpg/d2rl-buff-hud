<#
.SYNOPSIS
    Configure, build and test Buff Panel; optionally publish its DLL/MPQ pair.

.DESCRIPTION
    Locates the Visual Studio x64 build environment (cl.exe, rc.exe) via
    vswhere, imports it into this session, then runs the CMake preset.

.EXAMPLE
    .\scripts\build.ps1
    .\scripts\build.ps1 -Preset debug
    .\scripts\build.ps1 -Dist
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string]$Preset = 'release',
    [switch]$Clean,
    [switch]$Dist
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Dist -and $Preset -ne 'release') { throw '-Dist requires -Preset release.' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'vswhere.exe not found. Install Visual Studio 2022 with the "Desktop development with C++" workload.'
}

$vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) {
    throw 'No Visual Studio installation with the MSVC x64 toolset was found.'
}

# Import the x64 developer environment into this PowerShell session. vcvars64
# writes progress to stdout/stderr; silencing it inside cmd keeps PowerShell from
# turning native stderr into a terminating NativeCommandError.
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
& "$env:ComSpec" /s /c "call `"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake was not found on PATH. Install it with: winget install --id Kitware.CMake'
}

Push-Location $repo
try {
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "cmake --preset $Preset failed." }

    if ($Clean) {
        cmake --build --preset $Preset --target clean
        if ($LASTEXITCODE -ne 0) { throw "cmake --build --preset $Preset --target clean failed." }
    }

    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Building tests failed." }
    ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Buff Panel tests failed." }

    Write-Host "Built plugin pair is in build\$Preset\stage\." -ForegroundColor Green
    if ($Dist) {
        cmake --build --preset dist
        if ($LASTEXITCODE -ne 0) { throw "Publishing dist failed." }
        Write-Host 'dist\ has been refreshed.' -ForegroundColor Green
    }
}
finally {
    Pop-Location
}
