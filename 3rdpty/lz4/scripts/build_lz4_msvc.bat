@echo off
setlocal
REM ===========================================================================
REM  build_lz4_msvc.bat
REM
REM  Rebuilds the static LZ4 consumed by AlxCore.
REM
REM      cmd /c 3rdpty\lz4\scripts\build_lz4_msvc.bat
REM
REM  The shipped liblz4_static.lib was built /Zi, so its objects referenced
REM  liblz4_static.pdb -- a PDB that was never staged anywhere. Every link of
REM  AlxCore therefore reported LNK4099 and lost the LZ4 symbols. /Z7 embeds
REM  the records in the objects instead, which is what makes the archive
REM  self-contained.
REM
REM  Only lz4.c / lz4hc.c / lz4frame.c / xxhash.c are compiled: the shipped
REM  archive contains no LZ4IO_* symbols, so lz4file.c (the high-level file
REM  API) was not part of it and is not added here either.
REM
REM  Verify with:
REM      dumpbin -directives 3rdpty\lz4\lib\liblz4_static.lib  (expect MSVCRT)
REM ===========================================================================

for %%I in ("%~dp0..") do set "LZ4_DIR=%%~fI"

set "SRC_ROOT=%LZ4_DIR%\local\lz4-1.10.0"
set "SRC_DIR=%SRC_ROOT%\lib"
set "OUT=%LZ4_DIR%\lib"
set "OBJ=%LZ4_DIR%\local\build"
set "TARBALL=%LZ4_DIR%\lz4-1.10.0.tar.gz"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC_DIR%\lz4.c" (
  if not exist "%TARBALL%" (
    echo [ERROR] %TARBALL% not found
    echo         Upstream archives are no longer kept in this repository.
    echo         See 3rdpty\README.md for the download link and hash, or unpack
    echo         the source tree yourself where this script expects it.
    exit /b 1
  )
  echo [INFO] extracting lz4-1.10.0.tar.gz
  if not exist "%LZ4_DIR%\local" mkdir "%LZ4_DIR%\local"
  REM cd first, then pass a relative name: GNU tar reads the colon in
  REM "E:\..." as a remote host spec and fails with "Cannot connect to \E:".
  pushd "%LZ4_DIR%\local"
  tar -xzf "..\lz4-1.10.0.tar.gz"
  if errorlevel 1 (
    popd
    echo [ERROR] extraction failed
    exit /b 1
  )
  popd
)

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvars64.bat failed: %VCVARS%
  exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%OUT%" mkdir "%OUT%"
cd /d "%OBJ%" || exit /b 1

echo [INFO] compiling lz4 ...
cl -nologo -c -O2 -MD -Z7 -I"%SRC_DIR%" ^
   "%SRC_DIR%\lz4.c" "%SRC_DIR%\lz4hc.c" "%SRC_DIR%\lz4frame.c" "%SRC_DIR%\xxhash.c"
if errorlevel 1 (
  echo [ERROR] compile failed
  exit /b 1
)

echo [INFO] archiving ...
lib -nologo -out:"%OUT%\liblz4_static.lib" *.obj
if errorlevel 1 (
  echo [ERROR] lib failed
  exit /b 1
)

echo.
echo [OK] staged %OUT%\liblz4_static.lib
