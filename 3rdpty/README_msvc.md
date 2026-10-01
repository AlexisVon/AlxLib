# Prebuilt 3rd-party libraries — MSVC notes

The MSVC build links these archives statically into `AlxBase.dll` /
`AlxCore.dll` / `AlxComm.dll`. Two things about them are easy to get wrong
silently: the C runtime model, and where the debug info lives. This file
records both rules and how each archive is rebuilt.

## Rule 1: every static archive must be `/MD`

The project's DLLs use `MultiThreadedDLL` (`/MD`). An archive built `/MT` puts
`/DEFAULTLIB:LIBCMT` into its objects, and linking that into a `/MD` target
pulls **both** CRTs into one image. MSVC only warns (`LNK4098`) and picks one,
which can leave two copies of CRT state — separate heaps, `errno`, `FILE*`
tables — in a single process.

Check any archive with:

```bash
MSVC_BIN="$(dirname "$(command -v dumpbin.exe)")"   # after vcvars64 / from a Developer Command Prompt
"$MSVC_BIN/dumpbin.exe" -directives 3rdpty/zlib/lib/zlib.lib | grep -i DEFAULTLIB
```

Expect `MSVCRT` (dynamic) or nothing at all. `LIBCMT` / `libcmt.lib` means the
archive is `/MT` and needs a rebuild.

## Rule 2: every static archive must carry its own debug info (`/Z7`)

A static archive built with `/Zi` leaves its objects pointing at a **side PDB**.
That PDB is not staged next to the archives, so anyone who links them without
this exact build tree gets:

- `LNK4099: PDB not found` — once per affected archive, every single link
- an output PDB with **no symbols** for that library, so a crash inside it shows
  raw addresses instead of file and line

`/Z7` embeds the CodeView records in the objects instead. The archive grows
(roughly 1.5x on top of the code), but it is self-contained: no LNK4099, and
full symbols for anyone who links it.

This matters because AlxLib ships a PDB package. Whatever debug info the
3rd-party archives carry is what ends up in `AlxComm.pdb`.

> Careful: `/Zi` objects still *contain* the PDB filename as a string, so
> grepping an archive for `.pdb` proves nothing either way. Measure the
> `.debug$S` / `.debug$T` section bytes (see the script at the bottom).

## Current state

None of these archives is checked in — `README.md` says where to get each one. The table
records what the build expects to find, and which script produces it.

| Archive | CRT | Debug | Rebuild script |
|---|---|---|---|
| `zlib/lib/zlib.lib` | `MSVCRT` ✓ | `/Z7` ✓ | `zlib/scripts/build_zlib_msvc.bat` |
| `lz4/lib/liblz4_static.lib` | `MSVCRT` ✓ | `/Z7` ✓ | `lz4/scripts/build_lz4_msvc.bat` |
| `zstd/lib/zstd_static.lib` | to be built | to be built | `zstd/scripts/build_zstd_msvc.bat` |
| `sqlite3/lib/sqlite3.lib` | `MSVCRT` ✓ | `/Z7` ✓ | `sqlite3/scripts/build_sqlite3_msvc.bat` |
| `libjpeg-turbo64/lib/jpeg-static.lib` | `MSVCRT` ✓ | `/Z7` ✓ | `libjpeg-turbo64/scripts/build_jpeg_msvc.bat` |
| `libjpeg-turbo64/lib/turbojpeg-static.lib` | `MSVCRT` ✓ | `/Z7` ✓ | `libjpeg-turbo64/scripts/build_jpeg_msvc.bat` |
| `openssl/lib/libssl.lib`, `libcrypto.lib` | none (deferred) | `/Z7` ✓ | `openssl/scripts/build_openssl_msvc.bat` |
| `pcre2/lib/pcre2-8-static.lib` | **missing** | — | **no script** |

Everything except OpenSSL was rebuilt with the scripts above.

**Two archives are missing, so the MSVC build does not link as it stands.**
`AlxCore.vcxproj` names both in `AdditionalDependencies` (search path
`3rdpty\<name>\lib`), and neither file exists ⇒ `LNK1104: cannot open file`
for each:

- **zstd** — `zstd/scripts/build_zstd_msvc.bat` exists but has never been run
  anywhere. Its compile list mirrors the Linux archive and it defines
  `ZSTD_DISABLE_ASM` (the only assembly upstream ships is GNU-syntax `.S`,
  which `cl` cannot take), so run it and check the result.
- **pcre2** — nothing at all: no script, no archive. The Linux side gets the
  same library from `pcre2/lib/libpcre2-8.a`, built 8-bit and static
  (`PCRE2_CODE_UNIT_WIDTH 8` in `source/alxcore/aregex_pcre2.cpp:32`).

