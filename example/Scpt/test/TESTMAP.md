# Alexis Script language test coverage matrix

**Date**: 2026-07-22
**Engine version**: example-0.1

## Test layers

| Layer | Directory | Description |
|------|------|------|
| Unit tests | `gtest/` | C++ Google Test, covering the parser/walker/engine API |
| System tests | `cover/` | `.axc` scripts, split by lang-guide dimension, assert style, judged by exit code |
| Smoke tests | `demo/` | `.axc` scripts, end-to-end scenarios, silent on pass |

Run: `cd example/Scpt/test && ./test.sh` (cover + demo)
Unit tests: `cd gtest && ./test.sh`

---

## Overview

| Dimension | Test files | Added / changed |
|------|---------|----------|
| 1. Type system | `cover/types/` 12 files | 5 files reworked from print to assert |
| 2. Containers | `cover/containers/` 7 files | — |
| 3. Type conversion | `cover/conversions/` 2 files | **new directory** |
| 4. Operators | `cover/operators/` 11 files | — |
| 5. Control flow | `cover/control_flow/` 1 file | — |
| 6. Exceptions | `cover/exceptions/` 31 files | — |
| 7. Functions | `cover/functions/` 11 files | — |
| 8. Reflection @ | `cover/reflection/` 10 files | `t09_excall.axc`: @("$xxx")() — the extension-function reflection fast path, plus the static definitions $PI/$vtype/$etype folding and reflecting; `t10_eval.axc`: eval string function (last value / return / params becoming named frame variables / frame-chain closure / nested eval recursive reflection / error catching / TCO inside) |
| 9. Variables and scope | `cover/scope/` 28 files | — |
| 10. Modules and chained access | `cover/modules/` 13 files | — |
| 11. Data types, combined | `cover/data/` 11 files | **new directory** |
| 12. Control flow, combined | `cover/control/` 6 files | **new directory** |
| 13. Extension functions ($xxx) | `cover/excall/` 6 files | `datetime.axc`: $datetime() numeric map fields + snapshot consistency + reflection call |
| **Total** | all of `cover/` plus `demo/` -- `test.sh` prints the running count | |

---

## Test coverage detail

### 1. Type system

| Covered | System test script |
|--------|-------------|
| int: decimal/hex/octal/binary, boundary values, INT64_MIN | `cover/types/test_int.axc` ★ |
| float: plain notation/scientific notation/negative exponent/int vs float distinction | `cover/types/test_float.axc` ★ |
| bool: true/false, separate from int (true==1→TypeError) | `cover/types/test_bool.axc` ★ |
| string: double quotes/single quotes/backtick raw/escapes/\\xNN/# concatenation | `cover/types/test_string.axc` ★ |
| null: default initialisation, ==/!=, ordered comparison→TypeError | `cover/types/test_null.axc` ★ |
| Truthiness: if/while/for/&&/\|\|/!/?: | `cover/types/test_truthy_basic.axc`, `test_truthy_string.axc` |
| explicit bool() conversion: every path | `cover/types/test_bool_conv.axc` |
| type(): all 8 types | `cover/types/test_typefunc.axc` |
| Literal syntax | `cover/types/test_literals.axc` |
| Ternary operator | `cover/types/test_ternary.axc` |
| Boundary values | `cover/types/test_bounds.axc` |

★ = reworked from print style to assert style on 2026-07-22

### 2. Containers

Same as TESTMAP v1, unchanged.

### 3. Type conversion **(new)**

| Covered | System test script |
|--------|-------------|
| string(int, base): base 2/8/10/16, including zero and negatives | `cover/conversions/test_string_overloads.axc` |
| string(double, prec): prec=0/>0/<0 | same file |
| string(fmt, args...): %1/%2/%%, mixed | same file |
| 8×8 conversion matrix: int/float/bool/string/vec/lst/map/null in every direction | `cover/conversions/test_conv_matrix.axc` |
| fmjs failure→null, tojs compact | same file |
| UTF-8 code point encode/decode | same file |
| s1_recur single-element container | same file |

### 4-9. Operators / control flow / exceptions / functions / reflection / scope

Same as TESTMAP v1, no significant change. Regression tests live in `cover/regression/`.

### 12. Data types, combined **(new)**

