# pack.ps1 - Package AlxLib for Windows (matches package.sh format)

$ErrorActionPreference = "Stop"
$root = "$PSScriptRoot\..\.."
$cmakeFile = "$root\CMakeLists.txt"

# Read version numbers from CMakeLists.txt (same logic as package.sh)
$cmakeContent = Get-Content $cmakeFile -Raw
$majNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_MAJOR\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$minNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_MINOR\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$patNum = [int]($cmakeContent | Select-String 'set\(ALXLIB_VERSION_PATCH\s+(\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })

$verStr = "$majNum.$minNum.$patNum"

# Package name (-g<short sha>, plus -dirty when tracked files are modified)
$gitSuffix = ""
try { $gitSha = & git -C $root rev-parse --short HEAD 2>$null } catch { $gitSha = $null }
if ($gitSha) {
    $gitSuffix = "-g$gitSha"
    if (& git -C $root status --porcelain --untracked-files=no 2>$null) { $gitSuffix = "$gitSuffix-dirty" }
} else {
    Write-Host "*** warning: cannot read the git sha, package name carries none"
}

$pkgName = "AlxLib-${verStr}-win-x64$gitSuffix"
$pkgDir = "$root\$pkgName"
$pdbDir = "$pkgDir.pdb"

Write-Host "=== Packaging $pkgName ==="

# Clean
if (Test-Path $pkgDir) { Remove-Item -Recurse -Force $pkgDir }
Remove-Item "$root\$pkgName.zip" -ErrorAction SilentlyContinue
if (Test-Path $pdbDir) { Remove-Item -Recurse -Force $pdbDir }
Remove-Item "$root\$pkgName.pdb.zip" -ErrorAction SilentlyContinue

# Create directory structure
New-Item -ItemType Directory -Force -Path $pkgDir\include, $pkgDir\bin, $pkgDir\lib | Out-Null
New-Item -ItemType Directory -Force -Path $pdbDir\pdb | Out-Null

# Copy headers
Write-Host "Copying headers..."
Copy-Item -Recurse "$root\include\alxbase" "$pkgDir\include\"
Copy-Item -Recurse "$root\include\alxcore" "$pkgDir\include\"
Copy-Item -Recurse "$root\include\alxscpt" "$pkgDir\include\"
Copy-Item -Recurse "$root\include\alxcomm" "$pkgDir\include\"

# Copy DLLs
Write-Host "Copying DLLs..."
Get-Item "$root\bin\Alx*.dll" -ErrorAction SilentlyContinue | Copy-Item -Destination "$pkgDir\bin\"

# Copy import libraries
Write-Host "Copying import libraries..."
Get-Item "$root\bin\Alx*.lib" -ErrorAction SilentlyContinue | Copy-Item -Destination "$pkgDir\lib\"

# Copy PDB files (exclude *_GTest.pdb, separate package)
Write-Host "Copying PDB files..."
Get-Item "$root\bin\Alx*.pdb" -ErrorAction SilentlyContinue | Copy-Item -Destination "$pdbDir\pdb\"
Remove-Item "$pdbDir\pdb\*_GTest.pdb" -ErrorAction SilentlyContinue

# Copy docs to package root
Write-Host "Copying docs..."
Copy-Item "$root\README.md" "$pkgDir\" -ErrorAction SilentlyContinue
Copy-Item "$root\LICENSE" "$pkgDir\" -ErrorAction SilentlyContinue
Copy-Item -Recurse "$root\doc" "$pkgDir\doc" -ErrorAction SilentlyContinue
# issue/feature/todo/verify are internal ledgers and stay out of the package (same as package.sh)
Remove-Item "$pkgDir\doc\issue.md", "$pkgDir\doc\feature.md", "$pkgDir\doc\todo.md", "$pkgDir\doc\verify.md" -ErrorAction SilentlyContinue

# Copy third-party notices
Write-Host "Copying third-party notices..."
Copy-Item -Recurse "$root\licenses" "$pkgDir\licenses" -ErrorAction SilentlyContinue
Copy-Item "$root\THIRD-PARTY.md" "$pkgDir\" -ErrorAction SilentlyContinue

# Create archive
Write-Host "Creating archive..."
Push-Location $root
& 7z a -tzip "$pkgName.zip" "$pkgName" | Out-Null
& 7z a -tzip "$pkgName.pdb.zip" "$pkgName.pdb" | Out-Null
Pop-Location

# Clean
Remove-Item -Recurse -Force $pkgDir, $pdbDir

Write-Host "=== Done ==="
Write-Host "  Main:   .\$pkgName.zip"
Write-Host "  Debug:  .\$pkgName.pdb.zip"