The Linux build links `libzstd.a` / `libpcre2-8.a` and is unaffected by either.

### Why OpenSSL has no `MSVCRT` record

OpenSSL hardcodes `/MT` when configured `no-shared`, but pairs it with `/Zl`,
which suppresses the default-library record entirely. The archive therefore
carries **no** CRT reference and defers the choice to whoever links it — so it
is already correct and must not be "fixed" by forcing `/MD` (dropping `/Zl` is
what would actually create the heap split). See
the OpenSSL section below.

Do **not** put `/Zl` into `CMAKE_C_FLAGS` for a CMake-driven library: it applies
to CMake's compiler probe too, and the probe then fails to link
(`unresolved mainCRTStartup`). `/Z7` is safe there — or use
`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`, which is the CMake spelling of
`/Z7`.

### Rebuilding sqlite3 without losing features

`sqlite3.c` is an amalgamation, so the feature set is entirely down to the
`SQLITE_ENABLE_*` defines. The script reproduces the set present in the
previously shipped archive:

```
SQLITE_ENABLE_COLUMN_METADATA  SQLITE_ENABLE_FTS5  SQLITE_ENABLE_RTREE
SQLITE_ENABLE_UNLOCK_NOTIFY    SQLITE_ENABLE_DBSTAT_VTAB  SQLITE_ENABLE_STMTVTAB
```

If you change them, verify by diffing the symbol sets rather than trusting the
build:

```bash
"$MSVC_BIN/dumpbin.exe" -symbols 3rdpty/sqlite3/lib/sqlite3.lib > /tmp/after.txt
# compare against a dump of the previous archive
```

### libjpeg-turbo forces /MT itself

`WITH_CRT_DLL` defaults to `FALSE`, which makes its CMakeLists set
`CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded`. The script passes
`-DWITH_CRT_DLL=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` so it stops
overriding, plus `-DENABLE_SHARED=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF`.
`WITH_JPEG8` stays at its default (`FALSE`, the v6b API — `JPEG_LIB_VERSION 62`
in `jconfig.h`). `WITH_SIMD` is on, which is upstream's default and what the
shipped archive should have had.

A failed configure leaves flags in `local/build/CMakeCache.txt`, so after
changing any CMake variable delete `3rdpty/libjpeg-turbo64/local/build` before
re-running.

### `tar` and drive letters

Extraction steps `pushd` into the target directory and pass a relative archive
name. GNU tar reads the colon in `E:\...` as a remote-host spec and fails with
`Cannot connect to \E: resolve failed`; a relative name avoids it and works with
bsdtar too.

## Checking an archive

```bash
python - <<'PY'
import struct, pathlib
def members(d):
    off = 8
    while off + 60 <= len(d):
        h = d[off:off+60]
        try: size = int(h[48:58].decode().strip())
        except ValueError: break
        yield d[off+60:off+60+size]
        off += 60 + size + (size & 1)
def dbg(o):
    if len(o) < 20: return 0
    n = struct.unpack_from("<H", o, 2)[0]; t = 0
    for i in range(n):
        p = 20 + i*40
        if p+40 > len(o): break
        if o[p:p+8].rstrip(b"\0").startswith(b".debug"):
            t += struct.unpack_from("<I", o, p+16)[0]
    return t
d = pathlib.Path("3rdpty/zlib/lib/zlib.lib").read_bytes()
tot = sum(len(x) for x in members(d)); db = sum(dbg(x) for x in members(d))
print(f"{tot/1048576:.2f} MiB total, {db/1048576:.2f} MiB debug ({db/tot:.1%})")
PY
```

---

## OpenSSL for the MSVC build

How the static OpenSSL used by `AlxComm`'s TLS support is produced, and what to
watch out for when rebuilding it.

