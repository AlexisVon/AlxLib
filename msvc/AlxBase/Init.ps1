# init.ps1 - Sync version numbers from CMakeLists.txt to MSVC .rc files

$ErrorActionPreference = "Stop"
$root = "$PSScriptRoot\..\.."
$cmakeFile = "$root\CMakeLists.txt"

# Read version numbers from CMakeLists.txt
$cmakeContent = Get-Content $cmakeFile -Raw
$majNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_MAJOR\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$minNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_MINOR\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$patNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_PATCH\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })

# One version for the whole library: every module carries the same 4-segment number
$ver = "$majNum,$minNum,$patNum,0"
$versions = @{
    "AlxBase" = $ver
    "AlxCore" = $ver
    "AlxScpt" = $ver
    "AlxComm" = $ver
}

Write-Host "=== Syncing versions from CMakeLists.txt ==="
Write-Host "  Library version: $ver"

foreach ($module in $versions.Keys) {
    $ver = $versions[$module]
    $verStr = $ver -replace ',', '.'
    $rcFile = "$root\msvc\$module\$module.rc"

    if (-not (Test-Path $rcFile)) {
        Write-Warning "  $module.rc not found, skipping"
        continue
    }

    # Read as UTF-16LE
    $content = [System.IO.File]::ReadAllText($rcFile, [System.Text.Encoding]::Unicode)

    # Replace FILEVERSION and PRODUCTVERSION (comma-separated)
    # Use [regex]::Replace with single-quote replacement to avoid $1 being expanded
    $content = [regex]::Replace($content, '(FILEVERSION\s+)[\d,]+', { param($m) $m.Groups[1].Value + $ver })
    $content = [regex]::Replace($content, '(PRODUCTVERSION\s+)[\d,]+', { param($m) $m.Groups[1].Value + $ver })

    # Replace string FileVersion and ProductVersion (dot-separated)
    $content = [regex]::Replace($content, '("FileVersion",\s*")[\d.]+', { param($m) $m.Groups[1].Value + $verStr })
    $content = [regex]::Replace($content, '("ProductVersion",\s*")[\d.]+', { param($m) $m.Groups[1].Value + $verStr })

    # Write back as UTF-16LE BOM
    $utf16le = New-Object System.Text.UnicodeEncoding($false, $true)
    [System.IO.File]::WriteAllText($rcFile, $content, $utf16le)

    Write-Host "  $module -> $verStr"
}

Write-Host "=== Done ==="