| Covered | System test script |
|--------|-------------|
| Type checks: int/float/bool/string/null/type() | `cover/data/data_types.axc` |
| Containers: vec/lst/map/fill syntax/index/slice | `cover/data/containers.axc` |
| Operators: arithmetic/comparison/logical/bitwise/concat/ternary/inc-dec/compound assignment | `cover/data/operators.axc` |
| Type conversion: int()/float()/string()/bool()/vec()/map()/lst()/UTF-8 | `cover/data/conversions.axc` |
| Variables: var declaration/assignment/compound assignment/index operations/scope | `cover/data/variables.axc` |

| Smoke tests |
|---------|
| `demo/demo1_types.axc` |
| `demo/demo2_operators.axc` |
| `demo/demo3_containers.axc` |
| `demo/demo4_control_flow.axc` |
| `demo/demo5_functions.axc` |
| `demo/demo6_reflection.axc` |

### 13. Control flow, combined **(new)**

| Covered | System test script |
|--------|-------------|
| Control flow: if/else/for/for-in/while/switch/break/continue | `cover/control/control_flow.axc` |
| Functions: def/default arguments/return value/closure/TCO/argument expansion | `cover/control/functions.axc` |
| Exceptions: try/catch/throw/error types/propagation | `cover/control/exceptions.axc` |
| Reflection: @var read-write/@(expr)/indirect call/container path | `cover/control/reflection.axc` |
| Modules: import/link/dot chain/scope/delete | `cover/control/modules.axc` |
| Call semantics: one last-moment read rule (S1/S2)/error ordering (S4)/writable view/IndexError | `cover/control/args_eval.axc` |

### 10. Modules and chained access

Same as TESTMAP v1. `cover/modules/t8_link_area.axc` covers the link area / delete guard / @ reflection limits.

### 14. Extension functions ($xxx) **(new)**

| Covered | System test script |
|--------|-------------|
| Basic call: $exec call + return value | `cover/excall/basic.axc` |
| Argument passing: $fwrite/$fread | `cover/excall/args.axc` |
| Return types: varmap/string/bool/int | `cover/excall/return.axc` |
| Use in an expression: the exec result joins an operation | `cover/excall/in_expr.axc` |
| Undefined function: compile error (XFAIL) | `cover/excall/error.axc` |

| Smoke tests |
|---------|
| `demo/batch24_excall.axc` |

---

## 2026-08-04 change log

### Engine changes
- **the $ shared-domain system is removed** (var $x / def $f / $f() / wait / wake + thread.os): threads in the script layer violate the design red line (a deadlock can hang the engine, a lifetime race can be triggered into a crash by script input). The script returns to plain single-threaded sequential execution; asynchronous needs are bridged by a host link. See `doc/scpt/design.md` §9, the deprecation record.
- **Opcodes removed**: O_MVAR/O_MDEF/O_MCALL/O_MLOAD/O_MDOT/O_MINDEX/O_MSTORE/O_WAIT/O_WAKE, `s_op_names` kept in step; vtype → 2.
- **Error types removed**: TimeoutError/MultiError.
- **fwrap interface narrowed**: the `is_simult()/simult()/wait()/wake()` virtuals and the `$`-prefixed nload/call paths are gone.

### Test changes
- **Deleted**: `cover/shared/` (5 files), `cover/control/shared.axc`, `demo/batch21_shared.axc`
- **Deleted**: `link/thread.cpp` (the thread.os module)
- **Rewritten**: `demo.axc` — the threaded interactive input becomes a synchronous `$input()`

## 2026-07-23 change log

### Engine changes
- **wait/wake**: the key accepts `@expr` (evaluated at run time, must start with `$`)
- **wait**: the timeout becomes an expression (no longer an integer immediate)
- **wait**: returns `true` (wake) or `false` (timeout), no longer throws TimeoutError
- **WaitCond**: `signaled` is reset after `wait()` to prevent a spin
- **multi::run**: one `variant` argument (a `$func` reference or a string name); invalid input throws
- **on_cerr/on_cmpl/get_csys**: diagnostic/event signals (csys is pool-wide shared, the callback carries a thread id)
- **prtast**: an engine static method that prints a readable AST (auto-detecting source / compiled / packed)
- **s_op_names**: a name array beside the enum, kept in step automatically

### Test changes
- **Updated**: `s03_wait_wake.axc` — the timeout no longer throws, so it tests the return value + @expr key + expression timeout
- **Updated**: `s04_errors.axc` — the TimeoutError test is gone, an @expr validation test is in
- **Fixed**: `parse_wake_stmt` — consumes the `@` so the expression layer cannot take it for an O_ILOAD