Scope: **Windows / MSVC / x64 only.** The Linux and MinGW build systems
(`bash/package.sh`, `cmake/alxcomm.cmake`) are separate and consume different
artifacts — see [Interaction with the Linux build](#interaction-with-the-linux-build).

---

### Artifacts

| Path | What it is |
|---|---|
| `3rdpty/openssl/lib/libssl.lib` | static TLS library (MSVC x64) |
| `3rdpty/openssl/lib/libcrypto.lib` | static crypto library (MSVC x64) |
| `3rdpty/openssl/include-msvc/openssl/configuration.h` | MSVC x64 build config — **ABI-bearing**, see below |
| `3rdpty/openssl/include/` | platform-neutral headers, shared with the Linux build (untouched) |
| `3rdpty/openssl/scripts/build_openssl_msvc.bat` | full reproduction: extract → configure → build → stage |
| `3rdpty/openssl/local/` | gitignored scratch: source tree, objects, intermediate `.pem` |

### Reproduce from scratch

```bash
cmd //c 3rdpty/openssl/scripts/build_openssl_msvc.bat
```

Roughly 15–20 minutes on first run (`nmake` is serial). Re-running reuses the
extracted source and the objects, so it is cheap. The script is idempotent.

Override tool locations with environment variables if your install differs:

```bash
PERL=... NASM=... VCVARS=... cmd //c 3rdpty/openssl/scripts/build_openssl_msvc.bat
```

Defaults: `D:\Strawberry\perl\bin\perl.exe`, `D:\Strawberry\c\bin\nasm.exe`,
`D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat`.

### Manual equivalent

```bat
call "D:\Microsoft Visual Studio\2026\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=D:\Strawberry\c\bin;%PATH%"          & rem nasm
set "NASM=D:\Strawberry\c\bin\nasm.exe"

tar -xzf 3rdpty\openssl\openssl-3.6.3.tar.gz -C 3rdpty\openssl\local
cd 3rdpty\openssl\local\openssl-3.6.3

perl Configure VC-WIN64A no-shared no-tests no-apps no-docs ^
     --prefix=...\local\dist --openssldir=...\local\dist\ssl

nmake /NOLOGO build_libs
```

Then copy `libcrypto.lib` and `libssl.lib` to `3rdpty\openssl\lib\`, and
`include\openssl\configuration.h` to `3rdpty\openssl\include-msvc\openssl\`.

---

### Things that will bite you

#### `configuration.h` is ABI-bearing and platform-specific

This is the single most dangerous file in the set. The copy under
`3rdpty/openssl/include/openssl/` was generated by a **Linux** build and is
wrong for MSVC:

| Macro | Linux copy | MSVC copy | Why it matters |
|---|---|---|---|
| `SIXTY_FOUR_BIT_LONG` | defined | `#undef` | Linux `long` is 64-bit |
| `SIXTY_FOUR_BIT` | `#undef` | defined | Windows `long` is 32-bit |
| `OPENSSL_SYS_WIN64A` | absent | `1` | platform conditionals |

`SIXTY_FOUR_BIT` decides whether `BN_ULONG` is 8 bytes (`long long`) or 4
(`long`). Get it wrong and every struct in OpenSSL has the wrong layout while
still compiling — you get memory corruption, not a build error.

The MSVC overrides are applied by include-path order in `AlxComm.vcxproj`:
`include-msvc` **before** `include`. If that order is ever reversed the wrong
ABI silently wins. Both the project files and this note exist because of that.

Only `configuration.h` actually differs between the two builds; the other 141
headers are byte-identical apart from a `Makefile`/`makefile` comment.

#### `/MT /Zl` static libraries are correct for a `/MD` consumer

The build emits `/MT /Zl` (OpenSSL hardcodes `/MT` when `no-shared`). That looks
like a CRT mismatch against the project's `MultiThreadedDLL` (`/MD`), but it is
deliberate and safe: `/Zl` suppresses the `/DEFAULTLIB` record, so the library
carries **no CRT reference at all** and defers the choice to the final link.

Verified: `dumpbin -directives` on both `.lib` files lists no `DEFAULTLIB`, and
they link into a `/MD` target with no `LNK2038` and no `LNK4098`. Do **not**
"fix" this by rebuilding with `/MD` — dropping `/Zl` is what would actually
cause the heap split.

#### `perl` must be a native Windows perl

The MSYS/Cygwin perl reports `$^O eq "msys"`, which breaks Configure's VC target
detection. Strawberry Perl reports `MSWin32` and works. The script prints the
detected platform; check it says `MSWin32`.

#### `cl` is not resolvable from an MSYS shell

Git Bash cannot resolve `cl` through `PATH` even with the MSVC directory added,
so the whole build runs through `cmd` + `vcvars64.bat`. Don't try to set
`INCLUDE`/`LIB` by hand — the `$MSVC_BIN/../include` form is off by two levels
and yields `bin/HostX64/include`, which then fails on a missing `vcruntime.h`.

#### `--openssldir` is compiled into the binary

`OPENSSLDIR` is baked in as `3rdpty/openssl/local/dist/ssl`. Nothing in the TLS
path needs `openssl.cnf`, so a missing config file is harmless, but the path is
visible in the binary and the directory does not need to ship.

---

### How TLS is wired into the project

`msvc/AlxComm/AlxComm.vcxproj`, **x64 configurations only**. Win32 was removed
from all `msvc/*.vcxproj` and from `AlxLib.slnx`.

```
ClCompile > PreprocessorDefinitions    ALEXIS_USE_TLS=1
ClCompile > AdditionalIncludeDirectories
    $(SolutionDir)..\..\3rdpty\openssl\include-msvc
    $(SolutionDir)..\..\3rdpty\openssl\include
Link > AdditionalLibraryDirectories    $(SolutionDir)..\..\3rdpty\openssl\lib
Link > AdditionalDependencies          libssl.lib;libcrypto.lib;
                                       crypt32.lib;gdi32.lib;advapi32.lib;user32.lib;
```

`libssl` must precede `libcrypto`. `ws2_32.lib` was already present. The system
libraries are the ones `VC-WIN64A` declares in `Configurations/10-main.conf`.

`atransmit.h` has no `ALEXIS_USE_TLS` guards — TLS is selected at runtime by the
type string (`"tls_server"` / `"tls_client"`), so **consumers need no extra
defines and the GTest project needed no TLS changes**.

`ALXCOMM_TLS` already defaults to `ON` in `CMakeLists.txt`; that flag governs
the CMake build, not the `.vcxproj` one.

---

### Verifying

```bash
MSBUILD="$(dirname "$(command -v MSBuild.exe)")/MSBuild.exe"   # after vcvars64
"$MSBUILD" "msvc/AlxComm/AlxComm_GTest.vcxproj" \
    -p:Configuration=Release -p:Platform=x64
cd bin && ./AlxComm_GTest.exe --gtest_filter="gt_atransmit.*"
```

`gt_atransmit.tls_round_trip` performs a real handshake — certificate load,
ECDHE, peer verification against `ca` + `hostname`, and an encrypted round trip
in both directions. `gt_atransmit.tls_client_requires_hostname_with_ca` checks
that pinning `ca` without `hostname` is refused. Both skip themselves if the
build has no TLS.

#### Replacing the test certificate

The certificate and key are embedded as raw string literals in
`gtest/alxcomm/src/gt_atransmit.cpp` (`kCertPem` / `kKeyPem`), generated once on
2026-09-11 and valid 10 years. There is no generator in the tree. If they ever
have to be replaced, these produce drop-in equivalents — same subject, same SAN,
PKCS#8 key form:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out key.sec1.pem
openssl pkcs8 -topk8 -nocrypt -in key.sec1.pem -out key.pem
openssl req -new -x509 -key key.pem -sha256 -days 3650 -out cert.pem \
        -subj "/C=XX/O=AlxLib Test/CN=localhost" \
        -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

Paste both files into the two literals and rebuild AlxComm.

### Interaction with the Linux build

They are independent and do not share artifacts:

| | Windows / MSVC | Linux / MinGW |
|---|---|---|
| headers | `include-msvc` then `include` | `include` |
| libraries | `lib/*.lib` | `lib/*.a` (gitignored, built per machine) |
| build | `msvc/*.vcxproj` | `cmake` + `bash/package.sh` |

`include/openssl/configuration.h` is the Linux variant and is left untouched so
the CMake path keeps working. Committing that file for the Linux side and
overriding it for MSVC is what the `include-msvc` split is for.

---

### Debug info

OpenSSL's VC targets emit `/Zi /Fdossl_static.pdb` by default. That leaves the
objects pointing at an external PDB which is **not** staged next to the
archives, so linking them produces `LNK4099` and an `AlxComm.pdb` with no
OpenSSL symbols.

The build script rewrites that line to `/Z7`, so the CodeView records live in
the objects and the archives are self-contained. Sizes:

| | `/Zi` (upstream default) | `/Z7` (what the script produces) |
|---|---|---|
| `libcrypto.lib` | 47.3 MiB | 56.9 MiB (59% debug) |
| `libssl.lib` | 8.9 MiB | 12.3 MiB (62% debug) |

The output PDB actually gets *smaller* (40.1 → 26.4 MiB) because the linker can
deduplicate per-object records instead of merging a shared type stream.

### Known issues

- **`libcrypto.lib` is 57 MB**, most of it debug info. Other 3rd-party libs in
  this repo are committed as `.lib`, so this follows convention, but it is a
  large binary addition to git. It buys file-and-line symbols for crashes
  inside OpenSSL in the shipped PDB.
- **`AlxScpt_GTest` cannot build on Windows.** Six test sources
  (`gt_ascript_compile/context/lex/parse/utils/walk.cpp`) drive internals such as
  `script::parser`, `script::walker` and `script::res_mng`, which are not
  exported from `AlxScpt.dll` — on Linux the `.so` exports everything, so they
  link there and fail here. They are also Linux-only in other ways
  (`gt_ascript_compile.cpp` hardcodes `/tmp/`). Accepted: those tests stay out
  of the MSVC project.
