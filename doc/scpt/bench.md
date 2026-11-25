# Script Benchmark — Maintenance Rules

## Test environment

- **Compiler**: g++ C++17 `-O3 -march=native` (library and bench must use the same compiler and options: `CXX=g++ OPT=3 STD=17 ./tools/script_bench_run.sh -f`)
- **CPU**: i3-1315U (Raptor Lake, 2P+4E)
- **OS**: Arch Linux

## Method

- **Interpreter only**: `compile()` once (not timed), `reset()` (not timed), `exec(compiled bytes)` sampled — no lexing/parsing and no engine state reset. Note that every `exec()` call still deserializes the .axp and checks its sha256 (by the engine API's design); that fixed load cost sits inside the sample
- **sleep 5s after the build, then run** (built into script_bench_run.sh): disk IO winds down and cold pages stabilise, otherwise the numbers read high
- **Two-level round sampling**: one round = each of the 17 cases measured once (5 samples per case unit, IQR over them); 3s of sleep between rounds to isolate system state; M rounds in total (5 by default, CLI `--rounds M`), then IQR over the M round values of each case (the top and bottom 25% dropped); the GEOMEAN is computed last
- **Verification**: the script's top-level expression value is compared against the native result or a known constant, and a mismatch reports MISMATCH (a broken bridge, a wrong computation)
- **Anti-optimisation**: volatile loop bounds + asm barrier + noinline stop the compiler from evaluating at compile time; the native side is conservative. **A direct call to a pure function gets eliminated by the compiler** (a measured 0.3ns per call cannot be real) — every native call goes through a **volatile function pointer** (the body stays pure), and the three bridge cases all make a real 100000 calls under that same rule (measured, consistent at ~100us)
- **The ex series measures the bridge only**: the ex bodies are paper-thin (noop / identity) and are plain C++ inside, so their inner performance is not measured; ex registration has to happen before the compile
- **What each comparison means**: ex_call0 = the cost of switching with no data; ex_echo = data crossing the bridge (1 argument in, 1 value back); func_call = a def call of the same shape (the same bare C++ function on the native side)
- **Table-scale cases (added 2026-11-24)**: five generated cases cover shapes a module actually has — `ext_route64`/`ext_route256` (that many `$extend` registrations, the hot loop calls the last one, the native side does one table find plus an indirect call per iteration), `def_route64` (one module, 64 defs, the hot loop calls the last one), `locals16` (16 locals per activation), `roots64` (64 root variables, the loop reads/writes eight of them). Their scripts and the native tables are built at startup; the per-case `setup` (extend registration + native tables) runs before the compile and before every timed block, and is excluded from the timing.
- **Weighted GEOMEAN**: `exp(Σ wᵢ·ln rᵢ / Σ wᵢ)` over all 17 cases. What it models: a mixed architecture with the C++ layer dominant — the host does the heavy work in C++ (threads, IO, libraries) and the script is the glue, which is the only reason the architecture exists. The bridge is the defining cost — ex_echo 20 + ex_call0 10 = 30; extension routing back into C++ is frequent — ext_route64 6 + ext_route256 6 = 12; a script-defined dispatch — def_route64 6; script glue — func_call 10, map_access 8, string_concat 4, vec_access 4, foreach 4, int_loop 6, while_count 4, float_arith 4, fib 2, fib_tail 2 = 48; the variable-model diagnostics — locals16 2 + roots64 2 = 4. Raw weights are percentages; 100 points, checked in code.
- **The GEOMEAN column**: one weighted number. **Weights v2 (2026-11-25)**: the raw values are the percentages and the normalization is the identity; rows measured under v1 (dual a/b weights: base 100 plus 30 appended table points, raw 130) keep their place as history and are not comparable to v2 rows. The dual-weight scheme is retired: a pure-script-dominant scenario is not what this architecture is for

**Usage weights (a = full workload / b = pure script)**:

| Case | weight | | Case | weight |
|------|--------|-|------|--------|
| ex_echo | 20% | | vec_access | 4% |
| ex_call0 | 10% | | foreach | 4% |
| func_call | 10% | | int_loop | 6% |
| map_access | 8% | | while_count | 4% |
| string_concat | 4% | | float_arith | 4% |
| fib(28) | 2% | | fib_tail | 2% |
| ext_route64 | 6% | | ext_route256 | 6% |
| def_route64 | 6% | | locals16 | 2% |
| roots64 | 2% | | | |

(v2: the mixed architecture with the C++ layer dominant -- the bridge and extension routing carry the weight, because that is where the heavy work lives)

## Maintenance rules (Claude executes these)

1. **After every bench run**, transcribe the emitted CSV line into one row of the results table; the primary key is the version the CSV line's commit carried, read from `CMakeLists.txt` at that commit
2. CSV line format: `date,commit,GEOMEAN,<per-case S/N>` — the GEOMEAN (the combined ratio) comes right after the primary key; each cell holds the two raw `script/native` values (us), the ratio is not recorded
3. An old row is **never deleted or modified** — history is history; new data is appended only
4. The results table holds **results only**, one run per row; analysis and conclusions do not go into it
5. A change to the bench code or the engine calls for a re-run and a new row; the version must be the one the measured HEAD carried
6. The environment has to meet the Test environment section's standard (g++ -O3 -march=native), otherwise the results are not comparable; note the difference next to the version when transcribing

## Results table

**Units**: each cell is `script/native` (us); the GEOMEAN is the weighted ratio (dimensionless, one number since weights v2).

**The table starts with the library-wide numbering**: the earlier per-module numbering is retired, so its measurements are not carried over.

| version | GEOMEAN | ex_call0 | ex_echo | func_call | int_loop | fib(28) | fib_tail | while_count | float_arith | vec_access | foreach | map_access | string_concat | ext_route64 | ext_route256 | def_route64 | locals16 | roots64 |
|--------|--------------|----------|---------|-----------|----------|---------|----------|-------------|-------------|-----------|---------|------------|---------------|-------------|--------------|------------|----------|---------|
| 1.3.1 (-O1 lib) | 255/391 | 11671/90 | 21939/93 | 32207/93 | 16353/53 | 381542/814 | 39092/31 | 15516/12 | 17653/86 | 4885/4 | 3859/3 | 2203/67 | 1102/10 |
| 1.2.3 | 211/322 | 9944/92 | 18558/95 | 27174/99 | 13948/54 | 312094/845 | 34920/31 | 12065/13 | 14531/88 | 4215/4 | 3297/3 | 1854/68 | 1001/10 |
| 1.2.3 (frames ptr) | 207/316 | 9824/90 | 18186/95 | 26981/98 | 13945/54 | 306140/828 | 33995/31 | 11769/13 | 14578/89 | 4156/5 | 3240/3 | 1849/72 | 984/10 |
| 1.3.1 | 213/325 | 10093/94 | 18648/94 | 27676/96 | 13971/53 | 313674/830 | 34315/32 | 11842/13 | 14389/89 | 4197/4 | 3252/3 | 2025/69 | 1058/10 |
| 1.3.2 | 219/338 | 10405/96 | 18634/95 | 27666/95 | 14374/52 | 316696/844 | 35184/32 | 12181/13 | 15197/89 | 4356/4 | 3350/3 | 2242/68 | 1081/10 |
| 1.3.2 | 206/311 | 9729/92 | 18462/91 | 27210/94 | 13864/51 | 266274/829 | 34086/31 | 11851/13 | 14446/89 | 4238/4 | 3235/3 | 2161/109 | 1044/10 |
| 1.4.1 | 206/311 | 9948/92 | 18673/93 | 27518/95 | 13856/54 | 268905/835 | 34535/31 | 11973/13 | 15160/88 | 4263/4 | 3283/3 | 2097/108 | 1041/11 | | | | | |
| 1.7.0 | 170 | 10133/94 | 19764/96 | 29308/93 | 14808/54 | 267941/812 | 34003/30 | 12102/12 | 14474/87 | 4332/4 | 3326/3 | 1991/71 | 1071/10 | 25050/777 | 25174/557 | 34837/662 | 62248/73 | 10642/18 |
| 1.7.0 (win-14600KF) | 176 | 8360/76 | 21324/76 | 28504/76 | 11313/36 | 288194/912 | 33234/19 | 10085/10 | 12038/66 | 3763/9 | 3150/2 | 1846/60 | 1486/15 | 24310/769 | 24254/812 | 32238/946 | 85496/57 | 9796/23 |

Earlier rows predate the five table-scale columns (added 2026-11-24); their cells are empty and are not back-filled. Rows up to 1.6.x carry the dual a/b GEOMEAN; from 1.7.0 the column is the single weighted number (weights v2).
