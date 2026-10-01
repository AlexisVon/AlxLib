@echo off
setlocal
REM ===========================================================================
REM  build_zlib_msvc.bat
REM
REM  Rebuilds the static zlib consumed by AlxCore, so the archive matches the
REM  rest of the project's CRT and debug-info conventions.
REM
REM      cmd /c 3rdpty\zlib\scripts\build_zlib_msvc.bat
REM
REM  /MD  dynamic CRT, same as every other 3rd-party archive (the shipped
REM       zlib.lib was already /MD, so this one is unchanged on that axis).
REM  /Z7  CodeView records embedded in the objects rather than a side PDB, so
REM       the staged archive is self-contained: no LNK4099, and no lost
REM       symbols for anyone linking it without this build tree.
REM
REM  Verify with:
REM      dumpbin -directives 3rdpty\zlib\lib\zlib.lib   (expect MSVCRT, no LIBCMT)
REM ===========================================================================

for %%I in ("%~dp0..") do set "ZLIB_DIR=%%~fI"

set "SRC_DIR=%ZLIB_DIR%\local"
set "OUT=%ZLIB_DIR%\lib"
set "OBJ=%ZLIB_DIR%\local\build"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC_DIR%\deflate.c" (
  echo [ERROR] zlib sources not found: %SRC_DIR%
  echo         extract zlib-*.tar.gz into 3rdpty\zlib\local first
  exit /b 1
)

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvars64.bat failed: %VCVARS%
  exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%OUT%" mkdir "%OUT%"
cd /d "%OBJ%" || exit /b 1

echo [INFO] compiling zlib ...
cl -nologo -c -O2 -MD -Z7 -I"%SRC_DIR%" -I"%ZLIB_DIR%\include" ^
   "%SRC_DIR%\adler32.c" "%SRC_DIR%\compress.c" "%SRC_DIR%\crc32.c" ^
   "%SRC_DIR%\deflate.c" "%SRC_DIR%\infback.c"  "%SRC_DIR%\inffast.c" ^
   "%SRC_DIR%\inflate.c" "%SRC_DIR%\inftrees.c" "%SRC_DIR%\trees.c" ^
   "%SRC_DIR%\uncompr.c" "%SRC_DIR%\zutil.c" ^
   "%SRC_DIR%\gzclose.c" "%SRC_DIR%\gzlib.c"    "%SRC_DIR%\gzread.c" ^
   "%SRC_DIR%\gzwrite.c"
if errorlevel 1 (
  echo [ERROR] compile failed
  exit /b 1
)

echo [INFO] archiving ...
lib -nologo -out:"%OUT%\zlib.lib" *.obj
if errorlevel 1 (
  echo [ERROR] lib failed
  exit /b 1
)

echo.
echo [OK] staged %OUT%\zlib.lib
