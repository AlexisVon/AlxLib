/*****************************************************************/ /**
 * \file   gt_amap.cpp
 * \brief  map_btree / map_hash / map_read: functionality, std differential tests, conver
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "amap.h"
#include "amap_std.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace alx;

namespace {
    using Tree = map_btree<std::string, int>;
    using Hash = map_hash<std::string, int>;
    using Read = map_read<std::string, int>;
    using Flat = std::unordered_map<std::string, int>;
    using Ordered = std::map<std::string, int>;

    struct owned {
        static int live;
        int value;

        explicit owned(int _v) : value(_v) { ++live; }
        owned(const owned& _o) : value(_o.value) { ++live; }
        ~owned() { --live; }
    };
    int owned::live = 0;

    std::string key_of(int _i) { return "k" + std::to_string(_i); }

    uint_64 next_rand(uint_64& _state) {
        _state ^= _state << 13;
        _state ^= _state >> 7;
        _state ^= _state << 17;
        return _state;
    }

    std::vector<std::string> keys_of(const Tree& _tree) {
        std::vector<std::string> keys;
        for (Tree::const_iterator it = _tree.cbegin(); it != _tree.cend(); ++it) keys.push_back(it->first);
        return keys;
    }

    /// Low 32 bits constant: every key homes to the same L0 slot, forcing L1 and chained blocks
    struct weak_hash {
        static uint_64 hash(const void* _data, size_t _size) { return hash::fnv_64::hash(_data, _size) & 0xFFFFFFFF00000000ULL; }
        static uint_64 hash(uint_64 _x) { return hash::fnv_64::hash(_x) & 0xFFFFFFFF00000000ULL; }
        static bool is_hard() { return false; }
        static constexpr bool is_constexpr() { return false; }
    };

    /// Top six bits cleared: every key shares the slot fragment, so the fragment never rejects
    struct flat_frag_hash {
        static uint_64 hash(const void* _data, size_t _size) { return hash::fnv_64::hash(_data, _size) & 0x03FFFFFFFFFFFFFFULL; }
        static uint_64 hash(uint_64 _x) { return hash::fnv_64::hash(_x) & 0x03FFFFFFFFFFFFFFULL; }
        static bool is_hard() { return false; }
        static constexpr bool is_constexpr() { return false; }
    };

    /// Identity for integer keys: the caller picks the home slot
    struct id_hash {
        static uint_64 hash(const void* _data, size_t _size) { return hash::fnv_64::hash(_data, _size); }
        static uint_64 hash(uint_64 _x) { return _x; }
        static bool is_hard() { return false; }
        static constexpr bool is_constexpr() { return false; }
    };

    /// Low 32 bits cleared: every key homes to slot 0, one long probe cluster
    struct zero_low_hash {
        static uint_64 hash(const void* _data, size_t _size) { return hash::fnv_64::hash(_data, _size) & 0xFFFFFFFF00000000ULL; }
        static uint_64 hash(uint_64 _x) { return hash::fnv_64::hash(_x) & 0xFFFFFFFF00000000ULL; }
        static bool is_hard() { return false; }
        static constexpr bool is_constexpr() { return false; }
    };
}

TEST(gt_amap, Empty) {
    Tree tree;
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(tree.size(), 0u);
    ASSERT_EQ(tree.begin(), tree.end());
    ASSERT_EQ(tree.cbegin(), tree.cend());
    ASSERT_EQ(tree.find("x"), tree.end());
    ASSERT_EQ(tree.erase("x"), 0u);
    tree.clear();
    ASSERT_TRUE(tree.empty());
}

TEST(gt_amap, InsertFindErase) {
    Tree tree;
    tree.insert("b", 2);
    tree.insert("a", 1);
    tree.insert("c", 3);
    ASSERT_EQ(tree.size(), 3u);
    ASSERT_EQ(tree.find("a")->second, 1);
    ASSERT_EQ(tree.find("b")->second, 2);
    ASSERT_EQ(tree.find("c")->second, 3);
    ASSERT_EQ(tree.find("d"), tree.end());
    const Tree& ctree = tree;
    ASSERT_EQ(ctree.find("a")->second, 1);
    ASSERT_EQ(ctree.find("d"), ctree.cend());

    ASSERT_EQ(tree.erase("b"), 1u);
    ASSERT_EQ(tree.erase("b"), 0u);
    ASSERT_EQ(tree.size(), 2u);
    ASSERT_EQ(tree.find("b"), tree.end());
    ASSERT_EQ(tree.find("a")->second, 1);
}

TEST(gt_amap, InsertOverwrites) {
    Tree tree;
    tree.insert("k", 1);
    Tree::iterator it = tree.insert("k", 2);
    ASSERT_EQ(it->first, "k");
    ASSERT_EQ(it->second, 2);
    ASSERT_EQ(tree.size(), 1u);

    const std::pair<std::string, int> p("k", 3);
    tree.insert(p);
    ASSERT_EQ(tree.find("k")->second, 3);

    tree.insert(std::pair<std::string, int>("k", 4));
    ASSERT_EQ(tree.find("k")->second, 4);

    tree.insert(std::string("k"), 5);
    ASSERT_EQ(tree.find("k")->second, 5);
    ASSERT_EQ(tree.size(), 1u);
}

TEST(gt_amap, EmplaceAndSubscript) {
    map_btree<std::string, std::string> tree;
    auto it = tree.emplace("a", "one");
    ASSERT_EQ(it->first, "a");
    ASSERT_EQ(it->second, "one");
    tree.emplace(std::string("b"), std::string("two"));
    ASSERT_EQ(tree.size(), 2u);

    ASSERT_EQ(tree["c"], "");
    ASSERT_EQ(tree.size(), 3u);
    tree["c"] = "three";
    ASSERT_EQ(tree["c"], "three");
    ASSERT_EQ(tree.size(), 3u);

    Tree numbers;
    numbers.insert("x", 1);
    Tree::iterator dup = numbers.emplace("x", 2);
    ASSERT_EQ(dup->second, 2);
    ASSERT_EQ(numbers.find("x")->second, 2);
    ASSERT_EQ(numbers.size(), 1u);
}

TEST(gt_amap, ContainsCountTryEmplace) {
    Tree tree;
    ASSERT_FALSE(tree.contains("a"));
    ASSERT_EQ(tree.count("a"), 0u);

    std::pair<Tree::iterator, bool> r = tree.try_emplace("a", 1);
    ASSERT_TRUE(r.second);
    ASSERT_EQ(r.first->second, 1);
    ASSERT_TRUE(tree.contains("a"));
    ASSERT_EQ(tree.count("a"), 1u);

    r = tree.try_emplace("a", 2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(r.first->second, 1);
    ASSERT_EQ(tree.find("a")->second, 1);

    ASSERT_TRUE(tree.try_emplace("d").second);
    ASSERT_EQ(tree.find("d")->second, 0);

    map_btree<std::string, std::string> m;
    m.insert("k", "keep");
    std::string value = "unused";
    std::pair<map_btree<std::string, std::string>::iterator, bool> hit = m.try_emplace("k", std::move(value));
    ASSERT_FALSE(hit.second);
    ASSERT_EQ(m.find("k")->second, "keep");
    ASSERT_EQ(value, "unused");
    ASSERT_TRUE(m.try_emplace(std::string("n"), "fresh").second);
    ASSERT_EQ(m.find("n")->second, "fresh");
    ASSERT_EQ(m.size(), 2u);
}

TEST(gt_amap, IterateInKeyOrder) {
    const int shuffled[] = {5, 3, 9, 1, 7, 2, 8, 0, 6, 4};
    map_btree<int, int> tree;
    for (int k : shuffled) tree.insert(k, k);
    int expect = 0;
    for (const auto& e : tree) {
        ASSERT_EQ(e.first, expect);
        ASSERT_EQ(e.second, expect);
        ++expect;
    }
    ASSERT_EQ(expect, 10);

    map_btree<int, int> back;
    for (int k = 9; k >= 0; --k) back.insert(k, k);
    expect = 0;
    const map_btree<int, int>& cback = back;
    for (map_btree<int, int>::const_iterator it = cback.cbegin(); it != cback.cend(); ++it) {
        ASSERT_EQ(it->first, expect);
        ++expect;
    }
    ASSERT_EQ(expect, 10);
}

TEST(gt_amap, EraseReturnsSuccessor) {
    map_btree<int, int> tree;
    for (int i = 0; i < 100; ++i) tree.insert(i, i * 10);

    map_btree<int, int>::iterator it = tree.begin();
    while (it != tree.end()) {
        map_btree<int, int>::iterator next = it;
        ++next;
        const bool has_next = next != tree.end();
        const int next_key = has_next ? next->first : 0;
        it = tree.erase(it);
        if (has_next) {
            ASSERT_NE(it, tree.end());
            ASSERT_EQ(it->first, next_key);
        } else {
            ASSERT_EQ(it, tree.end());
        }
    }
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(tree.begin(), tree.end());
}

TEST(gt_amap, GrowShrink) {
    const int n = 2000;
    map_btree<int, int> tree;
    for (int i = 0; i < n; ++i) tree.insert(i, i);
    ASSERT_EQ(tree.size(), (size_t) n);

    int expect = 0;
    for (const auto& e : tree) {
        ASSERT_EQ(e.first, expect);
        ASSERT_EQ(e.second, expect);
        ++expect;
    }
    ASSERT_EQ(expect, n);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = pass; i < n; i += 2) tree.erase(i);
    }
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(tree.begin(), tree.end());

    for (int i = 0; i < 50; ++i) tree.insert(i, i);
    ASSERT_EQ(tree.size(), 50u);
    ASSERT_EQ(tree.begin()->first, 0);
}

TEST(gt_amap, OwningPointerValues) {
    using Own = map_btree<int, owned*, true>;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        m.insert(1, new owned(10));
        m.insert(2, new owned(20));
        ASSERT_EQ(owned::live, 2);

        m.insert(1, new owned(11));
        ASSERT_EQ(owned::live, 2);
        ASSERT_EQ(m.find(1)->second->value, 11);

        owned* spare = new owned(99);
        auto hit = m.try_emplace(2, spare);
        ASSERT_FALSE(hit.second);
        ASSERT_EQ(m.find(2)->second->value, 20);
        delete spare;

        Own copy(m);
        ASSERT_EQ(owned::live, 4);
        ASSERT_NE(copy.find(1)->second, m.find(1)->second);
        ASSERT_EQ(copy.find(1)->second->value, 11);

        m.erase(1);
        ASSERT_EQ(owned::live, 3);
        m.clear();
        ASSERT_EQ(owned::live, 2);
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, OwningStress) {
    using Own = map_btree<int, owned*, true>;
    uint_64 state = 0x0123456789ABCDEFULL;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        std::map<int, int> model;
        for (int step = 0; step < 3000; ++step) {
            const uint_64 r = next_rand(state);
            const int key = (int) (r % 64);
            switch (r % 4) {
            case 0:
            case 1: {
                const int value = (int) ((r >> 32) % 1000);
                m.insert(key, new owned(value));
                model[key] = value;
                break;
            }
            case 2: {
                owned* gone = m.take(key);
                const std::map<int, int>::iterator found = model.find(key);
                ASSERT_EQ(gone != nullptr, found != model.end());
                if (gone != nullptr) {
                    ASSERT_EQ(gone->value, found->second);
                    delete gone;
                    model.erase(found);
                }
                break;
            }
            default:
                ASSERT_EQ(m.erase(key), model.erase(key));
                break;
            }
            ASSERT_EQ(m.size(), model.size());
            ASSERT_EQ(owned::live, (int) model.size());
        }
        Own copy(m);
        ASSERT_EQ(owned::live, (int) (model.size() * 2));
        copy.clear();
        ASSERT_EQ(owned::live, (int) model.size());
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, Take) {
    using Own = map_btree<int, owned*, true>;
    Own m;
    m.insert(1, new owned(10));
    ASSERT_EQ(owned::live, 1);
    owned* got = m.take(1);
    ASSERT_NE(got, nullptr);
    ASSERT_EQ(got->value, 10);
    ASSERT_EQ(owned::live, 1);
    ASSERT_FALSE(m.contains(1));
    ASSERT_EQ(m.size(), 0u);
    delete got;
    ASSERT_EQ(owned::live, 0);
    ASSERT_EQ(m.take(9), nullptr);

    map_btree<std::string, int> plain;
    plain.insert("a", 7);
    ASSERT_EQ(plain.take("a"), 7);
    ASSERT_FALSE(plain.contains("a"));
    ASSERT_EQ(plain.take("a"), 0);
}

TEST(gt_amap, CopyMoveSwap) {
    Tree a;
    for (int i = 0; i < 100; ++i) a.insert(key_of(i), i);

    Tree b(a);
    ASSERT_EQ(b.size(), a.size());
    ASSERT_EQ(keys_of(b), keys_of(a));
    b.insert(key_of(50), -1);
    ASSERT_EQ(a.find(key_of(50))->second, 50);
    ASSERT_EQ(b.find(key_of(50))->second, -1);

    Tree c;
    c = a;
    ASSERT_EQ(conver<Ordered>(c), conver<Ordered>(a));

    Tree d(std::move(c));
    ASSERT_TRUE(c.empty());
    ASSERT_EQ(d.size(), a.size());

    Tree e;
    e.swap(d);
    ASSERT_TRUE(d.empty());
    ASSERT_EQ(keys_of(e), keys_of(a));

    a = a;
    ASSERT_EQ(a.size(), 100u);
}

TEST(gt_amap, Uint64Keys) {
    map_btree<uint_64, uint_64> tree;
    const int n = 1000;
    for (int i = 0; i < n; ++i) tree.insert((uint_64) i * 2654435761ULL % 1000000007ULL, (uint_64) i);
    ASSERT_EQ(tree.size(), (size_t) n);
    uint_64 prev = 0;
    bool first = true;
    size_t count = 0;
    for (const auto& e : tree) {
        if (!first) ASSERT_LT(prev, e.first);
        prev = e.first;
        first = false;
        ++count;
    }
    ASSERT_EQ(count, (size_t) n);
    while (!tree.empty()) tree.erase(tree.begin());
    ASSERT_TRUE(tree.empty());
}

TEST(gt_amap, StdConversion) {
    std::map<std::string, int> src{{"a", 1}, {"b", 2}, {"c", 3}};
    Tree tree = conver<Tree>(src);
    ASSERT_EQ(conver<Ordered>(tree), src);
    ASSERT_EQ(tree.size(), 3u);

    Tree moved = conver<Tree>(std::map<std::string, int>(src));
    ASSERT_EQ(conver<Ordered>(moved), src);

    std::map<std::string, int> back = conver<Ordered>(std::move(moved));
    ASSERT_EQ(back, src);
    ASSERT_TRUE(moved.empty());
}

TEST(gt_amap, RandomAgainstStdMap) {
    const int steps = 20000;
    Tree tree;
    std::map<std::string, int> ref;
    uint_64 state = 0x9E3779B97F4A7C15ULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % 500));
        switch (r % 7) {
        case 0:
        case 1:
        case 2:
        case 3: {
            const int val = (int) (r >> 32);
            Tree::iterator a = tree.insert(key, val);
            std::map<std::string, int>::iterator b = ref.insert_or_assign(key, val).first;
            ASSERT_EQ(a->first, b->first);
            ASSERT_EQ(a->second, b->second);
            break;
        }
        case 4:
            ASSERT_EQ(tree.erase(key), ref.erase(key));
            break;
        case 5: {
            Tree::iterator a = tree.find(key);
            std::map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == tree.end(), b == ref.end());
            if (b != ref.end()) ASSERT_EQ(a->second, b->second);
            break;
        }
        default: {
            int& x = tree[key];
            int& y = ref[key];
            ASSERT_EQ(x, y);
            ++x;
            ++y;
            break;
        }
        }
        ASSERT_EQ(tree.size(), ref.size());
        if (0 == step % 251) {
            ASSERT_EQ(conver<Ordered>(tree), ref);
            std::vector<std::string> ref_keys;
            for (const auto& e : ref) ref_keys.push_back(e.first);
            ASSERT_EQ(keys_of(tree), ref_keys);
        }
    }
    ASSERT_EQ(conver<Ordered>(tree), ref);
}

TEST(gt_amap, StressChurn) {
    const int pool = 5000;
    const int steps = 150000;
    Tree tree;
    std::map<std::string, int> ref;
    uint_64 state = 0xA5A5A5A55A5A5A5AULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % pool));
        switch (r % 12) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: {
            const int val = (int) (r >> 32);
            tree.insert(key, val);
            ref.insert_or_assign(key, val);
            break;
        }
        case 6:
        case 7:
        case 8:
            ASSERT_EQ(tree.erase(key), ref.erase(key));
            break;
        case 9:
        case 10: {
            Tree::iterator a = tree.find(key);
            std::map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == tree.end(), b == ref.end());
            break;
        }
        default:
            ++tree[key];
            ++ref[key];
            break;
        }
        if (0 == step % 4099) ASSERT_EQ(tree.size(), ref.size());
    }
    ASSERT_EQ(conver<Ordered>(tree), ref);
}

TEST(gt_amap, ValueLookup) {
    Tree tree;
    tree.insert("a", 1);
    ASSERT_EQ(tree.value("a"), 1);
    ASSERT_EQ(tree.value("missing"), 0);
    ASSERT_EQ(tree.value("missing", -1), -1);
    ASSERT_EQ(tree.size(), 1u);

    Hash map;
    map.insert("a", 1);
    ASSERT_EQ(map.value("a"), 1);
    ASSERT_EQ(map.value("missing"), 0);
    ASSERT_EQ(map.value("missing", -1), -1);
    ASSERT_EQ(map.size(), 1u);

    map.insert("b", 2);
    const int& stored = map.value("b");
    ASSERT_EQ(stored, 2);
    const int& absent = map.value("none");
    ASSERT_EQ(absent, 0);
}

TEST(gt_amap, CannotContainContract) {
    Hash map;
    ASSERT_TRUE(map.cannot_contain("a"));
    map.insert("a", 1);
    ASSERT_FALSE(map.cannot_contain("a"));

    uint_64 state = 0xDEADBEEFCAFEF00DULL;
    for (int i = 0; i < 300; ++i) map.insert(key_of(i), i);
    for (int step = 0; step < 2000; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % 400));
        if (0 == r % 4) map.insert(key, (int) (r >> 32));
        if (0 == r % 5) map.erase(key);
        // true only when certainly absent; false says nothing
        if (map.contains(key)) ASSERT_FALSE(map.cannot_contain(key));
        if (0 == step % 97) {
            for (int k = 0; k < 400; ++k) {
                if (map.cannot_contain(key_of(k))) ASSERT_FALSE(map.contains(key_of(k)));
            }
        }
    }
    map.clear();
    ASSERT_TRUE(map.cannot_contain("a"));
}

TEST(gt_amap, HashFragmentAlwaysMatches) {
    using Flat = map_hash<int, int, false, flat_frag_hash>;
    Flat map;
    for (int i = 0; i < 200; ++i) map.insert(i, i * 2);
    ASSERT_EQ(map.size(), 200u);
    for (int i = 0; i < 200; ++i) {
        Flat::iterator it = map.find(i);
        ASSERT_NE(it, map.end());
        ASSERT_EQ(it->second, i * 2);
    }
    ASSERT_EQ(map.find(1000), map.end());
    for (int i = 0; i < 200; i += 5) ASSERT_EQ(map.erase(i), 1u);
    for (int i = 0; i < 200; ++i) ASSERT_EQ(map.contains(i), 0 != i % 5);
    size_t seen = 0;
    for (Flat::const_iterator it = map.cbegin(); it != map.cend(); ++it) ++seen;
    ASSERT_EQ(seen, map.size());
}

TEST(gt_amap, HashEmpty) {
    Hash map;
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.size(), 0u);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(map.cbegin(), map.cend());
    ASSERT_EQ(map.find("x"), map.end());
    ASSERT_EQ(map.erase("x"), 0u);
    ASSERT_EQ(map.take("x"), 0);
    map.clear();
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, HashInsertFindErase) {
    Hash map;
    map.insert("b", 2);
    map.insert("a", 1);
    map.insert("c", 3);
    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map.find("a")->second, 1);
    ASSERT_EQ(map.find("b")->second, 2);
    ASSERT_EQ(map.find("c")->second, 3);
    ASSERT_EQ(map.find("d"), map.end());
    const Hash& cmap = map;
    ASSERT_EQ(cmap.find("a")->second, 1);
    ASSERT_EQ(cmap.find("d"), cmap.cend());

    ASSERT_EQ(map.erase("b"), 1u);
    ASSERT_EQ(map.erase("b"), 0u);
    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map.find("b"), map.end());
    ASSERT_EQ(map.find("a")->second, 1);
}

TEST(gt_amap, HashInsertOverwrites) {
    Hash map;
    map.insert("k", 1);
    Hash::iterator it = map.insert("k", 2);
    ASSERT_EQ(it->first, "k");
    ASSERT_EQ(it->second, 2);
    ASSERT_EQ(map.size(), 1u);

    const std::pair<std::string, int> p("k", 3);
    map.insert(p);
    ASSERT_EQ(map.find("k")->second, 3);

    map.insert(std::pair<std::string, int>("k", 4));
    ASSERT_EQ(map.find("k")->second, 4);

    map.insert(std::string("k"), 5);
    ASSERT_EQ(map.find("k")->second, 5);
    ASSERT_EQ(map.size(), 1u);
}

TEST(gt_amap, HashContainsCountTryEmplace) {
    Hash map;
    ASSERT_FALSE(map.contains("a"));
    ASSERT_EQ(map.count("a"), 0u);

    std::pair<Hash::iterator, bool> r = map.try_emplace("a", 1);
    ASSERT_TRUE(r.second);
    ASSERT_EQ(r.first->second, 1);
    ASSERT_TRUE(map.contains("a"));
    ASSERT_EQ(map.count("a"), 1u);

    r = map.try_emplace("a", 2);
    ASSERT_FALSE(r.second);
    ASSERT_EQ(r.first->second, 1);
    ASSERT_EQ(map.find("a")->second, 1);

    ASSERT_TRUE(map.try_emplace("d").second);
    ASSERT_EQ(map.find("d")->second, 0);

    map_hash<std::string, std::string> m;
    m.insert("k", "keep");
    std::string value = "unused";
    std::pair<map_hash<std::string, std::string>::iterator, bool> hit = m.try_emplace("k", std::move(value));
    ASSERT_FALSE(hit.second);
    ASSERT_EQ(m.find("k")->second, "keep");
    ASSERT_EQ(value, "unused");
    ASSERT_TRUE(m.try_emplace(std::string("n"), "fresh").second);
    ASSERT_EQ(m.find("n")->second, "fresh");
    ASSERT_EQ(m.size(), 2u);
}

TEST(gt_amap, HashEmplaceAndSubscript) {
    map_hash<std::string, std::string> map;
    auto it = map.emplace("a", "one");
    ASSERT_EQ(it->first, "a");
    ASSERT_EQ(it->second, "one");
    map.emplace(std::string("b"), std::string("two"));
    ASSERT_EQ(map.size(), 2u);

    ASSERT_EQ(map["c"], "");
    ASSERT_EQ(map.size(), 3u);
    map["c"] = "three";
    ASSERT_EQ(map["c"], "three");
    ASSERT_EQ(map.size(), 3u);

    Hash numbers;
    numbers.insert("x", 1);
    Hash::iterator dup = numbers.emplace("x", 2);
    ASSERT_EQ(dup->second, 2);
    ASSERT_EQ(numbers.size(), 1u);
}

TEST(gt_amap, HashIterateVisitsEachOnce) {
    Hash map;
    std::unordered_map<std::string, int> ref;
    for (int i = 0; i < 500; ++i) {
        map.insert(key_of(i), i);
        ref[key_of(i)] = i;
    }
    size_t seen = 0;
    for (Hash::const_iterator it = map.cbegin(); it != map.cend(); ++it) {
        const std::unordered_map<std::string, int>::const_iterator found = ref.find(it->first);
        ASSERT_NE(found, ref.end());
        ASSERT_EQ(found->second, it->second);
        ++seen;
    }
    ASSERT_EQ(seen, ref.size());

    for (Hash::iterator it = map.begin(); it != map.end(); ++it) it->second += 10;
    ASSERT_EQ(conver<Flat>(map), ([&] {
                  std::unordered_map<std::string, int> out;
                  for (const auto& e : ref) out[e.first] = e.second + 10;
                  return out;
              }()));
}

TEST(gt_amap, HashEraseReturnsSuccessor) {
    Hash map;
    for (int i = 0; i < 300; ++i) map.insert(key_of(i), i);
    Hash::iterator it = map.begin();
    size_t steps = 0;
    while (it != map.end()) {
        Hash::iterator next = it;
        ++next;
        if (next == map.end()) {
            it = map.erase(it);
            ASSERT_EQ(it, map.end());
        } else {
            const std::string next_key = next->first;
            it = map.erase(it);
            ASSERT_NE(it, map.end());
            ASSERT_EQ(it->first, next_key);
        }
        ++steps;
    }
    ASSERT_EQ(steps, 300u);
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.begin(), map.end());
}

TEST(gt_amap, HashWeakHashChains) {
    using Weak = map_hash<int, int, false, weak_hash>;
    Weak map;
    for (int i = 0; i < 400; ++i) map.insert(i, i * 3);
    ASSERT_EQ(map.size(), 400u);
    for (int i = 0; i < 400; ++i) {
        Weak::iterator it = map.find(i);
        ASSERT_NE(it, map.end());
        ASSERT_EQ(it->second, i * 3);
    }

    for (int i = 0; i < 400; i += 3) ASSERT_EQ(map.erase(i), 1u);
    size_t seen = 0;
    for (Weak::const_iterator it = map.cbegin(); it != map.cend(); ++it) {
        ASSERT_NE(it->first % 3, 0);
        ++seen;
    }
    ASSERT_EQ(seen, map.size());

    Weak::iterator it = map.begin();
    while (it != map.end()) it = map.erase(it);
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.begin(), map.end());
}

TEST(gt_amap, HashCopyKeepsDemotedHomes) {
    using Weak = map_hash<int, int, false, weak_hash>;
    Weak map;
    map.insert(1, 10);
    map.insert(2, 20); // same home; demoted into L1 or a chain
    ASSERT_EQ(map.erase(1), 1u);
    Weak copy(map);
    ASSERT_EQ(copy.size(), 1u);
    ASSERT_TRUE(copy.contains(2));
    ASSERT_FALSE(copy.cannot_contain(2));
    ASSERT_EQ(copy.find(2)->second, 20);
    copy.insert(2, 21);
    ASSERT_EQ(copy.size(), 1u);
    ASSERT_EQ(copy.find(2)->second, 21);
}

TEST(gt_amap, HashGrowShrink) {
    const int n = 5000;
    Hash map;
    for (int i = 0; i < n; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), (size_t) n);
    for (int i = 0; i < n; ++i) ASSERT_EQ(map.find(key_of(i))->second, i);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = pass; i < n; i += 2) map.erase(key_of(i));
    }
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.begin(), map.end());

    for (int i = 0; i < 50; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), 50u);
    ASSERT_TRUE(map.contains(key_of(0)));
}

TEST(gt_amap, HashReserve) {
    Hash map;
    map.reserve(1000);
    for (int i = 0; i < 1000; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), 1000u);
    for (int i = 0; i < 1000; ++i) ASSERT_EQ(map.find(key_of(i))->second, i);
    map.reserve(10);
    ASSERT_EQ(map.size(), 1000u);
    map.reserve(0);
    ASSERT_EQ(map.size(), 1000u);
    map.clear();
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, HashPointerKeys) {
    int a = 1;
    int b = 2;
    int c = 3;
    map_hash<const int*, std::string> map;
    map.insert(&a, "a");
    map.insert(&b, "b");
    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map.find(&a)->second, "a");
    ASSERT_EQ(map.find(&c), map.end());
    ASSERT_EQ(map.erase(&a), 1u);
    ASSERT_FALSE(map.contains(&a));
    ASSERT_TRUE(map.contains(&b));
}

TEST(gt_amap, HashCopyMoveSwap) {
    Hash a;
    for (int i = 0; i < 100; ++i) a.insert(key_of(i), i);

    Hash b(a);
    ASSERT_EQ(b.size(), a.size());
    ASSERT_EQ(conver<Flat>(b), conver<Flat>(a));
    b.insert(key_of(50), -1);
    ASSERT_EQ(a.find(key_of(50))->second, 50);
    ASSERT_EQ(b.find(key_of(50))->second, -1);

    Hash c;
    c = a;
    ASSERT_EQ(conver<Flat>(c), conver<Flat>(a));

    Hash d(std::move(c));
    ASSERT_TRUE(c.empty());
    ASSERT_EQ(d.size(), a.size());

    Hash e;
    e.swap(d);
    ASSERT_TRUE(d.empty());
    ASSERT_EQ(conver<Flat>(e), conver<Flat>(a));

    a = a;
    ASSERT_EQ(a.size(), 100u);
}

TEST(gt_amap, HashOwningPointerValues) {
    using Own = map_hash<int, owned*, true>;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        m.insert(1, new owned(10));
        m.insert(2, new owned(20));
        ASSERT_EQ(owned::live, 2);

        m.insert(1, new owned(11));
        ASSERT_EQ(owned::live, 2);
        ASSERT_EQ(m.find(1)->second->value, 11);

        owned* spare = new owned(99);
        auto hit = m.try_emplace(2, spare);
        ASSERT_FALSE(hit.second);
        ASSERT_EQ(m.find(2)->second->value, 20);
        delete spare;

        Own copy(m);
        ASSERT_EQ(owned::live, 4);
        ASSERT_NE(copy.find(1)->second, m.find(1)->second);
        ASSERT_EQ(copy.find(1)->second->value, 11);

        m.erase(1);
        ASSERT_EQ(owned::live, 3);
        m.clear();
        ASSERT_EQ(owned::live, 2);
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, HashOwningStress) {
    using Own = map_hash<int, owned*, true>;
    uint_64 state = 0x0123456789ABCDEFULL;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        std::map<int, int> model;
        for (int step = 0; step < 3000; ++step) {
            const uint_64 r = next_rand(state);
            const int key = (int) (r % 64);
            switch (r % 4) {
            case 0:
            case 1: {
                const int value = (int) ((r >> 32) % 1000);
                m.insert(key, new owned(value));
                model[key] = value;
                break;
            }
            case 2: {
                owned* gone = m.take(key);
                const std::map<int, int>::iterator found = model.find(key);
                ASSERT_EQ(gone != nullptr, found != model.end());
                if (gone != nullptr) {
                    ASSERT_EQ(gone->value, found->second);
                    delete gone;
                    model.erase(found);
                }
                break;
            }
            default:
                ASSERT_EQ(m.erase(key), model.erase(key));
                break;
            }
            ASSERT_EQ(m.size(), model.size());
            ASSERT_EQ(owned::live, (int) model.size());
        }
        Own copy(m);
        ASSERT_EQ(owned::live, (int) (model.size() * 2));
        copy.clear();
        ASSERT_EQ(owned::live, (int) model.size());
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, HashTake) {
    using Own = map_hash<int, owned*, true>;
    Own m;
    m.insert(1, new owned(10));
    ASSERT_EQ(owned::live, 1);
    owned* got = m.take(1);
    ASSERT_NE(got, nullptr);
    ASSERT_EQ(got->value, 10);
    ASSERT_EQ(owned::live, 1);
    ASSERT_FALSE(m.contains(1));
    ASSERT_EQ(m.size(), 0u);
    delete got;
    ASSERT_EQ(owned::live, 0);
    ASSERT_EQ(m.take(9), nullptr);

    map_hash<std::string, int> plain;
    plain.insert("a", 7);
    ASSERT_EQ(plain.take("a"), 7);
    ASSERT_FALSE(plain.contains("a"));
    ASSERT_EQ(plain.take("a"), 0);
}

TEST(gt_amap, HashStdConversion) {
    std::unordered_map<std::string, int> src{{"a", 1}, {"b", 2}, {"c", 3}};
    Hash map = conver<Hash>(src);
    ASSERT_EQ(conver<Flat>(map), src);
    ASSERT_EQ(map.size(), 3u);

    Hash moved = conver<Hash>(std::unordered_map<std::string, int>(src));
    ASSERT_EQ(conver<Flat>(moved), src);

    std::unordered_map<std::string, int> back = conver<Flat>(std::move(moved));
    ASSERT_EQ(back, src);
    ASSERT_TRUE(moved.empty());
}

TEST(gt_amap, HashRandomAgainstStdUnorderedMap) {
    const int steps = 20000;
    Hash map;
    std::unordered_map<std::string, int> ref;
    uint_64 state = 0x9E3779B97F4A7C15ULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % 500));
        switch (r % 7) {
        case 0:
        case 1:
        case 2:
        case 3: {
            const int val = (int) (r >> 32);
            Hash::iterator a = map.insert(key, val);
            std::unordered_map<std::string, int>::iterator b = ref.insert_or_assign(key, val).first;
            ASSERT_EQ(a->first, b->first);
            ASSERT_EQ(a->second, b->second);
            break;
        }
        case 4:
            ASSERT_EQ(map.erase(key), ref.erase(key));
            break;
        case 5: {
            Hash::iterator a = map.find(key);
            std::unordered_map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == map.end(), b == ref.end());
            if (b != ref.end()) ASSERT_EQ(a->second, b->second);
            break;
        }
        default: {
            int& x = map[key];
            int& y = ref[key];
            ASSERT_EQ(x, y);
            ++x;
            ++y;
            break;
        }
        }
        ASSERT_EQ(map.size(), ref.size());
        if (0 == step % 251) ASSERT_EQ(conver<Flat>(map), ref);
    }
    ASSERT_EQ(conver<Flat>(map), ref);
}

TEST(gt_amap, HashStressChurn) {
    const int pool = 5000;
    const int steps = 150000;
    Hash map;
    std::unordered_map<std::string, int> ref;
    uint_64 state = 0xA5A5A5A55A5A5A5AULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % pool));
        switch (r % 12) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: {
            const int val = (int) (r >> 32);
            map.insert(key, val);
            ref.insert_or_assign(key, val);
            break;
        }
        case 6:
        case 7:
        case 8:
            ASSERT_EQ(map.erase(key), ref.erase(key));
            break;
        case 9:
        case 10: {
            Hash::iterator a = map.find(key);
            std::unordered_map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == map.end(), b == ref.end());
            break;
        }
        default:
            ++map[key];
            ++ref[key];
            break;
        }
        if (0 == step % 4099) ASSERT_EQ(map.size(), ref.size());
    }
    ASSERT_EQ(conver<Flat>(map), ref);
}

TEST(gt_amap, HashTinyTiers) {
    // Every size from the linear block through the bucket table into the two zones
    for (int n = 1; n <= 64; ++n) {
        Hash map;
        std::unordered_map<std::string, int> ref;
        for (int i = 0; i < n; ++i) {
            map.insert(key_of(i), i * 3);
            ref.insert_or_assign(key_of(i), i * 3);
        }
        ASSERT_EQ(map.size(), (size_t) n);
        ASSERT_EQ(conver<Flat>(map), ref);
        for (int i = 0; i < n; ++i) {
            ASSERT_EQ(map.find(key_of(i))->second, i * 3);
            ASSERT_FALSE(map.cannot_contain(key_of(i)));
        }
        ASSERT_FALSE(map.contains("absent_" + std::to_string(n)));

        // clear returns to the linear shape and the map refills there
        map.clear();
        ASSERT_TRUE(map.empty());
        ASSERT_EQ(map.begin(), map.end());
        map.insert("a", 1);
        map.insert("b", 2);
        ASSERT_EQ(map.size(), 2u);
        ASSERT_EQ(map.find("b")->second, 2);
        for (Hash::iterator it = map.begin(); it != map.end();) it = map.erase(it);
        ASSERT_TRUE(map.empty());
    }
}

TEST(gt_amap, HashWeakHashTiers) {
    // A constant low hash half must not collapse the bucket table, and the promotion into
    // two zones still spreads the keys by the high half
    using Weak = map_hash<int, int, false, weak_hash>;
    for (int n : {5, 8, 15, 16, 17, 30, 32, 33, 64, 129}) {
        Weak map;
        for (int i = 0; i < n; ++i) map.insert(i, i * 5);
        ASSERT_EQ(map.size(), (size_t) n);
        for (int i = 0; i < n; ++i) {
            Weak::iterator it = map.find(i);
            ASSERT_NE(it, map.end());
            ASSERT_EQ(it->second, i * 5);
        }
        for (int i = 0; i < n; i += 2) ASSERT_EQ(map.erase(i), 1u);
        for (int i = 0; i < n; ++i) ASSERT_EQ(map.contains(i), 0 != (i & 1));
        ASSERT_EQ(map.size(), (size_t) (n / 2));
    }
}

TEST(gt_amap, HashCopyEmpty) {
    Hash empty;
    Hash copy(empty);
    ASSERT_TRUE(copy.empty());
    copy.insert("a", 1);
    ASSERT_EQ(copy.size(), 1u);
    ASSERT_EQ(copy.find("a")->second, 1);

    Hash assigned;
    assigned.insert("x", 9);
    assigned = empty;
    ASSERT_TRUE(assigned.empty());
    assigned.insert("b", 2);
    ASSERT_EQ(assigned.size(), 1u);
    ASSERT_EQ(assigned.find("b")->second, 2);
}

TEST(gt_amap, Peek) {
    Tree tree;
    tree.insert("a", 1);
    tree.insert("b", 2);
    const Tree& view = tree;
    ASSERT_EQ(*view.peek("a"), 1);
    ASSERT_EQ(*view.peek("b"), 2);
    ASSERT_EQ(view.peek("absent"), nullptr);

    int* slot = tree.peek("a");
    ASSERT_NE(slot, nullptr);
    *slot = 11;
    ASSERT_EQ(*view.peek("a"), 11);
    ASSERT_EQ(tree.find("a")->second, 11);
}

TEST(gt_amap, HashPeek) {
    // Every shape from the linear block through the bucket table into the two zones
    for (int n = 1; n <= 64; ++n) {
        Hash map;
        for (int i = 0; i < n; ++i) map.insert(key_of(i), i);
        const Hash& view = map;
        for (int i = 0; i < n; ++i) {
            ASSERT_EQ(*view.peek(key_of(i)), i);
            ASSERT_NE(map.peek(key_of(i)), nullptr);
        }
        ASSERT_EQ(view.peek("absent_" + std::to_string(n)), nullptr);

        int* slot = map.peek(key_of(0));
        ASSERT_NE(slot, nullptr);
        *slot = 1000;
        ASSERT_EQ(map.find(key_of(0))->second, 1000);

        map.erase(key_of(0));
        ASSERT_EQ(view.peek(key_of(0)), nullptr);
    }
}

TEST(gt_amap, ReadEmpty) {
    Read map;
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.size(), 0u);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(map.cbegin(), map.cend());
    ASSERT_EQ(map.find("x"), map.end());
    ASSERT_EQ(map.erase("x"), 0u);
    ASSERT_EQ(map.take("x"), 0);
    map.clear();
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, ReadInsertFindErase) {
    Read map;
    map.insert("b", 2);
    map.insert("a", 1);
    map.insert("c", 3);
    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map.find("a")->second, 1);
    ASSERT_EQ(map.find("b")->second, 2);
    ASSERT_EQ(map.find("c")->second, 3);
    ASSERT_EQ(map.find("d"), map.end());
    const Read& cmap = map;
    ASSERT_EQ(cmap.find("a")->second, 1);
    ASSERT_EQ(cmap.find("d"), cmap.cend());

    ASSERT_EQ(map.erase("b"), 1u);
    ASSERT_EQ(map.erase("b"), 0u);
    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map.find("b"), map.end());
    ASSERT_EQ(map.find("a")->second, 1);
}

TEST(gt_amap, ReadInsertOverwrites) {
    Read map;
    map.insert("a", 1);
    map.insert("a", 2);
    ASSERT_EQ(map.size(), 1u);
    ASSERT_EQ(map.find("a")->second, 2);

    Read::value_type entry("b", 3);
    map.insert(entry);
    ASSERT_EQ(map.find("b")->second, 3);
    map.insert(Read::value_type("b", 4));
    ASSERT_EQ(map.find("b")->second, 4);

    std::string key = "c";
    map.insert(std::move(key), 5);
    ASSERT_EQ(map.find("c")->second, 5);
}

TEST(gt_amap, ReadContainsCountTryEmplace) {
    Read map;
    ASSERT_FALSE(map.contains("a"));
    ASSERT_EQ(map.count("a"), 0u);
    map.insert("a", 1);
    ASSERT_TRUE(map.contains("a"));
    ASSERT_EQ(map.count("a"), 1u);

    std::pair<Read::iterator, bool> hit = map.try_emplace("a", 9);
    ASSERT_FALSE(hit.second);
    ASSERT_EQ(hit.first->second, 1);

    std::pair<Read::iterator, bool> fresh = map.try_emplace("b", 2);
    ASSERT_TRUE(fresh.second);
    ASSERT_EQ(fresh.first->second, 2);
    ASSERT_EQ(map.size(), 2u);
}

TEST(gt_amap, ReadEmplaceAndSubscript) {
    Read map;
    map.emplace("a", 1);
    ASSERT_EQ(map.find("a")->second, 1);

    int& slot = map["b"];
    ASSERT_EQ(slot, 0);
    slot = 7;
    ASSERT_EQ(map.find("b")->second, 7);
    ASSERT_EQ(map.size(), 2u);
    map["b"] = 8;
    ASSERT_EQ(map.find("b")->second, 8);
}

TEST(gt_amap, ReadIterateVisitsEachOnce) {
    Read map;
    const int n = 300;
    for (int i = 0; i < n; ++i) map.insert(key_of(i), i * 3);
    std::vector<bool> seen((size_t) n, false);
    size_t count = 0;
    for (Read::const_iterator it = map.cbegin(); it != map.cend(); ++it) {
        const int idx = std::stoi(it->first.substr(1));
        ASSERT_FALSE(seen[(size_t) idx]);
        seen[(size_t) idx] = true;
        ASSERT_EQ(it->second, idx * 3);
        ++count;
    }
    ASSERT_EQ(count, (size_t) n);

    for (int i = 0; i < n; i += 2) map.erase(key_of(i));
    count = 0;
    for (Read::iterator it = map.begin(); it != map.end(); ++it) {
        ASSERT_EQ(std::stoi(it->first.substr(1)) % 2, 1);
        ++count;
    }
    ASSERT_EQ(count, (size_t) (n / 2));
}

TEST(gt_amap, ReadEraseReturnsSuccessor) {
    for (int n : {0, 1, 2, 4, 5, 17, 64}) {
        Read map;
        for (int i = 0; i < n; ++i) map.insert(key_of(i), i);
        int erased = 0;
        for (Read::iterator it = map.begin(); it != map.end();) {
            it = map.erase(it);
            ++erased;
        }
        ASSERT_EQ(erased, n);
        ASSERT_TRUE(map.empty());
    }

    Read map;
    map.insert("a", 1);
    Read::iterator next = map.erase(map.find("a"));
    ASSERT_EQ(next, map.end());
}

TEST(gt_amap, ReadEraseDuringScan) {
    // Homes 13/14/15/13 put key 29 past the table end; erasing 13 used to shift it into the
    // freed slot, so a forward scan met it twice
    using Id = map_read<int, int, false, id_hash>;
    Id map;
    map.reserve(4); // capacity 16
    map.insert(13, 1);
    map.insert(14, 2);
    map.insert(15, 3);
    map.insert(29, 4);

    std::vector<int> seen;
    for (Id::iterator it = map.begin(); it != map.end();) {
        seen.push_back(it->second);
        if (13 == it->first) it = map.erase(it);
        else ++it;
    }
    ASSERT_EQ(seen, (std::vector<int>{4, 1, 2, 3}));
    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map.erase(29), 1u);
    ASSERT_EQ(map.size(), 2u);

    // a tombstone is reused: refill past the erased slots and read everything back
    for (int i = 0; i < 40; ++i) map.insert(100 + i, 100 + i);
    ASSERT_EQ(map.size(), 42u);
    for (int i = 0; i < 40; ++i) ASSERT_EQ(*map.peek(100 + i), 100 + i);
    size_t count = 0;
    for (Id::iterator it = map.begin(); it != map.end(); ++it) ++count;
    ASSERT_EQ(count, map.size());
}

TEST(gt_amap, ReadEraseMiddleOfCluster) {
    // Every key homes to slot 0: one long cluster, so a middle erasure exercises the
    // backward shift over many entries
    using Weak = map_read<int, int, false, zero_low_hash>;
    const int n = 200;
    Weak map;
    for (int i = 0; i < n; ++i) map.insert(i, i * 5);
    for (int i = 0; i < n; ++i) {
        ASSERT_EQ(map.erase(i), 1u);
        ASSERT_EQ(map.size(), (size_t) (n - i - 1));
        for (int k = i + 1; k < n; ++k) ASSERT_EQ(*map.peek(k), k * 5);
        ASSERT_EQ(map.peek(i), nullptr);
    }
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, ReadGrowShrink) {
    const int n = 5000;
    Read map;
    for (int i = 0; i < n; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), (size_t) n);
    for (int i = 0; i < n; ++i) ASSERT_EQ(map.find(key_of(i))->second, i);

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = pass; i < n; i += 2) map.erase(key_of(i));
    }
    ASSERT_TRUE(map.empty());
    ASSERT_EQ(map.begin(), map.end());

    for (int i = 0; i < 50; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), 50u);
    ASSERT_TRUE(map.contains(key_of(0)));
}

TEST(gt_amap, ReadReserve) {
    Read map;
    map.reserve(1000);
    for (int i = 0; i < 1000; ++i) map.insert(key_of(i), i);
    ASSERT_EQ(map.size(), 1000u);
    for (int i = 0; i < 1000; ++i) ASSERT_EQ(map.find(key_of(i))->second, i);
    map.reserve(10);
    ASSERT_EQ(map.size(), 1000u);
    map.reserve(0);
    ASSERT_EQ(map.size(), 1000u);
    map.clear();
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, ReadPointerKeys) {
    int a = 1;
    int b = 2;
    int c = 3;
    map_read<const int*, std::string> map;
    map.insert(&a, "a");
    map.insert(&b, "b");
    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map.find(&a)->second, "a");
    ASSERT_EQ(map.find(&c), map.end());
    ASSERT_EQ(map.erase(&a), 1u);
    ASSERT_FALSE(map.contains(&a));
    ASSERT_TRUE(map.contains(&b));
}

TEST(gt_amap, ReadCopyMoveSwap) {
    Read a;
    for (int i = 0; i < 100; ++i) a.insert(key_of(i), i);

    Read b(a);
    ASSERT_EQ(b.size(), a.size());
    ASSERT_EQ(conver<Flat>(b), conver<Flat>(a));
    b.insert(key_of(50), -1);
    ASSERT_EQ(a.find(key_of(50))->second, 50);
    ASSERT_EQ(b.find(key_of(50))->second, -1);

    Read c;
    c = a;
    ASSERT_EQ(conver<Flat>(c), conver<Flat>(a));

    Read d(std::move(c));
    ASSERT_TRUE(c.empty());
    ASSERT_EQ(d.size(), a.size());

    Read e;
    e.swap(d);
    ASSERT_TRUE(d.empty());
    ASSERT_EQ(conver<Flat>(e), conver<Flat>(a));

    a = a;
    ASSERT_EQ(a.size(), 100u);

    Read empty;
    Read copy(empty);
    ASSERT_TRUE(copy.empty());
    copy.insert("a", 1);
    ASSERT_EQ(copy.size(), 1u);
    copy = empty;
    ASSERT_TRUE(copy.empty());
}

TEST(gt_amap, ReadTombstoneCopySwap) {
    // A copy or swap has to carry the tombstone count with the arrays; the answers stay
    // correct either way, so this pins the tombstone-carrying paths down through behaviour
    Read a;
    for (int i = 0; i < 100; ++i) a.insert(key_of(i), i);
    for (int i = 0; i < 50; ++i) a.erase(key_of(i));

    Read b;
    b.insert("x", 1);
    b = a;
    ASSERT_EQ(b.size(), 50u);
    for (int i = 50; i < 100; ++i) ASSERT_EQ(*b.peek(key_of(i)), i);
    ASSERT_EQ(b.peek(key_of(0)), nullptr);

    // refilling reuses the tombstones; every later lookup must still answer
    for (int i = 0; i < 50; ++i) b.insert(key_of(i), -i);
    ASSERT_EQ(b.size(), 100u);
    for (int i = 0; i < 50; ++i) ASSERT_EQ(*b.peek(key_of(i)), -i);
    for (int i = 50; i < 100; ++i) ASSERT_EQ(*b.peek(key_of(i)), i);

    Read c;
    for (int i = 0; i < 10; ++i) c.insert(key_of(i), i);
    c.erase(key_of(0));
    b.swap(c);
    ASSERT_EQ(b.size(), 9u);
    ASSERT_EQ(c.size(), 100u);
    for (int i = 1; i < 10; ++i) ASSERT_EQ(*b.peek(key_of(i)), i);
    for (int i = 0; i < 100; ++i) ASSERT_NE(c.peek(key_of(i)), nullptr);
    size_t seen = 0;
    for (Read::const_iterator it = c.cbegin(); it != c.cend(); ++it) ++seen;
    ASSERT_EQ(seen, c.size());
}

TEST(gt_amap, ReadOwningPointerValues) {
    using Own = map_read<int, owned*, true>;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        m.insert(1, new owned(10));
        m.insert(2, new owned(20));
        ASSERT_EQ(owned::live, 2);

        m.insert(1, new owned(11));
        ASSERT_EQ(owned::live, 2);
        ASSERT_EQ(m.find(1)->second->value, 11);

        owned* spare = new owned(99);
        auto hit = m.try_emplace(2, spare);
        ASSERT_FALSE(hit.second);
        ASSERT_EQ(m.find(2)->second->value, 20);
        delete spare;

        Own copy(m);
        ASSERT_EQ(owned::live, 4);
        ASSERT_NE(copy.find(1)->second, m.find(1)->second);
        ASSERT_EQ(copy.find(1)->second->value, 11);

        m.erase(1);
        ASSERT_EQ(owned::live, 3);
        m.clear();
        ASSERT_EQ(owned::live, 2);
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, ReadOwningStress) {
    using Own = map_read<int, owned*, true>;
    uint_64 state = 0x0123456789ABCDEFULL;
    ASSERT_EQ(owned::live, 0);
    {
        Own m;
        std::map<int, int> model;
        for (int step = 0; step < 3000; ++step) {
            const uint_64 r = next_rand(state);
            const int key = (int) (r % 64);
            switch (r % 4) {
            case 0:
            case 1: {
                const int value = (int) ((r >> 32) % 1000);
                m.insert(key, new owned(value));
                model[key] = value;
                break;
            }
            case 2: {
                owned* gone = m.take(key);
                const std::map<int, int>::iterator found = model.find(key);
                ASSERT_EQ(gone != nullptr, found != model.end());
                if (gone != nullptr) {
                    ASSERT_EQ(gone->value, found->second);
                    delete gone;
                    model.erase(found);
                }
                break;
            }
            default:
                ASSERT_EQ(m.erase(key), model.erase(key));
                break;
            }
            ASSERT_EQ(m.size(), model.size());
            ASSERT_EQ(owned::live, (int) model.size());
        }
        Own copy(m);
        ASSERT_EQ(owned::live, (int) (model.size() * 2));
        copy.clear();
        ASSERT_EQ(owned::live, (int) model.size());
    }
    ASSERT_EQ(owned::live, 0);
}

TEST(gt_amap, ReadTake) {
    using Own = map_read<int, owned*, true>;
    Own m;
    m.insert(1, new owned(10));
    ASSERT_EQ(owned::live, 1);
    owned* got = m.take(1);
    ASSERT_NE(got, nullptr);
    ASSERT_EQ(got->value, 10);
    ASSERT_EQ(owned::live, 1);
    ASSERT_FALSE(m.contains(1));
    ASSERT_EQ(m.size(), 0u);
    delete got;
    ASSERT_EQ(owned::live, 0);
    ASSERT_EQ(m.take(9), nullptr);

    Read plain;
    plain.insert("a", 7);
    ASSERT_EQ(plain.take("a"), 7);
    ASSERT_FALSE(plain.contains("a"));
    ASSERT_EQ(plain.take("a"), 0);
}

TEST(gt_amap, ReadConver) {
    std::vector<std::pair<std::string, int>> src;
    for (int i = 0; i < 200; ++i) src.emplace_back(key_of(i), i);
    Read map = conver<Read>(src);
    ASSERT_EQ(map.size(), 200u);
    for (int i = 0; i < 200; ++i) ASSERT_EQ(map.find(key_of(i))->second, i);

    Read copy = conver<Read>(map);
    ASSERT_EQ(copy.size(), 200u);
    ASSERT_EQ(conver<Flat>(copy), conver<Flat>(map));

    Hash hm;
    hm.insert("a", 1);
    hm.insert("b", 2);
    ASSERT_EQ(conver<Flat>(conver<Read>(hm)), conver<Flat>(hm));
}

TEST(gt_amap, ReadStdConversion) {
    std::unordered_map<std::string, int> src{{"a", 1}, {"b", 2}, {"c", 3}};
    Read map = conver<Read>(src);
    ASSERT_EQ(conver<Flat>(map), src);
    ASSERT_EQ(map.size(), 3u);

    Read moved_in = conver<Read>(std::unordered_map<std::string, int>(src));
    ASSERT_EQ(conver<Flat>(moved_in), src);

    std::unordered_map<std::string, int> back = conver<Flat>(std::move(map));
    ASSERT_EQ(back, src);
    ASSERT_TRUE(map.empty());
}

TEST(gt_amap, ReadPeek) {
    for (int n = 1; n <= 64; ++n) {
        Read map;
        for (int i = 0; i < n; ++i) map.insert(key_of(i), i);
        const Read& view = map;
        for (int i = 0; i < n; ++i) {
            ASSERT_EQ(*view.peek(key_of(i)), i);
            ASSERT_NE(map.peek(key_of(i)), nullptr);
        }
        ASSERT_EQ(view.peek("absent_" + std::to_string(n)), nullptr);

        int* slot = map.peek(key_of(0));
        ASSERT_NE(slot, nullptr);
        *slot = 1000;
        ASSERT_EQ(map.find(key_of(0))->second, 1000);

        map.erase(key_of(0));
        ASSERT_EQ(view.peek(key_of(0)), nullptr);
    }
}

TEST(gt_amap, ReadValueLookup) {
    Read map;
    ASSERT_EQ(map.value("absent"), 0);
    ASSERT_EQ(map.value("absent", -3), -3);
    map.insert("a", 1);
    ASSERT_EQ(map.value("a"), 1);
    ASSERT_EQ(map.value("a", -3), 1);
}

TEST(gt_amap, ReadRandomAgainstStdUnorderedMap) {
    const int steps = 20000;
    Read map;
    std::unordered_map<std::string, int> ref;
    uint_64 state = 0x9E3779B97F4A7C15ULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % 500));
        switch (r % 7) {
        case 0:
        case 1:
        case 2:
        case 3: {
            const int val = (int) (r >> 32);
            Read::iterator a = map.insert(key, val);
            std::unordered_map<std::string, int>::iterator b = ref.insert_or_assign(key, val).first;
            ASSERT_EQ(a->first, b->first);
            ASSERT_EQ(a->second, b->second);
            break;
        }
        case 4:
            ASSERT_EQ(map.erase(key), ref.erase(key));
            break;
        case 5: {
            Read::iterator a = map.find(key);
            std::unordered_map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == map.end(), b == ref.end());
            if (b != ref.end()) ASSERT_EQ(a->second, b->second);
            break;
        }
        default: {
            int& x = map[key];
            int& y = ref[key];
            ASSERT_EQ(x, y);
            ++x;
            ++y;
            break;
        }
        }
        ASSERT_EQ(map.size(), ref.size());
        if (0 == step % 251) ASSERT_EQ(conver<Flat>(map), ref);
    }
    ASSERT_EQ(conver<Flat>(map), ref);
}

TEST(gt_amap, ReadStressChurn) {
    const int pool = 5000;
    const int steps = 150000;
    Read map;
    std::unordered_map<std::string, int> ref;
    uint_64 state = 0xA5A5A5A55A5A5A5AULL;

    for (int step = 0; step < steps; ++step) {
        const uint_64 r = next_rand(state);
        const std::string key = key_of((int) (r % pool));
        switch (r % 12) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: {
            const int val = (int) (r >> 32);
            map.insert(key, val);
            ref.insert_or_assign(key, val);
            break;
        }
        case 6:
        case 7:
        case 8:
            ASSERT_EQ(map.erase(key), ref.erase(key));
            break;
        case 9:
        case 10: {
            Read::iterator a = map.find(key);
            std::unordered_map<std::string, int>::iterator b = ref.find(key);
            ASSERT_EQ(a == map.end(), b == ref.end());
            break;
        }
        default:
            ++map[key];
            ++ref[key];
            break;
        }
        if (0 == step % 4099) ASSERT_EQ(map.size(), ref.size());
    }
    ASSERT_EQ(conver<Flat>(map), ref);
}
