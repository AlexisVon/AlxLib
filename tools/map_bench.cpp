/*****************************************************************/ /**
 * \file   map_bench.cpp
 * \brief  map_btree against std::map, and map_hash against std::unordered_map: fill, hit,
 *         miss, iterate, overwrite, erase, churn, and bytes per entry; --tiny sweeps
 *         n = 1..32 with hit, miss, insert and erase for three key shapes (equal-length
 *         hex, mixed-length script-like names, uint64); --read sweeps map_read against the
 *         direct-array ceiling, the bare hash, map_hash and unordered_map
 *
 * Methodology: IQR mean over the samples (tools/bench_common.h); setup (building and
 * filling the container) is untimed, only the measured phase is. Both containers run the
 * same key set and the same operation sequence, verified element by element through
 * conver<Ref>(). Bytes per entry is the live heap delta of a filled container, measured inside a
 * fresh child process (glibc tcache reuse hides it from mallinfo2 otherwise), not peak RSS.
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "amap.h"
#include "amap_std.h"
#include "bench_common.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33)) && defined(__linux__)
#    include <malloc.h>
#    include <unistd.h>
#    define ALX_MAP_BENCH_HEAP 1
#else
#    define ALX_MAP_BENCH_HEAP 0
#endif

using namespace alx;

namespace {
    uint_64 s_rng = 0x9E3779B97F4A7C15ULL;

    uint_64 next_rand() noexcept {
        s_rng ^= s_rng << 13;
        s_rng ^= s_rng >> 7;
        s_rng ^= s_rng << 17;
        return s_rng;
    }

    template <typename T>
    void shuffle(std::vector<T>& _v) {
        for (size_t i = _v.size(); i > 1; --i) {
            const size_t j = (size_t) (next_rand() % i);
            std::swap(_v[i - 1], _v[j]);
        }
    }

    /// Keys are an injective map of the index, so i in [0, n) never repeats and [n, 2n) is disjoint
    struct wide_slot {
        using key = std::string;
        using value = uint_64;

        static const char* tag() { return "wide"; }

        static key key_at(uint_64 _i) {
            char buf[13];
            snprintf(buf, sizeof(buf), "%012llx", (unsigned long long) (_i * 0x9E3779B97F4A7C15ULL));
            return key(buf);
        }
    };

    struct narrow_slot {
        using key = uint_64;
        using value = uint_64;

        static const char* tag() { return "narrow"; }

        static key key_at(uint_64 _i) { return _i * 0x9E3779B97F4A7C15ULL; }
    };

    /// Script-like names: the length varies, so a linear scan usually rejects a candidate on the
    /// size check; the numeric suffix keeps the map injective
    struct mixed_slot {
        using key = std::string;
        using value = uint_64;

        static const char* tag() { return "mixed"; }

        static key key_at(uint_64 _i) { return key(1 + _i % 5, (char) ('a' + _i % 26)) + std::to_string(_i); }
    };

    /// reserve() where the container has one; map_btree and std::map have none
    template <typename C>
    void reserve_for(C&, size_t) {}
    template <typename K, typename V, bool Own, typename H>
    void reserve_for(map_hash<K, V, Own, H>& _c, size_t _n) {
        _c.reserve(_n);
    }
    template <typename K, typename V>
    void reserve_for(std::unordered_map<K, V>& _c, size_t _n) {
        _c.reserve(_n);
    }

    /// std::map::insert does not overwrite, map_btree::insert does; this levels them
    template <typename C, typename K, typename V>
    void put(C& _c, const K& _k, V _v) {
        _c.insert(_k, _v);
    }

    template <typename K, typename V>
    void put(std::map<K, V>& _c, const K& _k, V _v) {
        _c.insert_or_assign(_k, _v);
    }

    template <typename K, typename V>
    void put(std::unordered_map<K, V>& _c, const K& _k, V _v) {
        _c.insert_or_assign(_k, _v);
    }

    /// ns per operation of _op, which runs _ops operations and returns a sink that must escape
    template <typename F>
    double ns_per_op(int _samples, size_t _ops, const F& _op) {
        volatile uint_64 sink = _op();
        (void) sink;
        std::vector<double> taken;
        taken.reserve((size_t) _samples);
        for (int i = 0; i < _samples; i++) {
            const auto start = bench::clk::now();
            sink = _op();
            taken.push_back(std::chrono::duration<double, std::nano>(bench::clk::now() - start).count() / (double) _ops);
        }
        return bench::iqr_mean(taken);
    }

    /// As ns_per_op, with a fresh container from _make built untimed before each sample
    template <typename Make, typename Body>
    double ns_per_op_setup(int _samples, size_t _ops, Make _make, Body _body) {
        std::vector<double> taken;
        taken.reserve((size_t) _samples);
        for (int i = 0; i < _samples; i++) {
            auto c = _make();
            const auto start = bench::clk::now();
            volatile uint_64 sink = _body(c);
            (void) sink;
            taken.push_back(std::chrono::duration<double, std::nano>(bench::clk::now() - start).count() / (double) _ops);
        }
        return bench::iqr_mean(taken);
    }

#if ALX_MAP_BENCH_HEAP
    /**
     * \brief Live heap bytes of a filled container, measured in a fresh child process
     *
     * tcache reuse bypasses the mallinfo2 accounting, so a second build in the same process
     * reports zero; the child starts with clean arenas and answers through stdout.
     */
    size_t live_heap(const char* _slot, size_t _n, const char* _container) {
        char exe[4096];
        const ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (len <= 0) return 0;
        exe[len] = 0;
        char cmd[4300];
        snprintf(cmd, sizeof(cmd), "%s --mem %s %zu %s", exe, _slot, _n, _container);
        FILE* pipe = popen(cmd, "r");
        if (!pipe) return 0;
        size_t bytes = 0;
        if (1 != fscanf(pipe, "%zu", &bytes)) bytes = 0;
        pclose(pipe);
        return bytes;
    }

    /// Live bytes the allocator holds: the brk heap plus mmapped regions, where large arrays land
    size_t live_bytes() {
        const struct mallinfo2 info = mallinfo2();
        return info.uordblks + info.hblkhd;
    }

    /// Bytes a filled container holds at the moment it is complete; runs only in the child
    template <typename Slot, typename Container>
    size_t measure_heap(size_t _n) {
        const size_t before = live_bytes();
        size_t bytes = 0;
        {
            Container c;
            for (size_t i = 0; i < _n; i++)
                c.insert(std::make_pair(Slot::key_at((uint_64) i), (typename Slot::value)(i * 7 + 3)));
            bytes = live_bytes() - before;
        }
        return bytes;
    }
