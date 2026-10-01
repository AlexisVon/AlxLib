# Prebuilt 3rd-party libraries

AlxLib links **seven** third-party libraries statically into `libalxcore` / `libalxcomm`.
None of them is built by this repository's build system: you build each one, put the
result in the shape below, and CMake links it.

This file is the platform-neutral half; `README_msvc.md` covers what MSVC needs beyond it
(the C runtime model and the debug-info format — both fail *silently* if wrong).

## The layout CMake expects

Each library lives under `3rdpty/<name>/`:

```
3rdpty/<name>/lib/<archive>       the static archive that gets linked
3rdpty/<name>/include/<headers>   the headers AlxLib compiles against
```

The exact paths CMake names live in `cmake/alxcore.cmake:35-41` and
`cmake/alxcomm.cmake:41-43`:

| Library | Directory | Archive | Headers |
|---|---|---|---|
| zlib | `zlib/` | `lib/libz.a` | `include/zlib.h`, `include/zconf.h` |
| LZ4 | `lz4/` | `lib/liblz4.a` | `include/lz4.h` |
| zstd | `zstd/` | `lib/libzstd.a` | `include/zstd.h`, `include/zdict.h`, `include/zstd_errors.h` |
| SQLite3 | `sqlite3/` | `lib/libsqlite3.a` | `include/sqlite3.h`, `include/sqlite3ext.h` |
| libjpeg-turbo | `libjpeg-turbo64/` | `lib/libturbojpeg.a` | `include/jpeglib.h`, `include/turbojpeg.h`, `include/jconfig.h`, `include/jmorecfg.h`, `include/jerror.h` |
| PCRE2 | `pcre2/` | `lib/libpcre2-8.a` | `include/pcre2.h` |
| OpenSSL | `openssl/` | `lib/libssl.a`, `lib/libcrypto.a` | `include/openssl/` |

Anything else in those two directories is ignored, so a full upstream install tree can be
dropped in place as long as the names above land where they belong.

## Where to get each one, and how to build it

| Library | Version | Upstream | Build notes |
|---|---|---|---|
| zlib | **1.3.2** | <https://zlib.net/> | `./configure --static` or `cmake -DBUILD_SHARED_LIBS=OFF`; product is `libz.a` |
| LZ4 | **1.10.0** | <https://github.com/lz4/lz4/releases> | build **only `lib/`**. `programs/` is GPL-2.0-or-later and must never be linked; product is `liblz4.a` |
| zstd | **1.5.7** | <https://github.com/facebook/zstd/releases> | build **only `lib/`** (`make -C lib`); product is `libzstd.a` |
| SQLite3 | **3.50.4** | <https://sqlite.org/download.html> — the *amalgamation* | compile `sqlite3.c` on its own; there is no configure step, no other file is needed |
| libjpeg-turbo | **3.1.90** | <https://github.com/libjpeg-turbo/libjpeg-turbo/releases> | `-DENABLE_SHARED=OFF`; AlxLib links `libturbojpeg.a` (not `libjpeg.a`) |
| PCRE2 | **10.48** | <https://github.com/PCRE2Project/pcre2/releases> | 8-bit only, static: `-DPCRE2_BUILD_PCRE2_8=ON -DBUILD_SHARED_LIBS=OFF`; product is `libpcre2-8.a` |
| OpenSSL | **3.6.3** | <https://openssl-library.org/source/> | **has no CMake** — `perl Configure no-shared <target>` then `make`; products are `libssl.a` and `libcrypto.a`. Only needed when `ALXCOMM_TLS=ON` (the default) |

Versions above are the ones this tree is known to build against; they live in the paths and
in `THIRD-PARTY.md` (repo root), never in the CMake — a different patch level usually works.

## The exact archives this tree was built from

| Archive | sha256 |
|---|---|
| `zlib-1.3.2.tar.gz` | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` |
| `lz4-1.10.0.tar.gz` | `676cccc6819442ed1ff94b0ddbc05eea031624e063247128a7746a28ae319dff` |
| `zstd-1.5.7.tar.gz` | `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3` |
| `sqlite3-3.50.4.tar.gz` | `3a80c974c5ce0d93fc771a5f223669e39192a2a271c671ad1fd3ce1889facbf9` |
| `libjpeg-turbo-3.1.90.tar.gz` | `6cd37e8319c2301001e7ea0b3e12be7c66cddf566e96b4db9cac902127875329` |
| `pcre2-10.48.tar.gz` | `ebcc25aadf2a51fa1fefa9b8bc9e7a79b3dae86870a0f1152a22e42befd46888` |
| `openssl-3.6.3.tar.gz` | `243a86649cf6f23eeb6a2ff2456e09e5d77dd9018a54d3d96b0c6bdd6ba6c7f1` |

Put each one at `3rdpty/<name>/<archive>`: that is exactly where the three MSVC rebuild
scripts look for it (`lz4`, `zstd`, `openssl` unpack their own), and they will say so and
stop if it is missing.

## Two rules that fail silently (MSVC)

On MSVC the archives must be built with **`/MD`** and must carry **`/Z7`** debug info, or the
link succeeds and the mismatch shows up later. `README_msvc.md` explains both and records how
each archive is rebuilt. libjpeg-turbo in particular forces `/MT` on itself and has to be
overridden.

## What is in this directory

**Checked in:**

- `README.md` — this file.
- `README_msvc.md` — the MSVC-specific rules and rebuild recipes.
- `<name>/scripts/*.bat` — one rebuild script per library.
- `openssl/include-msvc/openssl/configuration.h` — the MSVC build's OpenSSL config. This is
  **not** an upstream file; it is produced by the OpenSSL build, and the project files put
  this include path **ahead of** `openssl/include/` on purpose (see `README_msvc.md`).

**Not checked in** — `.gitignore`d, supply it yourself:

- `<name>/lib/` — the archives CMake links.
- `<name>/include/` — the headers AlxLib compiles against. Build these yourself rather than
  reusing someone else's: OpenSSL's `configuration.h` is ABI-bearing and platform-specific,
  so a mismatched header set compiles and then corrupts memory.
- `<name>/*.tar.gz` — the upstream source archives (hashes above).
- `<name>/local/` — the unpacked upstream source tree, which the rebuild scripts read.

## Licence

Everything here that comes from upstream stays under its own licence — none of it is covered
by AlxLib's MIT. `THIRD-PARTY.md` in the repository root lists each of the seven with its
licence and the path to its full text.
