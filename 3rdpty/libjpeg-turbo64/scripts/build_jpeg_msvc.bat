@echo off
setlocal
REM ===========================================================================
REM  build_jpeg_msvc.bat
REM
REM  Rebuilds the static libjpeg-turbo libraries consumed by AlxCore with the
REM  same CRT model as the rest of the project.
REM
REM      cmd /c 3rdpty\libjpeg-turbo64\scripts\build_jpeg_msvc.bat
REM
REM  Why: the shipped turbojpeg-static.lib / jpeg-static.lib were built /MT,
REM  so they carry /DEFAULTLIB:LIBCMT. Linking them into a /MD target pulls
REM  both CRTs into one image (LNK4098), which risks two copies of CRT state.
REM
REM  libjpeg-turbo forces /MT by default: WITH_CRT_DLL=FALSE sets
REM  CMAKE_MSVC_RUNTIME_LIBRARY to MultiThreaded. WITH_CRT_DLL=ON stops it from
REM  overriding the runtime, and the explicit MultiThreadedDLL gives /MD -- so
REM  the archives carry /DEFAULTLIB:MSVCRT, same as the zlib and lz4 archives.
REM
REM  Note: do NOT add /Zl to CMAKE_C_FLAGS here. It applies to CMake's compiler
REM  probe too, and the probe executable then links without any CRT
REM  (unresolved mainCRTStartup). /Zl is only needed for libraries that are
REM  forced to /MT, which is the OpenSSL case, not this one.
REM
REM  CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded is the CMake spelling of /Z7:
REM  the CodeView records go into the objects instead of a side PDB, so the
REM  staged archives are self-contained and linking them never yields LNK4099.
REM
REM  Verify with:
REM      dumpbin -directives 3rdpty\libjpeg-turbo64\lib\turbojpeg-static.lib
REM ===========================================================================

for %%I in ("%~dp0..") do set "JPEG_DIR=%%~fI"

set "SRC_DIR=%JPEG_DIR%\local\libjpeg-turbo-3.1.90"
set "BUILD_DIR=%JPEG_DIR%\local\build"
set "CMAKE=D:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA=D:\Qt\Tools\Ninja\ninja.exe"
set "NASM=D:\Strawberry\c\bin\nasm.exe"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC_DIR%\CMakeLists.txt" (
  echo [ERROR] source not found: %SRC_DIR%
  echo         extract libjpeg-turbo-*.tar.gz into 3rdpty\libjpeg-turbo64\local first
  exit /b 1
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
  -DCMAKE_ASM_NASM_COMPILER="%NASM%" ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded ^
  -DWITH_CRT_DLL=ON ^
  -DENABLE_SHARED=OFF ^
  -DENABLE_STATIC=ON ^
  -DWITH_TURBOJPEG=ON ^
  -DWITH_TOOLS=OFF ^
  -DWITH_TESTS=OFF
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
if not exist "%JPEG_DIR%\lib" mkdir "%JPEG_DIR%\lib"
copy /y "%BUILD_DIR%\jpeg-static.lib" "%JPEG_DIR%\lib\" >nul
if errorlevel 1 ( echo [ERROR] jpeg-static.lib not produced & exit /b 1 )
copy /y "%BUILD_DIR%\turbojpeg-static.lib" "%JPEG_DIR%\lib\" >nul
if errorlevel 1 ( echo [ERROR] turbojpeg-static.lib not produced & exit /b 1 )
copy /y "%BUILD_DIR%\jconfig.h" "%JPEG_DIR%\include\" >nul
if errorlevel 1 ( echo [ERROR] jconfig.h not produced & exit /b 1 )

echo.
echo [OK] staged %JPEG_DIR%\lib\jpeg-static.lib, turbojpeg-static.lib and include\jconfig.h