#endif

    template <typename Slot, typename Tree, typename Ref>
    void run_case(const char* _label, const char* _tree_name, const char* _ref_name, const char* _ratio_name,
                  const char* _mem_tree, const char* _mem_ref, bool _reserved_fill, size_t _n, int _samples) {
        using K = typename Slot::key;
        using V = typename Slot::value;

        std::vector<K> keys(_n);
        std::vector<K> misses(_n);
        for (size_t i = 0; i < _n; i++) {
            keys[i] = Slot::key_at((uint_64) i);
            misses[i] = Slot::key_at((uint_64) (_n + i));
        }

        std::vector<size_t> fill_order(_n);
        for (size_t i = 0; i < _n; i++) fill_order[i] = i;
        shuffle(fill_order);
        std::vector<size_t> erase_order = fill_order;
        shuffle(erase_order);
        std::vector<size_t> hit_order(_n * 2);
        std::vector<size_t> miss_order(_n * 2);
        for (auto& idx : hit_order) idx = (size_t) (next_rand() % _n);
        for (auto& idx : miss_order) idx = (size_t) (next_rand() % _n);

        std::vector<std::pair<uint_8, size_t>> churn(_n * 4);
        for (auto& op : churn) {
            const uint_64 r = next_rand();
            op.first = (uint_8) (r % 3);
            op.second = (size_t) ((r >> 32) % _n);
        }

        const auto fill = [&](auto& _c) {
            for (size_t idx : fill_order) put(_c, keys[idx], (V) (idx * 7 + 3));
        };

        printf("\n== %s: N = %zu, samples = %d ==\n", _label, _n, _samples);
        printf("  %-22s %12s %12s %9s\n", "phase", _tree_name, _ref_name, _ratio_name);

        const double fill_tree = ns_per_op_setup(_samples, _n, [] { return Tree(); }, [&](Tree& _t) {
            fill(_t);
            return (uint_64) _t.size(); });
        const double fill_ref = ns_per_op_setup(_samples, _n, [] { return Ref(); }, [&](Ref& _r) {
            fill(_r);
            return (uint_64) _r.size(); });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "fill (ns/insert)", fill_tree, fill_ref, fill_tree / fill_ref);

        if (_reserved_fill) {
            const double fill_tree_res = ns_per_op_setup(_samples, _n, [&] {
                Tree c;
                reserve_for(c, _n);
                return c; }, [&](Tree& _t) {
                fill(_t);
                return (uint_64) _t.size(); });
            const double fill_ref_res = ns_per_op_setup(_samples, _n, [&] {
                Ref c;
                reserve_for(c, _n);
                return c; }, [&](Ref& _r) {
                fill(_r);
                return (uint_64) _r.size(); });
            printf("  %-22s %12.1f %12.1f %9.2f\n", "fill reserved (ns/insert)", fill_tree_res, fill_ref_res, fill_tree_res / fill_ref_res);
        }

        Tree tree;
        Ref ref;
        fill(tree);
        fill(ref);
        if (conver<Ref>(tree) != ref) {
            printf("  VERIFY FAILED after fill\n");
            exit(1);
        }

        const double hit_tree = ns_per_op(_samples, hit_order.size(), [&] {
            uint_64 s = 0;
            for (size_t idx : hit_order) {
                typename Tree::iterator it = tree.find(keys[idx]);
                if (it != tree.end()) s += it->second;
            }
            return s;
        });
        const double hit_ref = ns_per_op(_samples, hit_order.size(), [&] {
            uint_64 s = 0;
            for (size_t idx : hit_order) {
                typename Ref::iterator it = ref.find(keys[idx]);
                if (it != ref.end()) s += it->second;
            }
            return s;
        });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "hit lookup (ns/op)", hit_tree, hit_ref, hit_tree / hit_ref);

        const double miss_tree = ns_per_op(_samples, miss_order.size(), [&] {
            uint_64 s = 0;
            for (size_t idx : miss_order) s += (tree.find(misses[idx]) == tree.end());
            return s;
        });
        const double miss_ref = ns_per_op(_samples, miss_order.size(), [&] {
            uint_64 s = 0;
            for (size_t idx : miss_order) s += (ref.find(misses[idx]) == ref.end());
            return s;
        });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "miss lookup (ns/op)", miss_tree, miss_ref, miss_tree / miss_ref);

        const double iter_tree = ns_per_op(_samples, _n, [&] {
            uint_64 s = 0;
            for (const auto& e : tree) s += e.second;
            return s;
        });
        const double iter_ref = ns_per_op(_samples, _n, [&] {
            uint_64 s = 0;
            for (const auto& e : ref) s += e.second;
            return s;
        });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "iterate (ns/entry)", iter_tree, iter_ref, iter_tree / iter_ref);

        const double over_tree = ns_per_op(_samples, hit_order.size(), [&] {
            for (size_t idx : hit_order) tree.insert(keys[idx], (V) (idx + 1));
            return (uint_64) tree.size();
        });
        const double over_ref = ns_per_op(_samples, hit_order.size(), [&] {
            for (size_t idx : hit_order) put(ref, keys[idx], (V) (idx + 1));
            return (uint_64) ref.size();
        });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "overwrite (ns/op)", over_tree, over_ref, over_tree / over_ref);

        const double erase_tree = ns_per_op_setup(_samples, _n, [&] {
            Tree c;
            fill(c);
            return c; }, [&](Tree& _t) {
            for (size_t idx : erase_order) _t.erase(keys[idx]);
            return (uint_64) _t.size(); });
        const double erase_ref = ns_per_op_setup(_samples, _n, [&] {
            Ref c;
            fill(c);
            return c; }, [&](Ref& _r) {
            for (size_t idx : erase_order) _r.erase(keys[idx]);
            return (uint_64) _r.size(); });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "erase all (ns/op)", erase_tree, erase_ref, erase_tree / erase_ref);

        const double churn_tree = ns_per_op(_samples, churn.size(), [&] {
            for (const auto& op : churn) {
                if (0 == op.first) tree.insert(keys[op.second], (V) op.second);
                else if (1 == op.first) tree.erase(keys[op.second]);
                else tree.find(keys[op.second]);
            }
            return (uint_64) tree.size();
        });
        const double churn_ref = ns_per_op(_samples, churn.size(), [&] {
            for (const auto& op : churn) {
                if (0 == op.first) put(ref, keys[op.second], (V) op.second);
                else if (1 == op.first) ref.erase(keys[op.second]);
                else ref.find(keys[op.second]);
            }
            return (uint_64) ref.size();
        });
        printf("  %-22s %12.1f %12.1f %9.2f\n", "churn mixed (ns/op)", churn_tree, churn_ref, churn_tree / churn_ref);

        if (conver<Ref>(tree) != ref) {
            printf("  VERIFY FAILED after churn\n");
            exit(1);
        }

