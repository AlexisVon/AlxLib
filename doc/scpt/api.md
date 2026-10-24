# alxscpt — API Manual (C++ side)

This is for the **host developer**: how to embed the script engine in a C++ program, register extension capabilities, write a link module, install hooks and channels. The language's own syntax and the engine's internals are in the matching entries of the README document index.

- Header: `include/alxscpt/ascript.h` (the only public header)
- Library: `libalxscpt.so` (or the merged static library `alxlib.a`)
- Namespace: `alx::script`

```bash
g++ -std=c++17 -I include -I include/alxbase -I include/alxcore -I include/alxscpt main.cpp \
    -L bin -lalxscpt -lalxcore -lalxbase -Wl,-rpath,$PWD/bin
```

---

## 1. Minimal embedding

```cpp
#include "ascript.h"

alx::script::engine* eng = alx::script::engine::create();
eng->on_cerr.connect([](const std::string& _s) { fprintf(stderr, "%s\n", _s.c_str()); });

alx::script::engine::result rst = eng->exec("demo.axc");
if (rst.error != alx::script::error_type::NoError) {
    fprintf(stderr, "run failed: %s\n", alx::script::error_type_name(rst.error));
} else {
    // rst.value is the value of the script's last statement
}

delete eng;    // the caller deletes it (engine is not a singleton)
```

The engine ships with **no IO and no extension functions**: `print`, file access and the like are all registered by the host (§5 extension functions, §11 link modules). The engine itself keeps only its diagnostic signals.

---

## 2. Lifecycle and configuration

```cpp
alx::script::engine_config cfg;
cfg.etype       = "";          // compatibility tag stamped into every product (see §7)
cfg.vtype       = 0;           // extension counter stamped into every product, 0 = gate off
cfg.max_stack   = 1024;        // live frames allowed, 0 = unlimited
cfg.max_vecfill = 0;           // elements one vec/lst fill may produce, 0 = unlimited
cfg.parse_depth = 1024;        // syntax nesting depth of one parse, 0 = unlimited
cfg.overflow_check = false;    // integer overflow check
cfg.max_yield   = 256;         // deliveries the yield queue may hold, 0 = unlimited

alx::script::engine* eng = alx::script::engine::create(cfg);
eng->config();                 // read the configuration back

// runtime adjustment (affects later compiles and execs only — set_max_yield is the exception:
// deliveries admitted from then on see the new capacity immediately)
eng->set_max_stack(...);  eng->set_parse_depth(...);  eng->set_max_vecfill(...);
eng->set_max_yield(...);  eng->set_overflow_check(...);  eng->set_search_paths({"mods", "lib"});
eng->reset();                  // drop the intermediate state of a run: the root store is rebuilt and loaded modules are released with it (hot reload)
```

`reset()` is also the only reaper: a run that fails during execution leaves root variables, defs and loaded modules standing — and with them the parsed ASTs behind the defs — so a long-lived engine holds that memory until an explicit `reset()`.

`engine` is **single-threaded and not reentrant**: one instance runs on one thread at a time. Only four interfaces are meant to cross threads: `running()` and `set_interrupt()` (§10), `yield()` (§3.2b), and `set_max_yield()` (its admission capacity is atomic and synced immediately).

---

## 3. Execution, calls and compilation

### 3.1 Execution

```cpp
alx::script::engine::result exec(const bytes_view& _data, const std::string& _home_dir);
alx::script::engine::result exec(const std::string& _file_path);   // home_dir becomes the file's own directory
```

The three fields of `result`:

| Field | Meaning |
|---|---|
| `value` | the last statement's value (not a valid result when `error != NoError`) |
| `error` | `error_type`; `NoError` means the run finished normally |
| `elapsed_us` | wall time, in microseconds; 0 for failures that precede the walk (a parse error, a version-gate refusal, a corrupt product) |

`home_dir` is **the base relative imports resolve against**, and the anchor of the module namespace.

Only retainable data may leave `exec()`: a result that holds an object handle — an `anyptr` (a module instance, a host object) or a container holding one — is refused; `error` comes back as `TypeError` (`exec result: <type> cannot be returned`, the name `type()` reports) and the value is released. The gate reports through `result` alone — no `on_cerr` line. An `exec()` value is safe to keep indefinitely — its lifetime is the host's.

