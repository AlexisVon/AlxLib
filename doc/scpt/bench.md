# Script Benchmark — Maintenance Rules

## Test environment

- **Compiler**: g++ C++17 `-O3 -march=native` (library and bench must use the same compiler and options: `CXX=g++ OPT=3 STD=17 ./tools/script_bench_run.sh -f`)
- **CPU**: i3-1315U (Raptor Lake, 2P+4E)
- **OS**: Arch Linux

## Method

- **Interpreter only**: `compile()` once (not timed), `reset()` (not timed), `exec(compiled bytes)` sampled — no lexing/parsing and no engine state reset. Note that every `exec()` call still deserializes the .axp and checks its sha256 (by the engine API's design); that fixed load cost sits inside the sample
- **sleep 5s after the build, then run** (built into script_bench_run.sh): disk IO winds down and cold pages stabilise, otherwise the numbers read high
- **Two-level round sampling**: one round = each of the 12 cases measured once (5 samples per case unit, IQR over them); 3s of sleep between rounds to isolate system state; M rounds in total (5 by default, CLI `--rounds M`), then IQR over the M round values of each case (the top and bottom 25% dropped); the GEOMEAN is computed last
- **Verification**: the script's top-level expression value is compared against the native result or a known constant, and a mismatch reports MISMATCH (a broken bridge, a wrong computation)
- **Anti-optimisation**: volatile loop bounds + asm barrier + noinline stop the compiler from evaluating at compile time; the native side is conservative. **A direct call to a pure function gets eliminated by the compiler** (a measured 0.3ns per call cannot be real) — every native call goes through a **volatile function pointer** (the body stays pure), and the three bridge cases all make a real 100000 calls under that same rule (measured, consistent at ~100us)
- **The ex series measures the bridge only**: the ex bodies are paper-thin (noop / identity) and are plain C++ inside, so their inner performance is not measured; ex registration has to happen before the compile
- **What each comparison means**: ex_call0 = the cost of switching with no data; ex_echo = data crossing the bridge (1 argument in, 1 value back); func_call = a def call of the same shape (the same bare C++ function on the native side)
- **Weighted GEOMEAN — a (full workload)**: `exp(Σ wᵢ·ln rᵢ / Σ wᵢ)`, all 12 cases in (bridges included). What it models: ex is where heavy computation, threads and IO live, every workload execution crosses the bridge, and bridge calls run 1:1 with the workload — the bridge carries 40% (ex_echo 30 + ex_call0 10), glue 45%, control flow/numerics 15%
- **Weighted GEOMEAN — b (lightweight compute scripts)**: the same formula, over the non-ex cases only (ex carries weight 0). What it models: lightweight compute scripts — pure-script tasks that do not lean on ex (small tools, algorithm scripts), where the script side is control flow plus logic glue and calls/computation/loops weigh more — func_call 20%, int_loop 15%, loop control 20%, recursion 15%, data/text 22%
- **The a/b format**: the GEOMEAN prints as `a/b` (full-workload weight / pure-script weight); both weight sets have to sum to 100, checked in code

**Usage weights (a = full workload / b = pure script)**:

| Case | a | b | | Case | a | b |
|------|---|---|-|------|---|---|
| ex_echo | 30% | 0% | | vec_access | 5% | 7% |
| ex_call0 | 10% | 0% | | foreach | 5% | 10% |
| func_call | 15% | 20% | | int_loop | 5% | 15% |
| map_access | 10% | 10% | | while_count | 5% | 10% |
| string_concat | 5% | 5% | | float_arith | 5% | 8% |
| fib(28) | 3% | 8% | | fib_tail | 2% | 7% |

(Set b: from the pure-script view, calls/computation/loops/control flow weigh more — func_call 20, int_loop 15, the loop cases 20, recursion 15, data/text 22)

## Maintenance rules (Claude executes these)

1. **After every bench run**, transcribe the emitted CSV line into one row of the results table; the primary key is date + version — the version the CSV line's commit carried, read from `CMakeLists.txt` at that commit
2. CSV line format: `date,commit,GEOMEAN,<per-case S/N>` — the GEOMEAN (the combined ratio) comes right after the primary key; each cell holds the two raw `script/native` values (us), the ratio is not recorded
3. An old row is **never deleted or modified** — history is history; new data is appended only
4. The results table holds **results only**, one run per row; analysis and conclusions do not go into it
5. A change to the bench code or the engine calls for a re-run and a new row; the version must be the one the measured HEAD carried
6. The environment has to meet the Test environment section's standard (g++ -O3 -march=native), otherwise the results are not comparable; note the difference next to the version when transcribing

## Results table

**Units**: each cell is `script/native` (us); the GEOMEAN is `full-workload weight/pure-script weight` (the a/b format, dimensionless).

**The table starts with the library-wide numbering** (2026-09-24): the earlier per-module numbering is retired, so its measurements are not carried over.

| Date | version | GEOMEAN(a/b) | ex_call0 | ex_echo | func_call | int_loop | fib(28) | fib_tail | while_count | float_arith | vec_access | foreach | map_access | string_concat |
|------|--------|--------------|----------|---------|-----------|----------|---------|----------|-------------|-------------|-----------|---------|------------|---------------|
| 2026-10-17 | 1.3.1 (-O1 lib) | 255/391 | 11671/90 | 21939/93 | 32207/93 | 16353/53 | 381542/814 | 39092/31 | 15516/12 | 17653/86 | 4885/4 | 3859/3 | 2203/67 | 1102/10 |
| 2026-10-17 | 1.2.3 | 211/322 | 9944/92 | 18558/95 | 27174/99 | 13948/54 | 312094/845 | 34920/31 | 12065/13 | 14531/88 | 4215/4 | 3297/3 | 1854/68 | 1001/10 |
| 2026-10-17 | 1.3.1 | 213/325 | 10093/94 | 18648/94 | 27676/96 | 13971/53 | 313674/830 | 34315/32 | 11842/13 | 14389/89 | 4197/4 | 3252/3 | 2025/69 | 1058/10 |
