# scripts/clang-tidy.ps1
# Run clang-tidy on native source files.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/clang-tidy.ps1 [-Fix]
#   -Fix  Apply suggested fixes automatically.

param(
    [switch]$Fix
)

$ErrorActionPreference = 'Stop'

# Locate clang-tidy from VS 2022 LLVM tools.
$clangTidy = Get-ChildItem `
    "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\Llvm\x64\bin\clang-tidy.exe" `
    -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $clangTidy) {
    $clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
}

if (-not $clangTidy) {
    Write-Error "clang-tidy not found. Install LLVM via Visual Studio or https://llvm.org"
    exit 1
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$repoRoot\native")) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
}

# Resolve include directories.
$winSdkInclude = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
$cppWinRT      = "$winSdkInclude\cppwinrt"
$nativeDir     = "$repoRoot\native"
$interopIncl   = "$nativeDir\third_party\winget_interop\include"
$dartSdkIncl   = "$nativeDir\third_party"

$sources = Get-ChildItem -Path "$nativeDir\src" -Include *.cpp -Recurse |
    Where-Object { $_.Name -ne 'dart_api_dl.c' }

$extraArgs = @(
    '--'
    '-std=c++20'
    '--target=aarch64-pc-windows-msvc'
    '-DWINGET_NC_EXPORTS'
    '-DNOMINMAX'
    '-DWIN32_LEAN_AND_MEAN'
    '-DWINRT_LEAN_AND_MEAN'
    "-I$nativeDir\src"
    "-I$interopIncl"
    "-I$dartSdkIncl"
    "-I$cppWinRT"
    "-I$winSdkInclude\ucrt"
    "-I$winSdkInclude\um"
    "-I$winSdkInclude\shared"
)

$fixArg = if ($Fix) { @('--fix') } else { @() }

Write-Host "Running clang-tidy on native sources..."
$exitCode = 0

foreach ($f in $sources) {
    Write-Host "  Checking: $($f.Name)"
    & $clangTidy $fixArg $f.FullName @extraArgs 2>&1 | ForEach-Object {
        if ($_ -match 'warning:|error:') {
            Write-Host "    $_" -ForegroundColor Yellow
        }
    }
    if ($LASTEXITCODE -ne 0) { $exitCode = 1 }
}

if ($exitCode -eq 0) {
    Write-Host "No clang-tidy errors." -ForegroundColor Green
} else {
    Write-Host "clang-tidy reported warnings/errors." -ForegroundColor Yellow
}

exit $exitCode