#if ALX_MAP_BENCH_HEAP
        const size_t heap_tree = live_heap(Slot::tag(), _n, _mem_tree);
        const size_t heap_ref = live_heap(Slot::tag(), _n, _mem_ref);
        printf("  %-22s %12.1f %12.1f %9.2f\n", "heap (B/entry)", (double) heap_tree / (double) _n,
               (double) heap_ref / (double) _n, (double) heap_tree / (double) heap_ref);
#endif
        printf("  verify: ok\n");
        fflush(stdout);
    }

    /// Tiny-table sweep: n = 1..32, map_hash against std::unordered_map, same key order and op mix
    template <typename Slot>
    void run_tiny(const char* _label, int _samples) {
        using K = typename Slot::key;
        using V = typename Slot::value;
        using Hash = map_hash<K, V>;
        using Ref = std::unordered_map<K, V>;

        printf("\n== tiny sweep: %s ==\n", _label);
        printf("  %3s %10s %10s %7s %10s %10s %7s %10s %10s %10s %10s\n", "n", "hit", "umap", "ratio", "miss", "umap",
               "ratio", "insert", "umap", "erase", "umap");

        const size_t probes = 16384;
        for (size_t n = 1; n <= 32; n++) {
            std::vector<K> keys(n);
            std::vector<K> misses(n);
            std::vector<size_t> order(n);
            for (size_t i = 0; i < n; i++) {
                keys[i] = Slot::key_at((uint_64) i);
                misses[i] = Slot::key_at((uint_64) (64 + i));
                order[i] = i;
            }
            shuffle(order);
            std::vector<size_t> hit_idx(probes);
            std::vector<size_t> miss_idx(probes);
            for (size_t k = 0; k < probes; k++) {
                hit_idx[k] = k % n;
                miss_idx[k] = k % n;
            }

            Hash hm;
            Ref um;
            for (size_t i : order) {
                hm.insert(keys[i], (V) (i * 7 + 3));
                um.insert_or_assign(keys[i], (V) (i * 7 + 3));
            }
            if (conver<Ref>(hm) != um) {
                printf("  VERIFY FAILED at n = %zu\n", n);
                exit(1);
            }

            const double hit_h = ns_per_op(_samples, probes, [&] {
                uint_64 s = 0;
                for (size_t idx : hit_idx) {
                    auto it = hm.find(keys[idx]);
                    if (it != hm.end()) s += (uint_64) it->second;
                }
                return s;
            });
            const double hit_u = ns_per_op(_samples, probes, [&] {
                uint_64 s = 0;
                for (size_t idx : hit_idx) {
                    auto it = um.find(keys[idx]);
                    if (it != um.end()) s += (uint_64) it->second;
                }
                return s;
            });
            const double miss_h = ns_per_op(_samples, probes, [&] {
                uint_64 s = 0;
                for (size_t idx : miss_idx) s += (hm.find(misses[idx]) == hm.end());
                return s;
            });
            const double miss_u = ns_per_op(_samples, probes, [&] {
                uint_64 s = 0;
                for (size_t idx : miss_idx) s += (um.find(misses[idx]) == um.end());
                return s;
            });
            const double ins_h = ns_per_op_setup(_samples, n, [] { return Hash(); }, [&](Hash& _c) {
                for (size_t i : order) _c.insert(keys[i], (V) (i * 7 + 3));
                return (uint_64) _c.size(); });
            const double ins_u = ns_per_op_setup(_samples, n, [] { return Ref(); }, [&](Ref& _c) {
                for (size_t i : order) _c.insert_or_assign(keys[i], (V) (i * 7 + 3));
                return (uint_64) _c.size(); });
            const auto make = [&] {
                Hash c;
                for (size_t i : order) c.insert(keys[i], (V) (i * 7 + 3));
                return c;
            };
            const auto make_ref = [&] {
                Ref c;
                for (size_t i : order) c.insert_or_assign(keys[i], (V) (i * 7 + 3));
                return c;
            };
            const double era_h = ns_per_op_setup(_samples, n, make, [&](Hash& _c) {
                for (size_t i : order) _c.erase(keys[i]);
                return (uint_64) _c.size();
            });
            const double era_u = ns_per_op_setup(_samples, n, make_ref, [&](Ref& _c) {
                for (size_t i : order) _c.erase(keys[i]);
                return (uint_64) _c.size();
            });
            printf("  %3zu %10.2f %10.2f %7.2f %10.2f %10.2f %7.2f %10.2f %10.2f %10.2f %10.2f\n", n, hit_h, hit_u,
                   hit_h / hit_u, miss_h, miss_u, miss_h / miss_u, ins_h, ins_u, era_h, era_u);
            fflush(stdout);
        }
    }

    /// Read sweep: map_read against the direct-array ceiling, the bare hash, map_hash and unordered_map
    template <typename Slot>
    void run_read(const char* _label, int _samples) {
        using K = typename Slot::key;
        using V = typename Slot::value;
        using Read = map_read<K, V>;
        using Hash = map_hash<K, V>;
        using Ref = std::unordered_map<K, V>;
        using Hasher = hash::func<hash::auto_64, uint_64>;

        printf("\n== read sweep: %s ==\n", _label);
        printf("  %4s %8s %8s %8s %8s %8s %8s | %8s %8s %8s | %8s %8s\n", "n", "array", "hash", "read", "mhash", "umap",
               "read/arr", "miss_rd", "miss_mh", "miss_um", "fill_rd", "erase_rd");

        const size_t probes = 16384;
        const size_t sizes[] = {8, 16, 32, 64, 128, 512};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
            const size_t n = sizes[s];
            std::vector<K> keys(n);
            std::vector<K> misses(n);
            std::vector<V> values(n);
            std::vector<size_t> idx(probes);
            for (size_t i = 0; i < n; i++) {
                keys[i] = Slot::key_at((uint_64) i);
                misses[i] = Slot::key_at((uint_64) (n + i));
                values[i] = (V) (i * 7 + 3);
            }
            for (size_t k = 0; k < probes; k++) idx[k] = k % n;

            const auto fill_read = [&](Read& _c) {
                for (size_t i = 0; i < n; i++) _c.insert(keys[i], values[i]);
            };
            Read rd;
            Hash mh;
            Ref um;
            fill_read(rd);
            for (size_t i = 0; i < n; i++) mh.insert(keys[i], values[i]);
            for (size_t i = 0; i < n; i++) um.insert_or_assign(keys[i], values[i]);
            if (conver<Ref>(rd) != um || conver<Ref>(mh) != um) {
                printf("  VERIFY FAILED at n = %zu\n", n);
                exit(1);
            }

            const double hit_arr = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) sum += (uint_64) values[i];
                return sum;
            });
            const double hit_hash = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) sum += Hasher::eval(keys[i]);
                return sum;
            });
            const double hit_rd = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) {
                    const V* v = rd.peek(keys[i]);
                    if (v) sum += (uint_64) *v;
                }
                return sum;
            });
            const double hit_mh = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) {
                    const V* v = mh.peek(keys[i]);
                    if (v) sum += (uint_64) *v;
                }
                return sum;
            });
            const double hit_um = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) {
                    auto it = um.find(keys[i]);
                    if (it != um.end()) sum += (uint_64) it->second;
                }
                return sum;
            });
            const double miss_rd = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) sum += (rd.peek(misses[i]) == nullptr);
                return sum;
            });
            const double miss_mh = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) sum += (mh.peek(misses[i]) == nullptr);
                return sum;
            });
            const double miss_um = ns_per_op(_samples, probes, [&] {
                uint_64 sum = 0;
                for (size_t i : idx) sum += (um.find(misses[i]) == um.end());
                return sum;
            });
            const double fill_rd = ns_per_op_setup(_samples, n, [] { return Read(); }, [&](Read& _c) {
                fill_read(_c);
                return (uint_64) _c.size(); });
            const double erase_rd = ns_per_op_setup(_samples, n, [&] {
                Read c;
                fill_read(c);
                return c; }, [&](Read& _c) {
                for (size_t i = 0; i < n; i++) _c.erase(keys[i]);
                return (uint_64) _c.size(); });
            printf("  %4zu %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f | %8.2f %8.2f %8.2f | %8.2f %8.2f\n", n, hit_arr, hit_hash,
                   hit_rd, hit_mh, hit_um, hit_rd / hit_arr, miss_rd, miss_mh, miss_um, fill_rd, erase_rd);
            fflush(stdout);
        }
    }
}