## 2026-07-22 change log

### Engine changes
- **Parser**: a bare `wake;` is legal only inside a `wait` body, where it leaves the wait. Outside a `wait` body `wake` still needs `$NAME`.
- **Walker**: `op_wake` with no argument sets the `wait_exit` flag; `op_wait` checks it to leave the loop.
- New: the `m_in_wait` counter (parser) and the `wait_exit` flag (walk_state).

### Test changes
- **Reworked**: `test_int.axc`, `test_float.axc`, `test_bool.axc`, `test_string.axc`, `test_null.axc` from print-based to assert-based, with wider coverage
- **New**: `cover/shared/` — 5 system tests for the $ concurrency
- **New**: `cover/conversions/` — 2 type-conversion tests (string overloads + conversion matrix)
- **Fixed**: `demo/batch21_shared.axc` wait body syntax (`wake $done;` → `wake;`)
- **Cleanup**: the empty directories `cover/x/`, `cover/x2/`, `demo/x/`, `demo/x2/` are gone

## 2026-07-31 change log

### Test changes
- **New**: `cover/data/` — 11 combined data-type test files
- **New**: `cover/control/` — 7 combined control-flow test files
- **New**: `demo/demo_*.axc` — 6 quick-verification demo files
- **New**: a document recording the test problems

### Problems found and design checks

1. **Closure variable capture** (by design)
   - a closure cannot capture the parent function variables when the function reference is returned, so it has to be called inside the parent
   - reason: the `scope_frame` destructor clears the frame variables when the function exits
   - the doc example `return inner()` calls inside, it does not return a reference

2. **Rethrow wraps the exception** (by design)
   - `throw e` re-wraps the whole exception map, so `e.info` becomes a map
   - reason: `e` is itself `map{"what":"RuntimeError","info":value}`, so the whole map goes in as the new exception value on rethrow

3. **Fill syntax** (the doc is right)
   - `[1,2,3:3]` creates `[1, 2, 3, 3, 3]` (5 elements)
   - the fill suffix repeats the last element up to the given count

4. **string(null)** (by design)
   - throws ConvError, unlike `bool(null)` which returns false
   - the doc is explicit: `string(v)` throws ConvError on failure

5. **the %% difference** (by design)
   - `%%` folds to `%` only when there are format arguments
   - `print("100%%")` → `100%%`, `print("%1%%", 42)` → `42%`

---

## 2026-08-03 change log

### Engine changes
- **thread.os exception shape**: the exception `value` becomes a `varmap{what, info}`
  - `what`: the error type name (e.g. `RuntimeError`, `NativeError`, `UnknownError`)
  - `info`: the specific message
  - the script reaches them as `e.what` and `e.info`

### Test changes
- **Updated**: `cover/shared/s05_thread.axc` — 4 exception cases added
  - Pattern E/F: catching a script exception + a native exception (division by zero)
  - Pattern G: an exception thrown inside a thread, checking the MultiError wrapping
  - Pattern H: the case where the exception info is a varmap
- **Updated**: `cover/control/exceptions.axc` — moved to the varmap{what, info} shape
- **Updated**: `demo/demo_control_flow.axc` — the try/catch example follows

### Exception shape

Every exception (script_exception, std::exception, unknown) is wrapped uniformly as:
```
MultiError: varmap{
    "what": string,    // error type: RuntimeError/NameError/TypeError/ConvError/NativeError/UnknownError
    "info": variant    // the specific message (a string or another type)
}
```

From the script:
```
try {
    // ...
} catch (e) {
    $print(e.what);   // "RuntimeError"
    $print(e.info);   // "the specific message"
}
```

---

## 2026-08-03 change log (excall)

### Engine changes
- **Extension functions ($xxx)**: built-ins that can be registered dynamically
  - `T_DOLLAR` + `T_NAME` token, `O_EXCALL` opcode
  - parser: a `$` prefix → a compile-time check against `ext_table`
  - walker: `op_excall` → a call through the `fwrap` interface
- **Version control**: `version` → `etype`(string) + `vtype`(uint_64)
  - the gate is always on (`strict_version` is gone): the etype check is an equality with no exemption; vtype refuses "too new" one way
  - new `error_type::VersionError`