### 3.2 Calling a script function

```cpp
alx::script::engine::result call(const std::string& _name, const varvec& _args);
alx::script::engine::result call(const std::string& _name, varvec&& _args);   // moves on acceptance
```

Calls the `def` of that name in the **root scope** (no module path is resolved), passes the arguments as values, and returns the same shape as `exec`. The rvalue overload takes the arguments over only once the name resolves to a def — both refusal paths (`function not found`, `not a def function`) leave the caller's vector untouched.

Unlike `exec()`, `call()` has **no result gate** — a value holding an object handle comes back whole: the unrestricted channel for complex C++ & script work (dynamic object construction and the like). Its contract flips: such a value references the engine's world and must be dropped before the engine dies — and before an explicit `reset()`, which rebuilds the world the handle's instance lives in (a failed run no longer drops it). The same holds for handles read back through `load()`, whose pointer is additionally invalidated by any root-variable addition and by any reset.

### 3.2b Delivery into a run — `engine::yield`

```cpp
std::future<alx::script::result> yield(const std::string& _name, const varvec& _args,
                                       uint_32 _priority = 0, uint_32 _expiry_ms = 0);
std::future<alx::script::result> yield(const std::string& _name, varvec&& _args,
                                       uint_32 _priority = 0, uint_32 _expiry_ms = 0);
void set_max_yield(size_t _n);   // writes engine_config::max_yield
```

Queues a call to run at the script's next `yield()` (`lang.md` §9.4) — `call()` serialized onto the running timeline. Before this interface, host code could only call into a script between runs; a delivery lets it run *inside* one, in the window the script itself opens. What runs there is **script**: the delivery names a root-scope def and the engine calls it like any other def — no arbitrary C++ is injected. The point is the reverse channel — the C++ layer's native threads hand calls to the script's timeline, the requests wait in the queue, and they execute only when the script yields; a script in a `yield(ms)` window is not blocking them, it is servicing them as they arrive. Callable from any thread; the thread that runs the script must never block on a future it queued itself (its synchronous tool is `fwrap::call`).

