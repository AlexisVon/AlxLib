/*****************************************************************/ /**
 * \file   ahash_bench.cpp
 * \brief  ah_64 benchmark: speed across key lengths, collision rate on base64 and hex keys
 *
 * Speed is ns per hash() call over a fixed total byte budget per length; auto_64 (the
 * default), wy_64 and fnv_64 run alongside. Collisions are measured two ways for
 * ah_64 and wy_64: a full sweep of every combination of the charset for small lengths (the
 * whole random space), and a sampled sweep for larger lengths against the birthday bound
 * N(N-1)/2^64.
 *
 * Methodology: IQR mean over the samples (tools/bench_common.h); comparing two revisions
 * means running this same binary against each build, alternating rounds.
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ahash.h"
#include "bench_common.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace alx;

namespace {
    uint_64 s_rng = 0x9E3779B97F4A7C15ULL;

    uint_64 next_rand() noexcept {
        s_rng ^= s_rng << 13;
        s_rng ^= s_rng >> 7;
        s_rng ^= s_rng << 17;
        return s_rng;
    }

    /// IQR mean of ns per call; _op runs _iters calls and returns a sink that must escape.
    template <typename F>
    double ns_per_call(const F& _op, size_t _iters, int _samples = 15) {
        volatile uint_64 sink = _op();
        (void) sink;
        std::vector<double> taken;
        taken.reserve((size_t) _samples);
        for (int i = 0; i < _samples; i++) {
            const auto start = bench::clk::now();
            sink = _op();
            taken.push_back(std::chrono::duration<double, std::nano>(bench::clk::now() - start).count() / (double) _iters);
        }
        return bench::iqr_mean(taken);
    }

    void speed_table() {
        const size_t lens[] = {0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 65, 127, 128, 129, 240, 256, 512, 1024};
        printf("%-6s %12s %12s %10s %12s %12s %12s %8s\n", "len", "ah_64 ns", "wy_64 ns", "auto ns", "fnv_64 ns", "ah_64 GB/s", "wy_64 GB/s", "auto/ah");
        for (const size_t len : lens) {
            std::vector<uint_8> buf(len ? len : 1);
            for (auto& b : buf) b = (uint_8) next_rand();
            const size_t iters = len ? std::max<size_t>(100000, (size_t) (64 * 1024 * 1024) / len) : 1000000;
            const double t_ah = ns_per_call(
                [&] {
                    uint_64 s = 0;
                    for (size_t i = 0; i < iters; i++) s ^= hash::ah_64::hash(buf.data(), len);
                    return s;
                },
                iters);
            const double t_wy = ns_per_call(
                [&] {
                    uint_64 s = 0;
                    for (size_t i = 0; i < iters; i++) s ^= hash::wy_64::hash(buf.data(), len);
                    return s;
                },
                iters);
            const double t_auto = ns_per_call(
                [&] {
                    uint_64 s = 0;
                    for (size_t i = 0; i < iters; i++) s ^= hash::auto_64::hash(buf.data(), len);
                    return s;
                },
                iters);
            const double t_fnv = ns_per_call(
                [&] {
                    uint_64 s = 0;
                    for (size_t i = 0; i < iters; i++) s ^= hash::fnv_64::hash(buf.data(), len);
                    return s;
                },
                iters);
            printf("%-6zu %12.2f %12.2f %10.2f %12.2f %12.1f %12.1f %8.2f\n", len, t_ah, t_wy, t_auto, t_fnv,
                   len ? (double) len / t_ah : 0.0, len ? (double) len / t_wy : 0.0, t_auto / t_ah);
            fflush(stdout);
        }
    }

    size_t collisions_in(std::vector<uint_64>& _hs) {
        std::sort(_hs.begin(), _hs.end());
        size_t collisions = 0;
        for (size_t i = 1; i < _hs.size(); i++) collisions += (_hs[i] == _hs[i - 1]);
        return collisions;
    }

    using hash_fn = uint_64 (*)(const void*, size_t);

    /// Every combination of the charset up to the element budget; the whole random space.
    void full_sweep(const char* _name, const char* _charset, size_t _budget, hash_fn _h) {
        const size_t charset_n = std::strlen(_charset);
        for (int len = 1;; len++) {
            unsigned long long space = 1;
            bool over = false;
            for (int i = 0; i < len; i++) {
                if (space > _budget / charset_n) {
                    over = true;
                    break;
                }
                space *= charset_n;
            }
            if (over) break;

            std::vector<uint_64> hs;
            hs.reserve((size_t) space);
            std::vector<int> idx((size_t) len, 0);
            std::string key((size_t) len, ' ');
            for (unsigned long long k = 0; k < space; k++) {
                for (int i = 0; i < len; i++) key[(size_t) i] = _charset[idx[(size_t) i]];
                hs.push_back(_h(key.data(), key.size()));
                for (int i = len - 1; i >= 0; i--) {
                    if (++idx[(size_t) i] < (int) charset_n) break;
                    idx[(size_t) i] = 0;
                }
            }
            printf("%-7s full  L=%-3d keys=%-10llu collisions=%zu\n", _name, len, space, collisions_in(hs));
            fflush(stdout);
        }
    }

    /// N random keys of length L, deduplicated first (a small key space repeats keys, not hashes),
    /// then compared against the birthday bound U(U-1)/2^64.
    void sampled_sweep(const char* _name, const char* _charset, int _len, size_t _n, hash_fn _h) {
        const size_t charset_n = std::strlen(_charset);
        std::vector<std::string> keys;
        keys.reserve(_n);
        std::string key((size_t) _len, ' ');
        for (size_t i = 0; i < _n; i++) {
            for (int j = 0; j < _len; j++) key[(size_t) j] = _charset[next_rand() % charset_n];
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        std::vector<uint_64> hs;
        hs.reserve(keys.size());
        for (const std::string& k : keys) hs.push_back(_h(k.data(), k.size()));
        const size_t unique_n = keys.size();
        const size_t collisions = collisions_in(hs);
        const double expected = (double) unique_n * ((double) unique_n - 1.0) / 2.0 / 18446744073709551616.0;
        printf("%-7s rand  L=%-3d N=%-9zu unique=%-9zu collisions=%zu expected=%.3g space=%zu^%d\n", _name, _len, _n,
               unique_n, collisions, expected, charset_n, _len);
        fflush(stdout);
    }
}

int main(int argc, char** argv) {
    const size_t sample_n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000;
    const size_t budget = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 2000000;
    if (0 == sample_n || 0 == budget) {
        printf("usage: ahash_bench [sample_n] [full_sweep_budget]\n");
        return 2;
    }
    printf("ahash bench: ah_64 (is_hard=%d), fnv_64 reference; samples=%zu budget=%zu\n", (int) hash::ah_64::is_hard(),
           sample_n, budget);

    speed_table();

    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char* hex = "0123456789abcdef";
    printf("\ncollisions: base64 charset (string keys)\n");
    full_sweep("ah64-base64", b64, budget, hash::ah_64::hash);
    full_sweep("wy64-base64", b64, budget, hash::wy_64::hash);
    for (const int len : {8, 16, 32, 64, 128}) sampled_sweep("ah64-b64", b64, len, sample_n, hash::ah_64::hash);
    for (const int len : {8, 16, 32, 64, 128}) sampled_sweep("wy64-b64", b64, len, sample_n, hash::wy_64::hash);

    printf("\ncollisions: hex charset (object keys)\n");
    full_sweep("ah64-hex", hex, budget, hash::ah_64::hash);
    full_sweep("wy64-hex", hex, budget, hash::wy_64::hash);
    for (const int len : {8, 16, 32, 64, 128}) sampled_sweep("ah64-hex", hex, len, sample_n, hash::ah_64::hash);
    for (const int len : {8, 16, 32, 64, 128}) sampled_sweep("wy64-hex", hex, len, sample_n, hash::wy_64::hash);

    return 0;
}