int main(int argc, char** argv) {
#if ALX_MAP_BENCH_HEAP
    if (argc > 4 && 0 == std::strcmp(argv[1], "--mem")) {
        const size_t n = std::strtoull(argv[3], nullptr, 10);
        const bool wide = 0 == std::strcmp(argv[2], "wide");
        const char* kind = argv[4];
        size_t bytes = 0;
        if (wide && 0 == std::strcmp(kind, "tree")) bytes = measure_heap<wide_slot, map_btree<std::string, uint_64>>(n);
        else if (wide && 0 == std::strcmp(kind, "hash")) bytes = measure_heap<wide_slot, map_hash<std::string, uint_64>>(n);
        else if (wide && 0 == std::strcmp(kind, "umap")) bytes = measure_heap<wide_slot, std::unordered_map<std::string, uint_64>>(n);
        else if (wide) bytes = measure_heap<wide_slot, std::map<std::string, uint_64>>(n);
        else if (0 == std::strcmp(kind, "tree")) bytes = measure_heap<narrow_slot, map_btree<uint_64, uint_64>>(n);
        else if (0 == std::strcmp(kind, "hash")) bytes = measure_heap<narrow_slot, map_hash<uint_64, uint_64>>(n);
        else if (0 == std::strcmp(kind, "umap")) bytes = measure_heap<narrow_slot, std::unordered_map<uint_64, uint_64>>(n);
        else bytes = measure_heap<narrow_slot, std::map<uint_64, uint_64>>(n);
        printf("%zu\n", bytes);
        return 0;
    }
#endif

    if (argc > 1 && 0 == std::strcmp(argv[1], "--tiny")) {
        const int samples = argc > 2 ? (int) std::strtoull(argv[2], nullptr, 10) : 21;
        run_tiny<wide_slot>("wide slot (std::string -> uint_64, 40 B)", samples);
        run_tiny<narrow_slot>("narrow slot (uint_64 -> uint_64, 16 B)", samples);
        run_tiny<mixed_slot>("mixed-length names (std::string -> uint_64)", samples);
        return 0;
    }

    if (argc > 1 && 0 == std::strcmp(argv[1], "--read")) {
        const int samples = argc > 2 ? (int) std::strtoull(argv[2], nullptr, 10) : 21;
        run_read<mixed_slot>("mixed-length names (std::string -> uint_64)", samples);
        run_read<wide_slot>("wide slot (std::string -> uint_64, 40 B)", samples);
        run_read<narrow_slot>("narrow slot (uint_64 -> uint_64, 16 B)", samples);
        return 0;
    }

    const size_t max_n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 131072;
    if (0 == max_n) {
        printf("usage: map_bench [max_n] | map_bench --tiny [samples] | map_bench --read [samples]\n");
        return 2;
    }

    printf("map bench: map_btree vs std::map, map_hash vs std::unordered_map; key sets and operation sequences identical\n");

    const size_t sizes[] = {64, 1024, 16384, max_n};
    const size_t count = max_n > 16384 ? 4 : 3;

    for (size_t s = 0; s < count; s++) {
        const size_t n = sizes[s];
        const int samples = n <= 1024 ? 15 : (n <= 16384 ? 11 : 5);
        run_case<wide_slot, map_btree<std::string, uint_64>, std::map<std::string, uint_64>>(
            "wide slot (std::string -> uint_64, 40 B)", "map_btree", "std::map", "btree/std", "tree", "std", false, n, samples);
        run_case<wide_slot, map_hash<std::string, uint_64>, std::unordered_map<std::string, uint_64>>(
            "wide slot (std::string -> uint_64, 40 B)", "map_hash", "std::unordered_map", "hash/umap", "hash", "umap", true, n, samples);
        run_case<narrow_slot, map_btree<uint_64, uint_64>, std::map<uint_64, uint_64>>(
            "narrow slot (uint_64 -> uint_64, 16 B)", "map_btree", "std::map", "btree/std", "tree", "std", false, n, samples);
        run_case<narrow_slot, map_hash<uint_64, uint_64>, std::unordered_map<uint_64, uint_64>>(
            "narrow slot (uint_64 -> uint_64, 16 B)", "map_hash", "std::unordered_map", "hash/umap", "hash", "umap", true, n, samples);
    }
    return 0;
}
