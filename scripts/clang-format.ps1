# scripts/clang-format.ps1
# Run clang-format on all native source files using the repo's .clang-format config.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/clang-format.ps1 [-Check]
#   -Check  Dry-run mode: exit 1 if any file would change (for CI).

param(
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

# Locate clang-format from VS 2022 LLVM tools.
$clangFormat = Get-ChildItem `
    "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\Llvm\x64\bin\clang-format.exe" `
    -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $clangFormat) {
    $clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
}

if (-not $clangFormat) {
    Write-Error "clang-format not found. Install LLVM via Visual Studio or https://llvm.org"
    exit 1
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$repoRoot\.clang-format")) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
}

$sources = Get-ChildItem -Path "$repoRoot\native\src" -Include *.cpp, *.h -Recurse

if ($Check) {
    Write-Host "Checking clang-format (dry run)..."
    $dirty = @()
    foreach ($f in $sources) {
        $formatted = & $clangFormat --style=file "$f" 2>&1
        $original = Get-Content -Raw $f
        if ($formatted -join "`n" -ne $original.TrimEnd()) {
            $dirty += $f.Name
        }
    }
    if ($dirty.Count -gt 0) {
        Write-Host "The following files need formatting:" -ForegroundColor Red
        $dirty | ForEach-Object { Write-Host "  $_" }
        exit 1
    }
    Write-Host "All files formatted correctly." -ForegroundColor Green
} else {
    Write-Host "Running clang-format on native sources..."
    foreach ($f in $sources) {
        & $clangFormat -i --style=file "$f"
        Write-Host "  Formatted: $($f.Name)"
    }
    Write-Host "Done." -ForegroundColor Green
}