- **Engine API**: `set_extend/del_extend/get_extend/fid_extend`
- **Built-in extensions**: `$fread`, `$fwrite`, `$exec` (registered by the host as needed)

## 2026-08-04 change log (built-in function migration)

- **print/input/tojs/fmjs leave the engine built-ins** → host-registered `$print`/`$input`/`$tojs`/`$fmjs` (implemented in the example main.cpp)
  - lexer: 4 keywords deleted; parser: 4 dispatch branches deleted; op_enum: `O_TOJS/O_FMJS/O_INPUT/O_PRINT` deleted (O_UNPACK/O_HERE move up)
  - **the on_cout signal is gone** (print was its only trigger); on_cerr/on_cmpl/on_csys, the engine diagnostic signals, stay
  - the example host calls `set_vtype(1)` — the opcode layout changed, old .axp files are incompatible
  - gtest: the on_cout/print/tojs/fmjs/input cases are gone (not migrated, the engine no longer provides those functions); the extend mechanism tests stay
- **Built-ins kept**: `int float string bool type vec map lst env here` (language core + path mechanism + compile-time constants)

### Test changes
- **New**: `cover/excall/` — 5 system tests for extension functions
- **New**: `demo/batch24_excall.axc` — the extension-function smoke test
- **Added**: 21 gtest cases (6 lexer + 5 parser + 10 engine)
- **Renumbered**: the demo files uniformly follow `batchN_*.axc` / `demoN_*.axc`

### Syntax highlighting
- VSCode: `extension-functions` pattern
- Vim: `alexisExtension` match

---

### The interrupt hook (2026-08-06)
- **Engine change**: `engine::set_hook(fn, ud, interval)` — one resource-control hook (hook_event{exec, import, link} + hook_info{type, info, desc, hkdt}): **hook = hard firewall (one semantics since 2026-08-06): any event returning false is an interrupt** — an exec checkpoint / the import/link gates (the absolute path after resolve, embed modules pass the same gate) give an uncatchable InterruptedError either way; the reason travels in desc (the engine supplies the entity, written straight through when the hook refuses); new `InterruptedError` (20 values in the enum at the time). An interrupt is uncatchable (the flag short-circuits, plus a fallback at the exec/run_init_walk exit, an in-flight throw does not mask it); a module init interrupted → the fly is marked bad → the reference count falling to zero heals it (after the pool split: the same walker short-circuits and reports, **no cross-engine infection** — a per-engine pool has no chain to spread along). See `doc/scpt/design.md` §4.9
- **Test change**: 10 gtest cases added (`gt_ascript_engine.Hook*`): budget interrupt / in-flight throw / try cannot swallow it / normal completion / recovery after the hook is turned off / module init interrupted + self-heal / import refused (desc delivered) / the gate receives the absolute path after resolve / link refused / recovery after the gate is off

### The module-management pool split (2026-08-06)
- **Engine change**: `res_mng` splits into two layers — **the resource layer res_mng** (a shared singleton: the AST cache + the dlopen handle pool, the only multi-threaded surface, deleted/dlclosed the moment the count reaches zero, emitting ast/link load/unload + link error itself) + **the instance layer mod_mng** (one per engine: the fly pool + a simplified boolean gate + loading_stack cycle detection, single-threaded and lock-free, **silent**). A fly holds its resource reference by construction (taken at birth, returned on leaving the pool); failures are layered: a parse failure has no fly, so a plain retry / an init failure leaves the fly bad and needs a release and a retry. The gate checkpoint moves to the per-engine instantiation point, so dependency closures are isolated by construction.
- **Test change**: the gtest anchor moves from "import load/unload" to "ast/link load/unload" (mod_mng is silent); the infection proof test moves to the same-walker short-circuit semantics; new `GateIsolationPerEngine` (an engine that forbids B initialising a module that depends on B → ImportError); a hot-reload behaviour probe (change the file → release → re-import shows the new behaviour)

## Regression tests

`cover/regression/` — scripts regressing past bugs (5 files), unchanged.

## Conclusion

The Alexis Script engine **passes all 188 tests** (160 cover + 28 demo), built with g++ C++17 -O2. The new combined data-type and control-flow tests fill the coverage blind spots. The thread.os exception shape is now a uniform varmap{what, info}.

Every "problem" found is by design; the engine behaves as the lang-guide describes.
