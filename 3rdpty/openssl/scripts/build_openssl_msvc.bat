@echo off
setlocal
REM ===========================================================================
REM  build_openssl_msvc.bat
REM
REM  Builds a static OpenSSL for AlxLib's MSVC x64 build and stages the
REM  artifacts where the .vcxproj files expect them.
REM
REM  Full reproduction from a clean checkout -- run once:
REM      cmd /c 3rdpty\openssl\scripts\build_openssl_msvc.bat
REM
REM  Outputs:
REM      3rdpty\openssl\lib\libssl.lib
REM      3rdpty\openssl\lib\libcrypto.lib
REM      3rdpty\openssl\include-msvc\openssl\configuration.h
REM
REM  Takes roughly 15-20 minutes (nmake is serial). Re-running is cheap: the
REM  source tree and the nmake objects are reused.
REM ===========================================================================

for %%I in ("%~dp0..") do set "OSSL_DIR=%%~fI"
for %%I in ("%~dp0..\..\..") do set "REPO=%%~fI"

set "BUILD_DIR=%OSSL_DIR%\local"
set "SRC_DIR=%BUILD_DIR%\openssl-3.6.3"
set "TARBALL=%OSSL_DIR%\openssl-3.6.3.tar.gz"
set "PREFIX=%BUILD_DIR%\dist"

REM --- tools ----------------------------------------------------------------
REM PERL must be a native Windows perl ($^O eq "MSWin32"). The MSYS/Cygwin perl
REM reports "msys" and breaks Configure's VC target detection.
if not defined PERL set "PERL=D:\Strawberry\perl\bin\perl.exe"
if not exist "%PERL%" (
  echo [WARN] %PERL% not found, falling back to "perl" on PATH
  set "PERL=perl"
)

REM nasm is required for the x86_64 assembly modules.
if not defined NASM set "NASM=D:\Strawberry\c\bin\nasm.exe"
if not exist "%NASM%" (
  echo [WARN] %NASM% not found, falling back to "nasm" on PATH
  set "NASM=nasm"
)

set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

REM --- environment ----------------------------------------------------------
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvars64.bat failed: %VCVARS%
  exit /b 1
)

for %%I in ("%NASM%") do set "PATH=%%~dpI;%PATH%"

echo [INFO] perl:  %PERL%
"%PERL%" -e "print qq{[INFO] perl platform: $^O\n}"
echo [INFO] nasm:  %NASM%
where cl
where nmake
if errorlevel 1 (
  echo [ERROR] nmake not on PATH -- vcvars did not initialise
  exit /b 1
)

REM --- source ---------------------------------------------------------------
if not exist "%SRC_DIR%\Configure" (
  if not exist "%TARBALL%" (
    echo [ERROR] %TARBALL% not found
    echo         Upstream archives are no longer kept in this repository.
    echo         See 3rdpty\README.md for the download link and hash, or unpack
    echo         the source tree yourself where this script expects it.
    exit /b 1
  )
  echo [INFO] extracting openssl-3.6.3.tar.gz
  if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
  REM cd first, then pass a relative name: GNU tar reads the colon in
  REM "E:\..." as a remote host spec and fails with "Cannot connect to \E:".
  pushd "%BUILD_DIR%"
  tar -xzf "..\openssl-3.6.3.tar.gz"
  if errorlevel 1 (
    popd
    echo [ERROR] extraction failed
    exit /b 1
  )
  popd
)

cd /d "%SRC_DIR%"
if errorlevel 1 exit /b 1

REM --- debug info -----------------------------------------------------------
REM OpenSSL's VC targets emit "/Zi /Fdossl_static.pdb": the objects then point at
REM an external PDB that is NOT staged next to the archives. Anyone linking them
REM gets LNK4099 and an AlxComm.pdb without OpenSSL symbols. /Z7 embeds the
REM CodeView records in the objects instead, so the staged libs are
REM self-contained. Costs roughly 1.5x on the archive size.
findstr /C:"Fdossl_static.pdb" "Configurations\10-main.conf" >nul 2>&1
if not errorlevel 1 (
  echo [INFO] switching debug info to self-contained /Z7, forcing a clean rebuild
  "%PERL%" -pi -e "s{/Zi /Fdossl_static\.pdb}{/Z7}g" Configurations\10-main.conf
  if exist makefile nmake /NOLOGO clean >nul 2>&1
  if exist configdata.pm del /q configdata.pm >nul 2>&1
)

REM --- configure ------------------------------------------------------------
REM Do NOT use caret line continuations inside a parenthesised block: cmd
REM mis-parses them, and the error branch ends up running after nmake has
REM already finished. Keep the command on one line and use goto for flow.
if exist "configdata.pm" goto :configured

echo [INFO] configuring VC-WIN64A (static, no apps/tests/docs)
"%PERL%" Configure VC-WIN64A no-shared no-tests no-apps no-docs --prefix="%PREFIX%" --openssldir="%PREFIX%\ssl"
if errorlevel 1 (
  echo [ERROR] Configure failed
  exit /b 1
)
:configured

REM --- build ----------------------------------------------------------------
echo [INFO] nmake build_libs ...
nmake /NOLOGO build_libs
if errorlevel 1 (
  echo [ERROR] nmake build_libs failed
  exit /b 1
)

REM --- stage ----------------------------------------------------------------
if not exist "%OSSL_DIR%\lib" mkdir "%OSSL_DIR%\lib"
if not exist "%OSSL_DIR%\include-msvc\openssl" mkdir "%OSSL_DIR%\include-msvc\openssl"

copy /y "libcrypto.lib" "%OSSL_DIR%\lib\" >nul
if errorlevel 1 ( echo [ERROR] copying libcrypto.lib & exit /b 1 )
copy /y "libssl.lib" "%OSSL_DIR%\lib\" >nul
if errorlevel 1 ( echo [ERROR] copying libssl.lib & exit /b 1 )

REM The generated configuration.h is per-platform and ABI-bearing: it selects
REM SIXTY_FOUR_BIT (Windows long is 32-bit) over SIXTY_FOUR_BIT_LONG, and
REM defines OPENSSL_SYS_WIN64A. The copy under include\ is the Linux one and
REM must NOT be used for MSVC. See README_msvc.md.
copy /y "include\openssl\configuration.h" "%OSSL_DIR%\include-msvc\openssl\" >nul
if errorlevel 1 ( echo [ERROR] copying configuration.h & exit /b 1 )

echo.
echo [OK] staged:
echo      %OSSL_DIR%\lib\libssl.lib
echo      %OSSL_DIR%\lib\libcrypto.lib
echo      %OSSL_DIR%\include-msvc\openssl\configuration.h
