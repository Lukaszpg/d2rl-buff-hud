[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$parts = $Version.Split('.')
$major = [int]$parts[0]
$minor = [int]$parts[1]
$patch = [int]$parts[2]
$utf8 = [Text.UTF8Encoding]::new($false)

function Rewrite([string]$RelativePath, [scriptblock]$Transform) {
    $path = Join-Path $repo $RelativePath
    $before = [IO.File]::ReadAllText($path)
    $after = & $Transform $before
    if ($after -eq $before) {
        throw "Version rewrite made no change in $RelativePath"
    }
    [IO.File]::WriteAllText($path, $after, $utf8)
}

[IO.File]::WriteAllText((Join-Path $repo 'VERSION'), "$Version`n", $utf8)

Rewrite 'CMakeLists.txt' {
    param($text)
    [regex]::Replace(
        $text,
        'project\(BuffPanel VERSION \d+\.\d+\.\d+ LANGUAGES CXX RC\)',
        "project(BuffPanel VERSION $Version LANGUAGES CXX RC)",
        1)
}

Rewrite 'src/plugin.cpp' {
    param($text)
    $text = [regex]::Replace($text, 'Buff HUD \d+\.\d+\.\d+ requires', "Buff HUD $Version requires")
    $text = [regex]::Replace($text, '\.version = "\d+\.\d+\.\d+"', ".version = `"$Version`"")
    [regex]::Replace($text, 'Buff HUD \d+\.\d+\.\d+ loaded', "Buff HUD $Version loaded")
}

Rewrite 'src/plugin.rc' {
    param($text)
    $text = [regex]::Replace($text, 'FILEVERSION \d+,\d+,\d+,0', "FILEVERSION $major,$minor,$patch,0")
    $text = [regex]::Replace($text, 'PRODUCTVERSION \d+,\d+,\d+,0', "PRODUCTVERSION $major,$minor,$patch,0")
    $text = [regex]::Replace($text, 'VALUE "FileVersion", "\d+\.\d+\.\d+"', "VALUE `"FileVersion`", `"$Version`"")
    [regex]::Replace($text, 'VALUE "ProductVersion", "\d+\.\d+\.\d+"', "VALUE `"ProductVersion`", `"$Version`"")
}

foreach ($relative in @(
    'src/systems/buff_hud/buff_hud.cpp',
    'src/systems/buff_tracker/buff_tracker.cpp'
)) {
    Rewrite $relative {
        param($text)
        [regex]::Replace($text, 'Buff HUD \d+\.\d+\.\d+', "Buff HUD $Version")
    }
}

Write-Host "Buff HUD version synchronized to $Version"
