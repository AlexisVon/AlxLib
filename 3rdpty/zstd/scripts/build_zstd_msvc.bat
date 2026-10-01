@echo off
setlocal
REM ===========================================================================
REM  build_zstd_msvc.bat
REM
REM  Rebuilds the static zstd consumed by AlxCore.
REM
REM      cmd /c 3rdpty\zstd\scripts\build_zstd_msvc.bat
REM
REM  /MD  dynamic CRT, same as every other 3rd-party archive (a /MT archive puts
REM       /DEFAULTLIB:LIBCMT into the image, see 3rdpty\README_msvc.md).
REM  /Z7  CodeView records embedded in the objects rather than a side PDB, so
REM       the staged archive is self-contained: no LNK4099, no lost symbols.
REM
REM  /DZSTD_DISABLE_ASM: the only assembly source upstream ships is the
REM  GNU-syntax decompress\huf_decompress_amd64.S -- cl cannot take it and the
REM  tarball carries no MASM twin. Upstream's own VS projects define this macro
REM  for the same reason; only that huffman path loses its hand-written
REM  version, results are unchanged.
REM
REM  No /DZSTD_MULTITHREAD: the archive must match the Linux one, which is built
REM  single-threaded (no pthread symbols) -- AlxCore is not a threaded codec host.
REM
REM  The file list below is the one the Linux archive carries (checked with
REM  `ar t`), so both platforms link the same code paths.
REM
REM  Verify with:
REM      dumpbin -directives 3rdpty\zstd\lib\zstd_static.lib   (expect MSVCRT)
REM ===========================================================================

for %%I in ("%~dp0..") do set "ZSTD_DIR=%%~fI"

set "SRC_DIR=%ZSTD_DIR%\local\lib"
set "OUT=%ZSTD_DIR%\lib"
REM upstream already owns local\build (its cmake/vs trees), so stage objects elsewhere
set "OBJ=%ZSTD_DIR%\local\obj"
set "TARBALL=%ZSTD_DIR%\zstd-1.5.7.tar.gz"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC_DIR%\zstd.h" (
  if not exist "%TARBALL%" (
    echo [ERROR] %TARBALL% not found
    echo         Upstream archives are no longer kept in this repository.
    echo         See 3rdpty\README.md for the download link and hash, or unpack
    echo         the source tree yourself where this script expects it.
    exit /b 1
  )
  echo [INFO] extracting zstd-1.5.7.tar.gz
  if not exist "%ZSTD_DIR%\local" mkdir "%ZSTD_DIR%\local"
  REM cd first, then pass a relative name: GNU tar reads the colon in
  REM "E:\..." as a remote host spec and fails with "Cannot connect to \E:".
  pushd "%ZSTD_DIR%\local"
  tar -xzf "..\zstd-1.5.7.tar.gz" --strip-components=1
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

echo [INFO] compiling zstd ...
cl -nologo -c -O2 -MD -Z7 -DZSTD_DISABLE_ASM ^
   -I"%SRC_DIR%" -I"%SRC_DIR%\common" -I"%SRC_DIR%\compress" ^
   -I"%SRC_DIR%\decompress" -I"%SRC_DIR%\dictBuilder" -I"%SRC_DIR%\legacy" ^
   "%SRC_DIR%\common\debug.c" "%SRC_DIR%\common\entropy_common.c" ^
   "%SRC_DIR%\common\error_private.c" "%SRC_DIR%\common\fse_decompress.c" ^
   "%SRC_DIR%\common\pool.c" "%SRC_DIR%\common\threading.c" ^
   "%SRC_DIR%\common\xxhash.c" "%SRC_DIR%\common\zstd_common.c" ^
   "%SRC_DIR%\compress\fse_compress.c" "%SRC_DIR%\compress\hist.c" ^
   "%SRC_DIR%\compress\huf_compress.c" "%SRC_DIR%\compress\zstd_compress.c" ^
   "%SRC_DIR%\compress\zstd_compress_literals.c" "%SRC_DIR%\compress\zstd_compress_sequences.c" ^
   "%SRC_DIR%\compress\zstd_compress_superblock.c" "%SRC_DIR%\compress\zstd_double_fast.c" ^
   "%SRC_DIR%\compress\zstd_fast.c" "%SRC_DIR%\compress\zstd_lazy.c" ^
   "%SRC_DIR%\compress\zstd_ldm.c" "%SRC_DIR%\compress\zstd_opt.c" ^
   "%SRC_DIR%\compress\zstd_preSplit.c" "%SRC_DIR%\compress\zstdmt_compress.c" ^
   "%SRC_DIR%\decompress\huf_decompress.c" "%SRC_DIR%\decompress\zstd_ddict.c" ^
   "%SRC_DIR%\decompress\zstd_decompress.c" "%SRC_DIR%\decompress\zstd_decompress_block.c" ^
   "%SRC_DIR%\dictBuilder\cover.c" "%SRC_DIR%\dictBuilder\divsufsort.c" ^
   "%SRC_DIR%\dictBuilder\fastcover.c" "%SRC_DIR%\dictBuilder\zdict.c" ^
   "%SRC_DIR%\legacy\zstd_v05.c" "%SRC_DIR%\legacy\zstd_v06.c" "%SRC_DIR%\legacy\zstd_v07.c"
if errorlevel 1 (
  echo [ERROR] compile failed
  exit /b 1
)

echo [INFO] archiving ...
lib -nologo -out:"%OUT%\zstd_static.lib" *.obj
if errorlevel 1 (
  echo [ERROR] lib failed
  exit /b 1
)

echo.
echo [OK] staged %OUT%\zstd_static.lib
