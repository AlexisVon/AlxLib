@echo off
setlocal
REM ===========================================================================
REM  build_pcre2_msvc.bat
REM
REM  Builds the static 8-bit PCRE2 consumed by AlxCore and stages it as
REM  3rdpty\pcre2\lib\pcre2-8-static.lib plus 3rdpty\pcre2\include\pcre2.h.
REM
REM      cmd /c 3rdpty\pcre2\scripts\build_pcre2_msvc.bat
REM
REM  Only the 8-bit library is built: AlxCore defines
REM  PCRE2_CODE_UNIT_WIDTH 8 (source\alxcore\aregex_pcre2.cpp). The Linux
REM  archive is 8-bit only too, so both platforms link the same code.
REM
REM  /MD  MultiThreadedDLL, same as every other 3rd-party archive.
REM  /Z7  CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded embeds the CodeView
REM       records in the objects, so the staged archive is self-contained.
REM
REM  JIT stays off (upstream's CMake default): AlxCore never calls
REM  pcre2_jit_compile, and the interpreter path keeps the build free of
REM  sljit's per-architecture assembly.
REM
REM  Verify with:
REM      dumpbin -directives 3rdpty\pcre2\lib\pcre2-8-static.lib   (expect MSVCRT)
REM ===========================================================================

for %%I in ("%~dp0..") do set "PCRE2_DIR=%%~fI"

set "SRC_DIR=%PCRE2_DIR%\local\pcre2-10.48"
set "BUILD_DIR=%PCRE2_DIR%\local\build"
set "TARBALL=%PCRE2_DIR%\pcre2-10.48.tar.gz"
set "CMAKE=D:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA=D:\Qt\Tools\Ninja\ninja.exe"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC_DIR%\CMakeLists.txt" (
  if not exist "%TARBALL%" (
    echo [ERROR] %TARBALL% not found
    echo         Upstream archives are no longer kept in this repository.
    echo         See 3rdpty\README.md for the download link and hash, or unpack
    echo         the source tree yourself where this script expects it.
    exit /b 1
  )
  echo [INFO] extracting pcre2-10.48.tar.gz
  if not exist "%PCRE2_DIR%\local" mkdir "%PCRE2_DIR%\local"
  REM cd first, then pass a relative name: GNU tar reads the colon in
  REM "E:\..." as a remote host spec and fails with "Cannot connect to \E:".
  if not exist "%SRC_DIR%" mkdir "%SRC_DIR%"
  pushd "%PCRE2_DIR%\local"
  tar -xzf "..\pcre2-10.48.tar.gz" -C "pcre2-10.48" --strip-components=1
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

echo [INFO] configuring ...
"%CMAKE%" -G Ninja -S "%SRC_DIR%" -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DBUILD_STATIC_LIBS=ON ^
  -DPCRE2_BUILD_PCRE2_8=ON ^
  -DPCRE2_BUILD_PCRE2_16=OFF ^
  -DPCRE2_BUILD_PCRE2_32=OFF ^
  -DPCRE2_BUILD_TESTS=OFF ^
  -DPCRE2_BUILD_PCRE2GREP=OFF
if errorlevel 1 (
  echo [ERROR] configure failed
  exit /b 1
)

echo [INFO] building ...
"%CMAKE%" --build "%BUILD_DIR%"
if errorlevel 1 (
  echo [ERROR] build failed
  exit /b 1
)

echo [INFO] staging ...
if not exist "%PCRE2_DIR%\lib" mkdir "%PCRE2_DIR%\lib"
if not exist "%PCRE2_DIR%\include" mkdir "%PCRE2_DIR%\include"
copy /y "%BUILD_DIR%\pcre2-8-static.lib" "%PCRE2_DIR%\lib\" >nul
if errorlevel 1 ( echo [ERROR] pcre2-8-static.lib not produced & exit /b 1 )
copy /y "%BUILD_DIR%\interface\pcre2.h" "%PCRE2_DIR%\include\" >nul
if errorlevel 1 ( echo [ERROR] pcre2.h not produced & exit /b 1 )

echo.
echo [OK] staged %PCRE2_DIR%\lib\pcre2-8-static.lib and include\pcre2.h