- **accepted only while a run is in progress and the queue is below `max_yield`**; otherwise the future comes back already fulfilled with `YieldError` (`no main fiber`, `yield full`) and nothing was moved — the rvalue overload hands the arguments over only on acceptance
- the name is resolved when the delivery is serviced, in the root scope like `call()`: a delivery may name a def defined later, and a miss comes back as `NameError` inside the future
- `_priority`: higher runs first, equal priorities keep their arrival order; `_expiry_ms` is the longest the delivery may wait in the queue — an **expired delivery never runs**: the take point settles one it finds expired exactly, and one still waiting in the queue is settled by the background pass (precision: about one 50 ms beat)
- **every outcome is inside the future** — the value, a script error, `YieldError` (`expired`, `not executed` when the run ends before a yield point reached it, `nested yield` when a serviced call calls `yield()` and does not catch the script-level error), or `InterruptedError` when an interrupt tears the run down (the hook's reject reason, when there is one, is the value); a future is never broken — the only ways out are an allocation failure while settling and a host slot that breaks the signal contract (§3.5), both outside the guarantee
- the result is shaped like `call()`'s and, like `call()`'s, has no exec result gate — a value holding an object handle must be dropped before the engine dies or `reset()` runs
- the serviced call runs in root posture: root variables and modules only, `max_stack` as its own root budget, hook events and interrupts still firing (the watchdog covers it)
- `set_max_yield` shrinks without evicting what is already queued

### 3.3 Compiling into a product

```cpp
bytes compile(const bytes_view& _data, const std::string& _home_dir,
              bool _cmps = false, bool _embed = false, const bytes_view& _hint = bytes_view()) const;
bytes compile(const std::string& _path, bool _cmps = false, bool _embed = false,
              const std::string& _hint = std::string()) const;
```

- `_cmps`: compress the payload.
- `_embed`: resolve and **inline** every `import` into one self-contained product; `link` and `env` are compile errors then, a relative import resolves against the importing module's own directory only, and an import that resolves to nothing is a compile error too. The key of an embedded module is `"@" + sha16(absolute path + content)`.
- `_hint`: the host payload. The `compile(source, ...)` overload **stores it as given and never interprets it**; the `compile(path, ...)` overload reads it as a **file path** (`source/alxscpt/ascript.cpp:425`), so passing raw JSON yields an empty hint. An empty one leaves no `hint` key in the product (backward compatible).

### 3.4 Reading a product back

```cpp
static bool unpack(const bytes_view& _axp, varmap& _out);   // key → dump
static std::string prtast(const bytes_view& _data);         // AST text
static bytes prtfmt(const bytes_view& _data);               // lexical-level reformat
static std::string prtinf(const bytes_view& _data);         // metadata (everything but the AST)
```

The keys `unpack` puts into `_out`: `"ast"` (AST dump), `"modules"` (module key → dump), `"info"` (a subset of the metadata), `"hint"` (the host payload in clear).

### 3.5 Diagnostic signals

| Signal | Parameter | Fires on |
|---|---|---|
| `on_cerr` | `const std::string&` | engine diagnostics (runtime error messages and the like) |
| `on_cmpl` | `const compile_error&` | a compile error (with row, col and byte offset) |
| `get_csys()` | `signal<uint_64, const std::string&>&` | pool-level events (module load/unload, link failures and the like); the callback gets (thread id, message). The yield service's background pass adds `heartbeat overrun: …` / `heartbeat error: …` — the only entries emitted from the pool's own thread (every other entry comes from whichever engine thread produced it, so a slot must tolerate another thread and must not block) |

`get_csys()` returns a reference to a **pool-level shared signal** that outlives any single engine, so it is safe to keep across engines.

**Every slot on every signal is a text notification, nothing more** — `on_cerr`, `on_cmpl` and `on_csys` alike. A slot runs on whichever thread emitted the entry (an `on_csys` entry from the yield service's background pass arrives on that thread), and the engine's own thread may be executing script while it runs, so a slot may only take the text (format it, or hand it off and return): it must not block, **must not throw**, must not create or destroy engines, and must not re-enter the engine in any other way. The engine does not defend against a slot that breaks this.

---

## 4. Data exchange

```cpp
alx::variant* load(const std::string& _name, bool _auto_create = false);   // read/write a root variable (usable outside a run too)
```

- A script value is an `alx::variant` (scalars, `bytes`, `std::string`, `varmap`/`varvec`/`varlst`, `anyptr`).
- On the script side integers are all `long long` semantics and floating point is `double`; a host reads one back under `to<T>()`'s **lossless conversion** rule, and a type that does not fit gives the default value.
- An object handed in from C++ travels in an `anyptr` (see `wrap<T>` in §11).

### 4.1 Types the script layer has no name for `set_type_ex`

```cpp
using type_ex = const char* (*)(const alx::variant& _v, void* _ud);

void set_type_ex(type_ex _fn, void* _ud = nullptr);
```

`type()` has two dead-end answers: an object a host handed over, and a variant type the script layer does not model (a 32-bit float, an unsigned integer, a `std::vector<T>`). Both go to this callback when one is installed — null or an empty answer means the host does not know it either, and `"unknown"` stands, which is also the answer with no callback at all; passing null removes the callback.

The callback runs on the thread that executes the script, and the engine is not reentrant. The pointer it returns is read once, right after the call, and never kept — its storage has to stay valid past the return. A C++ exception it throws is reported like a native's: a script `try` can catch it as `NativeError`, and without one the run comes back as `NativeError`. The diagnostic messages that name a value in a conversion (`"Expected int, got X"` and the like) never ask the callback and keep saying `"unknown"` — the `exec()` result gate does ask it, so its message carries the same name `type()` reports.

---

## 5. Extension functions `$name`

An extension function is the language superset a host injects: the script writes `$foo(args)`, the parser checks the table for it while compiling, and the walker calls the C++ function the host registered.

```cpp
bool set_extend(const std::string& _name, native_func _handler);   // native_func = void (*)(fwrap&)
void del_extend(const std::string& _name);
native_func get_extend(const std::string& _name) const;
bool fid_extend(const std::string& _name) const;

eng->set_extend("print", [](alx::script::fwrap& _fw) {
    std::string out;
    for (size_t i = 0; i < _fw.size(); ++i) out += _fw[i].to_string();
    printf("%s\n", out.c_str());
    _fw.freturn();      // no return value
});
```

- Name conflict: when the name is a script keyword or reserved word, `set_extend` returns `false` (the registration is refused).
- The `$` prefix is **lexical-level hard isolation** — a script author cannot define `$xxx` (an identifier carries no `$`), so no name the host registers can collide with a script user function.
- **Argument passing**: only a bare variable name travels by reference (it points at the variable's slot); an index, a member, a literal or an expression is evaluated into a copy first. A native that modifies in place (`$vec_push` and its kind) therefore gets a copy when handed `m["a"]`, and the change never lands back in the container — to modify in place, use a path assignment: `m["a"][null] = 7`.
- **Thread safety**: an extension function can be entered concurrently, so it has to be thread-safe by itself (no shared state, or the host's own lock).

### 5.1 The `fwrap` interface

| Member | Purpose |
|---|---|
| `size()` / `operator[](i)` | read an argument (`variant&`; a bare variable name aliases the script's variable, any other argument is a temporary; **dead once the call returns**; throws IndexError past the last argument) |
| `freturn(v)` / `freturn()` | set the return value |
| `raise(info, error_type)` | throw an exception the script can catch |
| `call(func, args)` | call a script function from the native (`func` may be a function name, a `.` path, or a callable value); the path lookup is read-only — a path that does not resolve leaves nothing behind |
| `iload(key)` | resolve a name or `.` path in the calling scope to a **slot address or nothing** — the script-level read semantics (a `[null]` size, an element's value) do not apply here, and a shape that is not an address (a slice, an out-of-range index, a byte element, a bare name that is not there) is simply `nullptr`; the promise covers the resolved outcome — a `.` path whose intermediate step does not resolve still reports that step's error (`Undefined: <name>`, a missing map key). **The host's one slot handle, in write posture**: it hands back a writable slot, so a missing terminal map key (and a `[null]` append position) is created — write into the returned slot to bind a value; the pointer follows the engine-side `load()` rule — invalidated by any addition to the store and by any reset |
| `bind(name, func, area)` | register a native function into an area (what a link module uses); an empty `area` registers into the call's plain data scope, and an existing name is replaced either way |
| `load/store/remove(name)` | reach the call's data scope: the calling link instance's own store, or the current entity's store for an extension handler (where `load`/`store` are misuse — use `iload`, `lang.md` §12.4); a returned pointer follows the same rule (any addition to the store, any reset) |
| `object()` / `unwrap<T>()` | the C++ object bound to the current area — **area natives only**: outside an area dispatch (an extend handler, a load/unload callback, a `bind` with no area) no area is set, and calling it there is outside the contract — nothing is set to hand back |
| `config()` | read the engine configuration (the pipe pointers included) |
| `currying(name)` | mint a **delivery handle** (`fwrap::callback*`) naming a def — each `cb->call(args, priority, expiry)` is `engine::yield` under that name (§3.2b), the native's channel to its worker threads. **The caller owns the handle and deletes it**, before the engine dies (using one afterwards is UB). A load/unload entry point has no engine to name, so calling it there throws `LinkError` |

---

## 6. Static definitions `$name`

```cpp
bool set_define(const std::string& _name, const variant& _value);
void del_define(const std::string& _name);
alx::variant get_define(const std::string& _name) const;
bool fid_define(const std::string& _name) const;
```

A host constant read as `$name` (**no parentheses**). It shares the `$` namespace with the extension functions (the two are mutually exclusive by name; a conflicting registration returns `false`). The value is **folded into a literal while compiling**, which leaves the product self-contained with no run-time lookup; a misspelling is a compile error rather than a run-time empty read.

---

## 7. Version gate

`etype` / `vtype` are the first two fields of `engine_config`: read them back through `config()`, set them at creation or with the two setters below the `config()` accessor.

```cpp
void set_etype(const std::string& _type);   // engine major version (a string)
void set_vtype(uint_64 _ver);               // version of the supported extension set (a number)
```

- A compiled product carries the etype/vtype of its compile; the check on the way in is **always on** (it cannot be turned off). A plain engine that set neither skips it.
- `etype` matches exactly: a product with no tag, or one that differs from the engine's, gives `VersionError`.
- `vtype` is checked one way only: a product whose vtype is greater than the engine's is refused as too new. Removing or changing an extension is a breaking change and calls for bumping `etype`; only an added extension bumps `vtype`.

---

## 8. The hook `set_hook`

```cpp
void set_hook(hook_fn _fn, void* _ud, uint_64 _interval);
// hook_fn = bool (*)(hook_info&); returning false = interrupt the run
```

| `hook_event` | Fires on | `info` holds | Effect of returning `false` |
|---|---|---|---|
| `exec` | an instruction checkpoint | checkpoints fired so far | an interrupt (`InterruptedError`) |
| `import` | a module load about to happen | the resolved path | an **uncatchable interrupt** |
| `link` | a dynamic library load about to happen | the resolved path | an **uncatchable interrupt** |
| `trap` | a script called `trap(...)` | `{here: posmap, args?}` | an interrupt |
| `debug` | once per statement (needs `set_debug_enable(true)`) | empty — the position goes through `wkdt_pos()` | an interrupt |

- `_interval` is the checkpoint interval; `0` means **no `exec` events at all**. A callback may rewrite the interval that follows through `hook_info::freq`. `import`/`link`/`trap`/`debug` are **not limited by the interval**.
- The `debug` event arrives once per statement: a host that does not care should filter on `info.type` and return on the callback's first line. A hook installed with `debug_enable` on means one callback per statement (negligible in volume, but worth knowing).
- `hook_info::desc` is a writable string the engine owns; **write it only when refusing** (one exec keeps the last explanation written).
- `hook_info::hkdt` is the host data passed to `set_hook`, handed back unchanged on every event.

### 8.1 The execution view `wkdt`

A callback may observe the current execution state read-only (frames, entities, links, testing what a value is). **The engine does not check arguments**: a stale handle, an out-of-range index or an unknown key is UB, and all of it is valid during the callback only.

```cpp
uint_64 wkfm_size();                        // frames alive, 0 = at the top level
std::string wkfm_func(uint64 i);            // frame name ("eval" for an eval frame, "?" for a block or loop frame)
std::vector<std::string> wkfm_data_keys(uint64 i);
const variant* wkfm_data_cptr(uint64 i, const char* key);

const void* wken_of(const variant* v);      // value -> entity handle
const void* wken_root();
std::vector<std::string> wken_data_keys(const void* e);
const variant* wken_data_cptr(const void* e, const char* key);
std::string wken_file(const void* e);  std::string wken_name(const void* e);
const std::string& wken_path(const void* e);  // the resolved path itself by reference (no copy), empty for the root
const void* wken_pptr(const void* e);

const void* wklk_of(const variant* v);      // value -> link handle
std::vector<std::string> wklk_data_keys(const void* l);
const variant* wklk_data_cptr(const void* l, const char* key);
std::vector<std::string> wklk_area_funs(const void* l, const char* area);

bool wkis_ent(const variant* v);   bool wkis_link(const variant* v);
bool wkis_func(const variant* v);  bool wkis_area(const variant* v);

varmap wkdt_pos();                    // the statement being executed {row, col, ofst, file}; row = 0 = the position is unknown
uint_32 wkdt_row();  uint_32 wkdt_col();  // the same row / column without allocating the varmap
std::list<varmap> wkdt_fpos();        // the statement chain {row, col, ofst, file, func}, innermost first and the top level last
                                      // func: the function name / "eval" / "?" for a nameless layer (a block or loop layer with a position is listed too)

const engine_config& wkconfig();
```

### 8.2 Row-level position `set_debug_enable`

```cpp
void set_debug_enable(bool _on);      // false by default; after set_interrupt in the vtable
```

- **One switch, two effects**: at parse time it decides whether position markers go in, at exec time whether positions are recorded and `debug` events fire. The semantics match `-g`: turning it off **suppresses**, it is not an error.
- **With it on**: a marker is inserted before every statement (a static annotation; the AST's shape is unchanged); at run time a frame keeps one slot only (**the top one is the current position**) and **no history**.
- When an uncaught error is brought out through `on_cerr`, each trace line is `[file] func:row:col` (the suffix is omitted when the row is unknown); the per-layer positions are what `wkdt_fpos()` returns.
- `row = 0` means **unknown** (and is normal): that is what an AST without markers gives (an `.axp` someone else compiled, or one from the shared cache).
- The granularity is the **statement**: no marker goes inside an expression, and the row at a call site is **the start of the statement the call sits in** (a stable anchor); a one-line `if (c) f();` is fully accurate, a multi-line bare body (the single-statement body of an `if`/`while`) falls back to the `if` line — the row of the carrying statement **itself** and of the **condition evaluation** is still accurate.
- With it off the behaviour is exactly the old build's with no position markers (instruction budget included: a marker consumes no checkpoint).

---

## 9. Host IO channels `set_pipe`

```cpp
using pipe_out = void (*)(const std::string* _out, void* _ud);   // engine -> host output
using pipe_in  = std::string (*)(void* _ud);                     // host -> engine input

void set_pipe(pipe_in _in, pipe_out _out, void* _in_ud = nullptr, void* _out_ud = nullptr);
```

The channels are per-engine; with none set (`nullptr`) the host's default is stdio. An extension function or a link module gets the two pointers from `fwrap::config()` and builds `$print` / `$input` and the like on them.

---

## 10. Interrupt

```cpp
bool running() const;        // true while an entry runs: exec() or call()
void set_interrupt();        // ask for an interrupt (callable from any thread)
```

The interrupt is **cooperative**: once the request is raised it takes effect at the next checkpoint of the execution flow (the run comes back as `InterruptedError`). Every entry that runs a walk reports it the same way — `exec` and `call` alike; a request made while nothing is running is discarded (an entry clears the flag as it starts), and a run that fails during execution — an error or an interrupt — **only ends that run**: root variables and loaded modules survive it, and `reset()` is the only thing that drops them. The checkpoints are governed by `set_hook`'s `_interval`, but **the flag is looked at on every checkpoint** — an interval of 0 only turns `exec` events off, `set_interrupt()` still works. `set_interrupt()` also wakes a `yield()` residency wait (§3.2b), so an interrupt lands promptly even while the script is sleeping there.

---

## 11. Link module development

A link module is a dynamic library that gives a script three layers: **a namespace, its functions, and object instances**.

### 11.1 Entry symbols

| Symbol | Signature | When |
|---|---|---|
| `alexis_script_load` | `void (*)(fwrap&)` | library level: once per engine's link, at load (the dlopen is shared process-wide) |
| `alexis_script_unload` | `void (*)(fwrap&)` | library level: once per engine's link, at unload (before the shared `dlclose`) |
| `alexis_script_create` | `void (*)(fwrap&)` | instance level: for every link instance created |
| `alexis_script_release` | `void (*)(fwrap&)` | instance level: when an instance is destroyed |

Constant names: `LINK_LOAD` / `LINK_UNLOAD` / `LINK_CREATE` / `LINK_RELEASE`; function type aliases `script_load` / `script_unload` / `script_create` / `script_release`.

```cpp
extern "C" ALXSCPT_API void alexis_script_load(alx::script::fwrap& _fw) {
    _fw.bind("abs", fn_abs, "mt");          // a static area: the function is registered directly
    _fw.bind("mkdir", fn_mkdir, "fs");
}
```

### 11.2 The two area kinds

**A static area** (registered at load time, no C++ object) — the script side is `b.mt.abs(-5)`, `b.fs.mkdir("d")`.

**A dynamic area instance** (a C++ object bound at run time):

```cpp
_fw.wrap<alx::file>(new alx::file(), "x", {
    {"read",  fn_read},
    {"write", fn_write},
    {"close", fn_close},
});
```

`wrap<T>` does two things: it stores `anyptr_ex<T>::make(_obj)` in the data slot keyed by the area name, and `bind`s each method into that area. When the script calls `b.x.read()`, the engine sets the area to `x` first, and `_fw.unwrap<alx::file>()` inside the native hands that C++ object back.

**`anyptr_ex<T>::make` splits on copyability**: a copyable type gets a copier; a `noncopyable` type (such as `alx::file`) takes the non-copyable version.

### 11.3 Callback conventions

```cpp
void fn_read(alx::script::fwrap& _fw) {
    alx::file* f = _fw.unwrap<alx::file>();
    if (nullptr == f) return _fw.raise("no file instance");
    _fw.freturn(alx::variant(f->read(...).to_string()));
}
```

- Arguments come from `_fw[i]`, the return value goes out through `_fw.freturn(...)`.
- On failure `_fw.raise(msg, error_type)` throws an exception the **script can catch**; a host exception works too — the engine wraps it into `NativeError`. The library's own allocation failures (`std::bad_alloc` / `std::length_error`) are not wrapped into `NativeError`: they are translated into the script-side `MemoryError` (a script `try` can catch that).
- An argument is a **pointer view** that is dead once `call()` returns; copy it if you need to keep it.

### 11.4 Lifecycle

| Stage | What happens |
|---|---|
| Creation | `wrap<T>()` in `create` (stores the anyptr and binds the methods) |
| Call | the engine sets the area → `object()` gives the anyptr → `unwrap<T>()` |
| Rebuild | `remove(area)` runs the anyptr's destructor (the C++ object dies) → `wrap<T>()` again |
| Instance deleted | the area's natives and data slot are cleared, the anyptr's destructor deletes the C++ object |
| Namespace deleted | only the area's natives are cleared; instances already created are untouched |
| Shutdown | **the link instances are destroyed first, `dlclose` after** — the anyptr's destructor lives in the dynamic library and has to be called before the unload |

A link instance's private data slot is invisible to the script (reading, writing and deleting all throw `TypeError`), so the script reaches it only indirectly, through the functions registered in the area.

---

## 12. Error handling

```cpp
struct script_exception { error_type type; std::string info; };
```

A script-side `throw`, a host-side `raise` and an engine-internal failure all land in `error_type`:

| Class | Values |
|---|---|
| Normal | `NoError` |
| Runtime | `RuntimeError`, `TypeError`, `NameError`, `ConvError`, `IndexError`, `KeyError`, `ArgError`, `NavError` |
| Arithmetic | `DivZeroError`, `DivOverflowError`, `OverflowError`, `ShiftError` |
| Resource | `StackError`, `ResourceError`, `MemoryError` |
| Compile | `ParseError` |
| Module | `ImportError`, `LinkError`, `VersionError` |
| Host | `NativeError` (a wrapped C++ exception), `InterruptedError` (an engine interrupt, uncatchable) |
| Delivery | `YieldError` — `no main fiber` / `yield full` / `expired` / `not executed` / `nested yield` arrive inside a delivery's future; a `yield()` during module init throws it into the script and is catchable. A delivery torn down by an interrupt reports `InterruptedError` (the interrupt's, not a `YieldError`) |
| Fallback | `UnknownError` |

- A script-side `try/catch` catches only what is catchable; the engine's `InterruptedError` (a hook returning `false` in §8, or `set_interrupt()`), **passes through the script's catch**. A native that raises `InterruptedError` itself through `fwrap::raise` without the interrupt flag is just an ordinary failed call — that one is catchable.
- For a string, use `error_type_name(type)`.
- A compile error goes through `compile_error` instead (`msg` / `loc.row` / `loc.col` / `loc.index`), reported through `on_cmpl`.
- An **uncaught error** is reported through `on_cerr`: the first line is `Uncaught: [type] msg`, and after it the folded call stack (one `[file] func:row:col` per line, a recursion folded into `(Nx)`). The row and col come from the position markers of §8.2 — **with the switch off or an AST without markers there is no row/col** (the trace keeps its old shape).
- **A module init failure**: `info` = `in module <path>: <type>: <msg>` plus that module's **internal** folded trace (its row numbers are readable).
