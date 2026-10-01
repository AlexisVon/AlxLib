@echo off
setlocal
REM ===========================================================================
REM  build_sqlite3_msvc.bat
REM
REM  Rebuilds the static SQLite library consumed by AlxCore with the same CRT
REM  model as the rest of the project.
REM
REM      cmd /c 3rdpty\sqlite3\scripts\build_sqlite3_msvc.bat
REM
REM  Why /MD: the shipped sqlite3.lib was built /MT and therefore carried
REM  /DEFAULTLIB:LIBCMT. Linking that into a /MD target pulls both CRTs into
REM  one image (LNK4098), which risks two copies of CRT state.
REM
REM  Why /Z7: debug info is embedded in the objects rather than left in a side
REM  PDB, so the staged archive is self-contained -- no LNK4099 and no lost
REM  symbols for anyone who links it without the original build tree.
REM
REM  The SQLITE_ENABLE_* set below reproduces the feature symbols present in
REM  the previously shipped library. Verify with:
REM      dumpbin -directives 3rdpty\sqlite3\lib\sqlite3.lib   (expect: no LIBCMT)
REM ===========================================================================

for %%I in ("%~dp0..") do set "SQLITE_DIR=%%~fI"

set "SRC=%SQLITE_DIR%\local\src\sqlite3.c"
set "INC=%SQLITE_DIR%\include"
set "OUT=%SQLITE_DIR%\lib"
set "OBJ=%SQLITE_DIR%\local\build"
set "VCVARS=D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%SRC%" (
  echo [ERROR] amalgamation not found: %SRC%
  echo         extract sqlite3-*.tar.gz into 3rdpty\sqlite3\local\src first
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

echo [INFO] compiling sqlite3.c ...
cl -nologo -c -O2 -MD -Z7 ^
   -DSQLITE_ENABLE_COLUMN_METADATA ^
   -DSQLITE_ENABLE_FTS5 ^
   -DSQLITE_ENABLE_RTREE ^
   -DSQLITE_ENABLE_UNLOCK_NOTIFY ^
   -DSQLITE_ENABLE_DBSTAT_VTAB ^
   -DSQLITE_ENABLE_STMTVTAB ^
   -DSQLITE_THREADSAFE=1 ^
   -I"%INC%" "%SRC%" -Fo:sqlite3.obj
if errorlevel 1 (
  echo [ERROR] compile failed
  exit /b 1
)

echo [INFO] archiving ...
lib -nologo -out:"%OUT%\sqlite3.lib" sqlite3.obj
if errorlevel 1 (
  echo [ERROR] lib failed
  exit /b 1
)

echo.
echo [OK] staged %OUT%\sqlite3.lib
