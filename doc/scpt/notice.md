# alxscpt — Known Limits and Pitfalls

Boundary cases and pitfalls that lie **outside the behaviour contract but do not count as defects**. Each entry gives its trigger and its check.

## Compile time

- **The nesting depth cap defaults to 1024** (`engine_config::parse_depth`, 0 = unlimited): the static entry of `parser::parse` now defaults to 1024 too, where the old implementation passed 0 (unlimited) — source nested deeper than 1024 levels went from "parses" to a compile error. `eval` and dynamic `iload` parse under the host's configured `parse_depth`, the same rule the main source follows. Hitting the cap is **unrecoverable**: one `nesting too deep` is reported and no secondary syntax errors follow it (other errors before the cap are reported as usual).
- **`set_extend` and `set_define` share the `$` namespace**: when the two names collide, or the name is a keyword or reserved word, `set_extend` returns `false` (the registration fails) and nothing is overwritten. Removal is `del_extend` / `del_define`.
- **A `$name` static definition is a compile-time constant fold**: the `set_define` value is folded into a literal node while compiling, and changing the definition at run time does not affect an already compiled product.

## Run time

- **Assignment targets are checked at run time**: `1 = 2;`, `"abc" = 1;`, `$name = 1;` and the like were silently dropped by the old implementation (neither applied nor reported) and now report a run-time `TypeError` (a dynamic `@()` path likewise; `delete`'s target checks at run time too, while a plain name that does not exist still deletes silently with `false`).

- **How coarse an exec checkpoint is**: with `hook_interval = 0` **no** exec event is produced at all; a leaf literal and the `LOAD` shortcut skip the checkpoint; a loop body adds one extra checkpoint per turn (which is why an empty `for(;;) {}` is still interruptible). `hook_info::freq`, and with it the checkpoint count, can be changed by the callback.
- **The interrupt is cooperative and takes effect at a checkpoint only**: both `engine::set_interrupt()` (safe across threads) and a hook callback returning `false` take effect at the next checkpoint; `exec` and `call` share the same exit, so either comes back as `InterruptedError` — the flag itself is cleared at the next entry (an entry clears it as it starts), and the run only ends: root variables and modules survive it; a host native is not interrupted while it runs, so a long-running extension function has to return in pieces of its own accord.
- **A `hook_event::import` / `link` callback returning `false` is an uncatchable interrupt** (`InterruptedError`) that a script-side `catch` cannot see; this differs from the old semantics, where returning `false` refused the import (`ImportError`).
- **The `hook_info::wkdt` read interfaces check nothing**: a stale handle, an out-of-range index or an unknown key is UB (the engine does not check), and they are guaranteed valid during the callback only. A value passed in is always treated as untrusted.
- **`hook_info::desc` is written only when refusing**: one exec keeps the last explanation string written.
- **The tail-call optimisation re-checks by name only**: `op_tcall` confirms once more that the name still resolves to the same `def`, and a name shadowed by a local or a parameter **falls back to an ordinary call** — that recursion layer is then no longer removed (the recursion depth grows as usual).
- **The `delete` boundaries**: `delete x.f` on a non-container value reports `TypeError`; a delete across a module boundary reports `NameError` (consistent with the read path).

## Host interface

- **Classify `engine::exec`'s return by `result::error`**: anything but `NoError` means the script ended in an exception, and what `value` holds is to be read according to `error_type` (do not go by whether `value` is empty alone).
- **`engine::call` acts on root-scope `def`s only**: it calls by name and passes the arguments as values; a name that does not resolve is an error, and no module path is resolved.
- **`compile`'s `_embed` is mutually exclusive with `link`/`env`**: with `_embed = true` a relative import resolves against the importing module's own directory only, an import that resolves to nothing is a compile error, and `link`/`env` are compile errors outright; the key of an embedded module is `"@" + sha16(absolute path + content)`.
- **`_hint` is a host payload the engine does not interpret**: it is stored as given (compressed when `_cmps` is set), and an empty one leaves no `hint` key in the product; `unpack`'s `"hint"` gets the clear text back.
- **`get_csys()` returns a pool-level shared signal**: it outlives any single engine (the engine forwards to the singleton resource pool), and the callback semantics are `(thread tag, message)`.
- **The configuration of `engine::create` can be changed at run time** (`set_max_stack` / `set_parse_depth` / `set_overflow_check` / `set_max_vecfill` / `set_max_yield` / `set_search_paths` / `set_hook` / `set_pipe` / `set_type_ex` / `set_debug_enable`), but a change affects later compiles and execs only — except `set_max_yield`, whose queue capacity is an atomic and becomes visible to the next admission immediately.

## Link module deployment

Dependency resolution belongs to the platform loader, not to the engine. `link` decides only **which file** the script names (absolute path → the instance's `env` list → `set_search_paths` → the path as written, then the suffix-guessing retry `foo` → `foo.so` / `foo.dll`); once the file is chosen, its dependencies follow the loader's own rules. `env()` and `set_search_paths` therefore have no effect on dependencies, and the engine never injects, preloads or edits a process search path.

A load failure is reported on `on_csys` as `link error: <op> failed: <path>: <detail>` and to the script as `Cannot link: <path>: <detail>` (`<op>` is `dlopen` on Linux, `LoadLibrary` on Windows); `<detail>` is the platform loader's own text. A path that does not resolve reports `Cannot link: <path>` with no detail — there is no loader message to append.

### Linux

- An ELF object does not search its own directory, and `DT_RUNPATH` is not transitive: each object must carry its own. The shipped `bin/` DSOs carry `$ORIGIN` (`CMAKE_INSTALL_RPATH`, used in the build tree through `CMAKE_BUILD_WITH_INSTALL_RPATH`).
- A link module that keeps private libraries beside itself needs `RUNPATH=$ORIGIN`; one deployed in a sibling layout (`X/links` with `X/alxlib`) needs `$ORIGIN/../alxlib`.
- A dependency already loaded in the process wins by SONAME over every search path. A module can therefore work on a machine where the build path still exists and fail on a deployment machine — check the RUNPATH, do not trust a development run.
- The engine loads with `RTLD_NOW`: an unresolved symbol fails the load itself, and the failure lands in the detail above. `RTLD_LAZY` would not change dependency location either — the libraries are found at load time regardless.

### Windows

- An import table stores DLL names only — a module cannot declare a relative dependency directory. Two layouts are recommended:
  - **Flat**: the executable and the AlxLib DLLs share one directory. The application directory is searched by the default order and by `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`, so nothing has to be configured.
  - **Shell + body**: the executable imports only system DLLs and the VC runtime, calls `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)` and `AddDllDirectory(<app>\alxlib)` in `main`, then loads the real program from a DLL through an `extern "C"` entry point. The engine's plain `LoadLibrary` then resolves AlxLib dependencies from the registered directory, wherever the link module itself lives.
- Keep the shell on the dynamic CRT (`/MD`): `vcruntime140.dll` and `msvcp140.dll` are resolved before `main`, so they must be app-local or installed by the redistributable.
- The engine loads link modules with plain `LoadLibrary`, which does not search the module's own directory: a module's private DLLs must live in a searched directory (the executable's directory, a registered directory, or System32).
- Entry points must be exported with `__declspec(dllexport)`; `extern "C"` alone exports nothing under MSVC. All four entry points are optional, so a missing export is not an error — the link loads and stays inert, and the failure surfaces at the first call. The contract is x64: on x86 a cdecl export carries a leading underscore and the lookup by plain name fails.
