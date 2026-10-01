/*****************************************************************/ /**
 * \file   script_bench.cpp
 * \brief  Script engine benchmark: bridge cost (script<->native) + interpreter ratios
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ascript.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace alx;
using namespace std::chrono;

#ifndef GIT_COMMIT
#    define GIT_COMMIT "?"
#endif

static std::string rep(const char* _s, int _n) {
    std::string r;
    r.reserve((size_t) _n * 3);
    for (int i = 0; i < _n; ++i) r += _s;
    return r;
}

static double now_us() {
    static auto t0 = high_resolution_clock::now();
    return (double) duration_cast<nanoseconds>(high_resolution_clock::now() - t0)
               .count() /
           1000.0;
}

static void ext_noop(script::fwrap&) {}

static void ext_echo(script::fwrap& fw) {
    int_64 x = fw[0].to<int_64>(0);
    fw.freturn(variant(x));
}

static double iqr_mean(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    size_t lo = v.size() / 4;
    size_t hi = v.size() - v.size() / 4;
    double s = 0.0;
    size_t c = 0;
    for (size_t i = lo; i < hi; ++i) {
        s += v[i];
        ++c;
    }
    return c ? s / (double) c : 0.0;
}

struct BenchCase {
    const char* name;
    const char* script;
    double (*native)();
    int_64 expected;
    bool check;
    double tol;
    double weight;
    double weight_s;
};

static volatile int_64 N_EX = 100000;
static volatile int_64 N_INT = 50000;
static volatile int_64 N_FIB = 28;
static volatile int_64 N_FIB_TAIL = 100000;
static volatile int_64 N_STR = 2000;
static volatile int_64 N_VEC = 10000;
static volatile int_64 N_MAPKEY = 100;
static volatile int_64 N_MAPGET = 5000;
static volatile int_64 N_WHILE = 100000;
static volatile int_64 N_FLOAT = 50000;
static volatile int_64 sink;

__attribute__((noinline)) static int_64 native_echo(int_64 x) { return x; }
__attribute__((noinline)) static void native_noop() {}

static int_64 (*volatile g_echo)(int_64) = native_echo;
static void (*volatile g_noop)() = native_noop;

static double native_ex_call0() {
    int_64 n = N_EX;
    for (int_64 i = 0; i < n; ++i) {
        g_noop();
        asm volatile("" ::: "memory");
    }
    return 0;
}

static double native_ex_echo() {
    int_64 n = N_EX, x = 0;
    for (int_64 i = 0; i < n; ++i) {
        x = g_echo(i);
        asm volatile("" : "+r"(x));
    }
    sink = x;
    return (double) x;
}

static double native_int_loop() {
    int_64 sum = 0;
    int_64 n = N_INT;
    for (int_64 i = 0; i < n; ++i) {
        sum += i * 3 - i / 2 + i % 7;
        asm volatile("" : "+r"(sum));
    }
    sink = sum;
    return (double) sum;
}

__attribute__((noinline)) static int_64 fib_func(int_64 n) {
    if (n <= 1) return n;
    return fib_func(n - 1) + fib_func(n - 2);
}

static double native_fib() {
    int_64 r = fib_func(N_FIB);
    sink = r;
    return (double) r;
}

__attribute__((noinline)) static double native_fib_tail() {
    int_64 n = N_FIB_TAIL;
    int_64 a = 0, b = 1;
    while (n > 1) {
        int_64 t = a + b;
        a = b;
        b = t;
        n--;
        asm volatile("" : "+r"(b));
    }
    int_64 r = n <= 0 ? 0 : b;
    sink = r;
    return (double) r;
}

static double native_while_count() {
    int_64 n = N_WHILE;
    while (n > 0) {
        n = n - 1;
        asm volatile("" : "+r"(n));
    }
    sink = n;
    return (double) n;
}

static double native_float_arith() {
    double f = 1.0, sum = 0.0;
    int_64 n = N_FLOAT;
    for (int_64 i = 0; i < n; ++i) {
        f = f * 1.0001 + 0.5;
        sum += f;
        asm volatile("" : "+r"(sum));
    }
    sink = (int_64) sum;
    return sum;
}

static double native_vec_access() {
    int_64 n = N_VEC;
    int_64* raw = new int_64[n];
    volatile int_64* v = raw;
    for (int_64 i = 0; i < n; ++i) v[i] = i;
    int_64 sum = 0;
    for (int_64 i = 0; i < n; ++i) sum += v[i];
    delete[] raw;
    sink = sum;
    return (double) sum;
}

static double native_foreach() {
    int_64 n = N_VEC, sum = 0;
    for (int_64 i = 0; i < n; ++i) {
        sum += i;
        asm volatile("" : "+r"(sum));
    }
    sink = sum;
    return (double) sum;
}

static double native_map_access() {
    int_64 nkey = N_MAPKEY, nget = N_MAPGET;
    std::unordered_map<std::string, int_64> m;
    for (int_64 i = 0; i < nkey; ++i) m[std::to_string(i)] = i * 2;
    int_64 sum = 0;
    for (int_64 i = 0; i < nget; ++i) {
        sum += m[std::to_string(i % nkey)];
        asm volatile("" : "+r"(sum));
    }
    sink = sum;
    return (double) sum;
}

static double native_string_concat() {
    std::string s;
    int n = (int) N_STR;
    for (int i = 0; i < n; ++i) {
        s += "a";
        s += std::to_string(i % 10);
    }
    sink = (int_64) s.size();
    return (double) s.size();
}

static const char* S_EX_CALL0 = R"(
for (var i = 0; i < 100000; ++i) {
    $ex_noop();
}
)";

static const char* S_EX_ECHO = R"(
var x = 0;
for (var i = 0; i < 100000; ++i) {
    x = $ex_echo(i);
}
x;
)";

static const char* S_FUNC_CALL = R"(
def echo(x) { return x; }
var x = 0;
for (var i = 0; i < 100000; ++i) {
    x = echo(i);
}
x;
)";

static const char* S_INT_LOOP = R"(
var sum = 0;
for (var i = 0; i < 50000; ++i) {
    sum = sum + i * 3 - i / 2 + i % 7;
}
sum;
)";

static const char* S_FIB = R"(
def fib(n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}
fib(28);
)";

static const char* S_FIB_TAIL = R"(
def fib_tail(n, a, b) {
    if (n <= 1) { return b; }
    return fib_tail(n - 1, b, a + b);
}
fib_tail(100000, 0, 1);
)";

static const char* S_WHILE_COUNT = R"(
var n = 100000;
while (n > 0) { n = n - 1; }
n;
)";

static const char* S_FLOAT_ARITH = R"(
var f = 1.0;
var sum = 0.0;
for (var i = 0; i < 50000; ++i) {
    f = f * 1.0001 + 0.5;
    sum = sum + f;
}
sum;
)";

static const char* S_VEC_ACCESS = R"(
var v = [0:10000];
for (var i = 0; i < 10000; ++i) {
    v[i] = i;
}
var sum = 0;
for (var i = 0; i < 10000; ++i) {
    sum = sum + v[i];
}
sum;
)";

static const char* S_FOREACH = R"(
var v = [0:10000];   // [val:N] = fill N copies of val — fill then assign
for (var i = 0; i < 10000; ++i) { v[i] = i; }
var sum = 0;
for (var x : v) {
    sum = sum + x;
}
sum;
)";

static const char* S_MAP_ACCESS = R"(
var m = map{};
for (var i = 0; i < 100; ++i) {
    m[string(i)] = i * 2;
}
var sum = 0;
for (var i = 0; i < 5000; ++i) {
    sum = sum + m[string(i % 100)];
}
sum;
)";

static const char* S_STRING_CONCAT = R"(
var s = "";
for (var i = 0; i < 2000; ++i) {
    s = s + "a" + string(i % 10);
}
s;
)";

static BenchCase benches[] = {

    {"ex_call0", S_EX_CALL0, native_ex_call0, 0, false, 0.0, 10, 0},
    {"ex_echo", S_EX_ECHO, native_ex_echo, 99999, true, 0.0, 30, 0},
    {"func_call", S_FUNC_CALL, native_ex_echo, 99999, true, 0.0, 15, 20},

    {"int_loop", S_INT_LOOP, native_int_loop, 0, true, 0.0, 5, 15},
    {"fib(28)", S_FIB, native_fib, 317811, true, 0.0, 3, 8},
    {"fib_tail", S_FIB_TAIL, native_fib_tail, 0, false, 0.0, 2, 7},
    {"while_count", S_WHILE_COUNT, native_while_count, 0, true, 0.0, 5, 10},
    {"float_arith", S_FLOAT_ARITH, native_float_arith, 0, true, 1e-9, 5, 8},
    {"vec_access", S_VEC_ACCESS, native_vec_access, 49995000, true, 0.0, 5, 7},
    {"foreach", S_FOREACH, native_foreach, 49995000, true, 0.0, 5, 10},
    {"map_access", S_MAP_ACCESS, native_map_access, 495000, true, 0.0, 10, 10},
    {"string_concat", S_STRING_CONCAT, native_string_concat, 4000, true, 0.0, 5, 5},
};

static bool verify_case(const BenchCase& b, const variant& sv, double nv) {
    if (!b.check) return true;
    if (b.tol > 0) {
        double s = sv.to_number();
        return std::fabs(s - nv) <= b.tol * std::max(1.0, std::fabs(nv));
    }
    if (sv.is<std::string>())
        return sv.to<std::string>().size() == (size_t) b.expected &&
               (int_64) nv == b.expected;

    int_64 s = (int_64) sv.to_number();
    return s == (int_64) nv && (b.expected == 0 || s == b.expected);
}

static void prep_run(script::engine* eng) {
    eng->reset();
    eng->set_overflow_check(false);
    eng->set_max_stack(100000);
    eng->set_max_vecfill(200000);
}

int main(int argc, char** argv) {

    int rounds = 5;
    constexpr int N_PER = 5;
    std::vector<std::string> filter;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rounds" && i + 1 < argc) {
            rounds = atoi(argv[++i]);
            if (rounds < 3) rounds = 3;
        } else if (a == "--help") {
            printf("usage: script_bench [case...] [--runs N]\n");
            printf("cases: ");
            for (auto& b : benches) printf("%s ", b.name);
            printf("\n");
            return 0;
        } else {
            filter.push_back(a);
        }
    }

    {
        double wsum = 0.0, wsum_s = 0.0;
        for (auto& b : benches) {
            wsum += b.weight;
            wsum_s += b.weight_s;
        }
        if (std::fabs(wsum - 100.0) > 1e-9 ||
            std::fabs(wsum_s - 100.0) > 1e-9) {
            fprintf(stderr,
                    "FATAL: bench weights sum to %.1f (all) / %.1f (script), "
                    "expected 100\n",
                    wsum, wsum_s);
            return 1;
        }
    }

    constexpr int NAME_W = 17, S_W = 10, N_W = 10, R_W = 9;

    constexpr int W = NAME_W + 3 + S_W + 3 + N_W + 3 + R_W + 2;
    const std::string H = "╔" + rep("═", W) + "╗\n";
    const std::string S = "╠" + rep("═", W) + "╣\n";
    const std::string F = "╚" + rep("═", W) + "╝\n";
    const char* HDR = "║ %-17s │ %10s │ %10s │ %9s ║\n";
    const char* ROW = "║ %-17s │ %10s │ %10s │ %8.0fx ║\n";
    const char* ERR = "║ %-17s │ %10s │ %10s │ %9s ║\n";

    std::string table;
    auto appendf = [&table](const char* fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        table += buf;
    };
    appendf("%s", H.c_str());
    appendf("%s", "║  Alexis Script Benchmark — bridge + interpreter");
    appendf("%*s%s", (int) (W - 2 - 46), "", "║\n");
    appendf("%s", S.c_str());
    appendf(HDR, "Benchmark", "Script", "Native", "Ratio");
    appendf("%s", S.c_str());

    script::engine* eng = script::engine::create();

    eng->set_extend("ex_noop", ext_noop);
    eng->set_extend("ex_echo", ext_echo);

    int passed = 0;
    int total = (int) (sizeof(benches) / sizeof(benches[0]));

    std::vector<std::pair<double, double>> wr_all, wr_script;
    bool has_geomean = false;
    double ga_all = 0.0, gb_script = 0.0;

    std::vector<std::tuple<const char*, double, double>> csv;

    struct Compiled {
        const BenchCase* b;
        bytes bin;
    };
    std::vector<Compiled> cases;
    for (auto& b : benches) {
        if (!filter.empty() &&
            std::find(filter.begin(), filter.end(), b.name) == filter.end())
            continue;
        bytes src(b.script);
        bytes bin = eng->compile(bytes_view(src), ".", false, false);
        if (bin.empty()) {
            appendf(ERR, b.name, "COMPILE", "FAIL");
            continue;
        }
        cases.push_back({&b, std::move(bin)});
    }

    std::vector<std::vector<double>> s_rounds(cases.size()), n_rounds(cases.size());
    std::vector<script::engine::result> last(cases.size());
    std::vector<double> nv_last(cases.size());
    std::vector<bool> ok(cases.size(), true);

    for (int r = 0; r < rounds; ++r) {
        if (r > 0) sleep(3);
        fprintf(stderr, "[%d/%d] round\n", r + 1, rounds);
        for (size_t i = 0; i < cases.size(); ++i) {
            if (!ok[i]) continue;
            auto& b = *cases[i].b;

            std::vector<double> su;
            for (int k = 0; k < N_PER && ok[i]; ++k) {
                prep_run(eng);
                double t0 = now_us();
                last[i] = eng->exec(bytes_view(cases[i].bin), ".");
                double t1 = now_us();
                if (last[i].error != script::error_type::NoError) {
                    appendf(ERR, b.name, "ERROR", "-");
                    ok[i] = false;
                } else {
                    su.push_back(t1 - t0);
                }
            }
            if (!ok[i]) continue;
            s_rounds[i].push_back(iqr_mean(su));

            std::vector<double> nu;
            double nv = 0;
            for (int k = 0; k < N_PER; ++k) {
                double t0 = now_us();
                nv = b.native();
                double t1 = now_us();
                nu.push_back(t1 - t0);
            }
            nv_last[i] = nv;
            n_rounds[i].push_back(iqr_mean(nu));
        }
    }

    for (size_t i = 0; i < cases.size(); ++i) {
        if (!ok[i]) continue;
        auto& b = *cases[i].b;
        double script_us = iqr_mean(s_rounds[i]);
        double native_us = iqr_mean(n_rounds[i]);

        if (!verify_case(b, last[i].value, nv_last[i])) {
            appendf(ERR, b.name, "-", "MISMATCH");
            continue;
        }

        double ratio = native_us > 0.1 ? script_us / native_us : 0.0;
        char s_buf[16], n_buf[16];
        if (script_us > 1000.0)
            snprintf(s_buf, sizeof(s_buf), "%.1f ms", script_us / 1000.0);
        else
            snprintf(s_buf, sizeof(s_buf), "%.0f us", script_us);
        if (native_us > 1000.0)
            snprintf(n_buf, sizeof(n_buf), "%.2f ms", native_us / 1000.0);
        else
            snprintf(n_buf, sizeof(n_buf), "%.1f us", native_us);

        appendf(ROW, b.name, s_buf, n_buf, ratio);
        csv.push_back({b.name, script_us, native_us});
        passed++;
        wr_all.push_back({b.weight, ratio});

        wr_script.push_back({b.weight_s, ratio});
    }

    appendf("%s", S.c_str());

    if (!wr_all.empty()) {
        auto weighted = [](const std::vector<std::pair<double, double>>& v) {
            double wsum = 0.0, log_sum = 0.0;
            for (auto& [w, r] : v) {
                wsum += w;
                log_sum += w * std::log(r);
            }
            return wsum > 0 ? std::exp(log_sum / wsum) : 0.0;
        };
        double ga = weighted(wr_all);
        double gb = wr_script.empty() ? 0.0 : weighted(wr_script);
        char g_buf[32];
        snprintf(g_buf, sizeof(g_buf), "%.0f/%.0f", ga, gb);
        appendf(ERR, "GEOMEAN(a/b)", "-", "-", g_buf);
        has_geomean = true;
        ga_all = ga;
        gb_script = gb;
    }
    appendf("%s", F.c_str());

    printf("%s", table.c_str());

    char dbuf[16];
    time_t now = time(nullptr);
    strftime(dbuf, sizeof(dbuf), "%Y-%m-%d", localtime(&now));
    printf("\nCSV:%s,%s", dbuf, GIT_COMMIT);
    if (has_geomean) printf(",%.0f/%.0f", ga_all, gb_script);
    for (auto& c : csv)
        printf(",%.0f/%.0f", std::get<1>(c), std::get<2>(c));
    printf("\n");

    printf("%d/%d benchmarks passed (3 bridge + 9 interp). %d rounds x %d "
           "samples, IQR over round values, 3s sleep between rounds, compile "
           "and reset excluded from timing, verify on.\n",
           passed, total, rounds, N_PER);

    delete eng;
    return 0;
}
