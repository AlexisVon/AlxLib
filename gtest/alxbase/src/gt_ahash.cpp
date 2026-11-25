/*****************************************************************/ /**
 * \file   gt_ahash.cpp
 * \brief  auto_64 / ah_64 / wy_64 / fnv_32 / fnv_64: boundary, avalanche, bit-split, collision and dispatch tests
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ahash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace alx;

namespace {
    using H64 = hash::func<hash::ah_64, uint_64>;
    using W64 = hash::func<hash::wy_64, uint_64>;
    using A64 = hash::func<hash::auto_64, uint_64>;
    using F32 = hash::func<hash::fnv_32, uint_32>;
    using F64 = hash::func<hash::fnv_64, uint_64>;

    int popcount64(uint_64 _x) {
        int n = 0;
        while (_x) {
            _x &= _x - 1;
            n++;
        }
        return n;
    }

    uint_64 next_rand(uint_64& _state) {
        _state ^= _state << 13;
        _state ^= _state >> 7;
        _state ^= _state << 17;
        return _state;
    }

    const size_t s_lens[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 65, 127, 128, 129, 240, 256};

    std::vector<uint_8> make_key(size_t _len, uint_64 _seed) {
        std::vector<uint_8> buf(_len ? _len : 1);
        uint_64 state = _seed;
        for (auto& b : buf) b = (uint_8) next_rand(state);
        return buf;
    }
}

TEST(gt_ahash, Fnv32KnownVectors) {
    EXPECT_EQ(0x811C9DC5U, hash::fnv_32::hash(""));
    EXPECT_EQ(0xE40C292CU, hash::fnv_32::hash("a"));
    EXPECT_EQ(0xBF9CF968U, hash::fnv_32::hash("foobar"));
}

TEST(gt_ahash, Fnv64KnownVectors) {
    EXPECT_EQ(0xCBF29CE484222325ULL, hash::fnv_64::hash(""));
    EXPECT_EQ(0xAF63DC4C8601EC8CULL, hash::fnv_64::hash("a"));
    EXPECT_EQ(0x85944171F73967E8ULL, hash::fnv_64::hash("foobar"));
}

/// The constexpr entry stays usable at compile time (autility.h's fnv is untouched and separate)
static_assert(hash::fnv_32::hash("a") == 0xE40C292CU, "fnv_32 hash(const char*) is not constexpr");
static_assert(hash::fnv_64::hash("a") == 0xAF63DC4C8601EC8CULL, "fnv_64 hash(const char*) is not constexpr");

TEST(gt_ahash, FnvConstexprMatchesRuntime) {
    const char* cases[] = {"", "a", "foobar", "\x80", "\x80\xff\x01"};
    for (const char* s : cases) {
        const size_t n = std::strlen(s);
        EXPECT_EQ(hash::fnv_32::hash(s), hash::fnv_32::hash(s, n));
        EXPECT_EQ(hash::fnv_64::hash(s), hash::fnv_64::hash(s, n));
        EXPECT_EQ(hash::fnv_32::hash(s, n), F32::eval(std::string(s, n)));
        EXPECT_EQ(hash::fnv_64::hash(s, n), F64::eval(std::string(s, n)));
    }
}

TEST(gt_ahash, FnvIntegerPathMatchesLittleEndianBytes) {
    const uint_64 x = 0x0102030405060708ULL;
    uint_8 b[8];
    for (int i = 0; i < 8; i++) b[i] = (uint_8) (x >> (8 * i));
    EXPECT_EQ(hash::fnv_32::hash(x), hash::fnv_32::hash(b, 8));
    EXPECT_EQ(hash::fnv_64::hash(x), hash::fnv_64::hash(b, 8));
}

TEST(gt_ahash, IntegerAdaptersZeroExtendBits) {
    EXPECT_EQ(H64::eval((int) -1), H64::eval((uint_64) 0xFFFFFFFFULL));
    EXPECT_EQ(H64::eval((short) -1), H64::eval((uint_64) 0xFFFFULL));
    EXPECT_EQ(H64::eval((unsigned char) 0xFF), H64::eval((uint_64) 0xFFULL));
    EXPECT_EQ(H64::eval((long long) -1), H64::eval((uint_64) 0xFFFFFFFFFFFFFFFFULL));
    EXPECT_EQ(F32::eval((int) -1), F32::eval((uint_64) 0xFFFFFFFFULL));
    EXPECT_EQ(F64::eval((int) -1), F64::eval((uint_64) 0xFFFFFFFFULL));
}

TEST(gt_ahash, FloatingAdaptersUseBitPattern) {
    const float f = 1.0f;
    const double d = 1.0;
    uint_32 fb = 0;
    uint_64 db = 0;
    std::memcpy(&fb, &f, sizeof(fb));
    std::memcpy(&db, &d, sizeof(db));
    EXPECT_EQ(H64::eval(f), H64::eval((uint_64) fb));
    EXPECT_EQ(H64::eval(d), H64::eval(db));
    EXPECT_EQ(H64::eval(0.0), H64::eval((uint_64) 0));
    EXPECT_NE(H64::eval(0.0), H64::eval(-0.0));
}

TEST(gt_ahash, ZeroLiteralResolvesThroughFunc) {
    EXPECT_EQ(H64::eval(0), H64::eval((uint_64) 0));
    EXPECT_EQ(F32::eval(0), F32::eval((uint_64) 0));
    EXPECT_EQ(F64::eval(0), F64::eval((uint_64) 0));
}

TEST(gt_ahash, DataSizeTypesGoThroughTheBytePrimitive) {
    struct blob {
        const uint_8* p;
        size_t n;
        const uint_8* data() const noexcept { return p; }
        size_t size() const noexcept { return n; }
    };
    uint_8 raw[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const blob b{raw, sizeof(raw)};
    EXPECT_EQ(hash::ah_64::hash(raw, sizeof(raw)), H64::eval(b));
    EXPECT_EQ(hash::fnv_32::hash(raw, sizeof(raw)), F32::eval(b));
    EXPECT_EQ(hash::fnv_64::hash(raw, sizeof(raw)), F64::eval(b));
}

TEST(gt_ahash, Ah64BoundaryDeterministic) {
    for (const size_t len : s_lens) {
        const std::vector<uint_8> a = make_key(len, 0x1234ULL ^ len);
        EXPECT_EQ(hash::ah_64::hash(a.data(), len), hash::ah_64::hash(a.data(), len)) << "len=" << len;
        EXPECT_EQ(hash::ah_64::hash(a.data(), len), H64::eval(std::string((const char*) a.data(), len)))
            << "len=" << len;
        if (len) {
            const std::vector<uint_8> b = make_key(len, 0x5678ULL ^ len);
            EXPECT_NE(hash::ah_64::hash(a.data(), len), hash::ah_64::hash(b.data(), len)) << "len=" << len;
        }
    }
    EXPECT_EQ(hash::ah_64::hash((const void*) nullptr, 0), H64::eval(""));
}

TEST(gt_ahash, Ah64DistinctLengthsAndContents) {
    std::vector<uint_64> seen;
    for (const size_t len : s_lens) {
        const std::vector<uint_8> a = make_key(len, 0xABCDULL ^ len);
        seen.push_back(hash::ah_64::hash(a.data(), len));
    }
    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen.end(), std::unique(seen.begin(), seen.end()));

    const char* variants[] = {"ab", "ab\0", "ab\0\0", "a\0b"};
    const size_t variant_lens[] = {2, 3, 4, 3};
    uint_64 hs[4];
    for (int i = 0; i < 4; i++) hs[i] = hash::ah_64::hash(variants[i], variant_lens[i]);
    std::sort(hs, hs + 4);
    EXPECT_EQ(hs + 4, std::unique(hs, hs + 4));
}

TEST(gt_ahash, Ah64Avalanche) {
    const int rounds = 16;
    for (const size_t len : s_lens) {
        if (0 == len) continue;
        uint_64 flipped = 0;
        uint_64 total = 0;
        for (int r = 0; r < rounds; r++) {
            std::vector<uint_8> buf = make_key(len, 0x5678ULL ^ (len << 8) ^ (uint_64) r);
            const uint_64 base = hash::ah_64::hash(buf.data(), len);
            const size_t bits = std::min<size_t>(len * 8, 64);
            for (size_t k = 0; k < bits; k++) {
                buf[k / 8] ^= (uint_8) (1U << (k % 8));
                flipped += (uint_64) popcount64(hash::ah_64::hash(buf.data(), len) ^ base);
                total += 64;
                buf[k / 8] ^= (uint_8) (1U << (k % 8));
            }
        }
        const double rate = (double) flipped / (double) total;
        EXPECT_NEAR(0.5, rate, 0.03) << "len=" << len;
    }
}

TEST(gt_ahash, Ah64BitSplitIndependence) {
    const size_t samples = 100000;
    std::vector<uint_64> same(32 * 32, 0);
    uint_64 state = 0xDEADBEEFCAFEBABEULL;
    std::vector<uint_8> buf(16);
    for (size_t s = 0; s < samples; s++) {
        for (auto& b : buf) b = (uint_8) next_rand(state);
        const uint_64 h = hash::ah_64::hash(buf.data(), buf.size());
        const uint_32 low = (uint_32) h;
        const uint_32 high = (uint_32) (h >> 32);
        for (int i = 0; i < 32; i++) {
            const bool li = ((low >> i) & 1U) != 0;
            for (int j = 0; j < 32; j++) same[(size_t) (i * 32 + j)] += (li == (((high >> j) & 1U) != 0));
        }
    }
    for (size_t i = 0; i < same.size(); i++) {
        const double rate = (double) same[i] / (double) samples;
        EXPECT_NEAR(0.5, rate, 0.01) << "pair=" << i;
    }
}

TEST(gt_ahash, Ah64CollisionSample) {
    const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t charset_n = std::strlen(charset);
    const size_t samples = 1000000;
    std::vector<std::string> keys;
    keys.reserve(samples);
    uint_64 state = 0x0123456789ABCDEFULL;
    std::string key(8, ' ');
    for (size_t s = 0; s < samples; s++) {
        for (size_t i = 0; i < key.size(); i++) key[i] = charset[next_rand(state) % charset_n];
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    std::vector<uint_64> hs;
    hs.reserve(keys.size());
    for (const std::string& k : keys) hs.push_back(H64::eval(k));
    std::sort(hs.begin(), hs.end());
    const size_t collisions = (size_t) (hs.end() - std::unique(hs.begin(), hs.end()));
    EXPECT_EQ(0U, collisions);
}

TEST(gt_ahash, Ah64IntegerMixIsDeterministic) {
    for (uint_64 x = 0; x < 1000; x++) {
        EXPECT_NE(x, hash::ah_64::hash(x));
    }
    // is_hard() depends on the build (ALXLIB_AHASH_NO_HW) and the CPU; the tests only go through
    // the public interface, so the concrete path is whatever this build selected.
    (void) hash::ah_64::is_hard();
    EXPECT_FALSE(hash::fnv_32::is_hard());
    EXPECT_FALSE(hash::fnv_64::is_hard());
}

// wyhash final v4.3 with seed 0 and the default secret: the reference answers are pinned so a
// future edit to the mixing cannot move them unnoticed
TEST(gt_ahash, Wy64KnownVectors) {
    EXPECT_EQ(0x93228A4DE0EEC5A2ULL, W64::eval(""));
    EXPECT_EQ(0xACED12527FE5BFF8ULL, W64::eval("a"));
    EXPECT_EQ(0x3AB768B7140588A1ULL, W64::eval("foobar"));
    EXPECT_EQ(0x88DE385A856CFB95ULL, hash::wy_64::hash("0123456789abcdef", 16));
    EXPECT_EQ(0x14F37288A5F8073AULL, hash::wy_64::hash("0123456789abcdefg", 17));
    EXPECT_EQ(0xFA303ABC2B1D7630ULL, hash::wy_64::hash((uint_64) 0));
    EXPECT_EQ(0x8FD90E7337AB042DULL, hash::wy_64::hash((uint_64) 1));
    EXPECT_EQ(0x7DFB2F243CF91FD7ULL, hash::wy_64::hash(0xDEADBEEFCAFEBABEULL));
}

TEST(gt_ahash, Wy64BoundaryDeterministic) {
    for (const size_t len : s_lens) {
        const std::vector<uint_8> a = make_key(len, 0x1234ULL ^ len);
        EXPECT_EQ(hash::wy_64::hash(a.data(), len), hash::wy_64::hash(a.data(), len)) << "len=" << len;
        EXPECT_EQ(hash::wy_64::hash(a.data(), len), W64::eval(std::string((const char*) a.data(), len)))
            << "len=" << len;
        if (len) {
            const std::vector<uint_8> b = make_key(len, 0x5678ULL ^ len);
            EXPECT_NE(hash::wy_64::hash(a.data(), len), hash::wy_64::hash(b.data(), len)) << "len=" << len;
        }
    }
    EXPECT_EQ(hash::wy_64::hash((const void*) nullptr, 0), W64::eval(""));
}

TEST(gt_ahash, Wy64DistinctLengthsAndContents) {
    std::vector<uint_64> seen;
    for (const size_t len : s_lens) {
        const std::vector<uint_8> a = make_key(len, 0xABCDULL ^ len);
        seen.push_back(hash::wy_64::hash(a.data(), len));
    }
    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(seen.end(), std::unique(seen.begin(), seen.end()));

    const char* variants[] = {"ab", "ab\0", "ab\0\0", "a\0b"};
    const size_t variant_lens[] = {2, 3, 4, 3};
    uint_64 hs[4];
    for (int i = 0; i < 4; i++) hs[i] = hash::wy_64::hash(variants[i], variant_lens[i]);
    std::sort(hs, hs + 4);
    EXPECT_EQ(hs + 4, std::unique(hs, hs + 4));
}

TEST(gt_ahash, Wy64Avalanche) {
    const int rounds = 16;
    for (const size_t len : s_lens) {
        if (0 == len) continue;
        uint_64 flipped = 0;
        uint_64 total = 0;
        for (int r = 0; r < rounds; r++) {
            std::vector<uint_8> buf = make_key(len, 0x9ABCULL ^ (len << 8) ^ (uint_64) r);
            const uint_64 base = hash::wy_64::hash(buf.data(), len);
            const size_t bits = std::min<size_t>(len * 8, 64);
            for (size_t k = 0; k < bits; k++) {
                buf[k / 8] ^= (uint_8) (1U << (k % 8));
                flipped += (uint_64) popcount64(hash::wy_64::hash(buf.data(), len) ^ base);
                total += 64;
                buf[k / 8] ^= (uint_8) (1U << (k % 8));
            }
        }
        const double rate = (double) flipped / (double) total;
        EXPECT_NEAR(0.5, rate, 0.03) << "len=" << len;
    }
}

TEST(gt_ahash, Wy64BitSplitIndependence) {
    const size_t samples = 100000;
    std::vector<uint_64> same(32 * 32, 0);
    uint_64 state = 0xFEDCBA9876543210ULL;
    std::vector<uint_8> buf(16);
    for (size_t s = 0; s < samples; s++) {
        for (auto& b : buf) b = (uint_8) next_rand(state);
        const uint_64 h = hash::wy_64::hash(buf.data(), buf.size());
        const uint_32 low = (uint_32) h;
        const uint_32 high = (uint_32) (h >> 32);
        for (int i = 0; i < 32; i++) {
            const bool li = ((low >> i) & 1U) != 0;
            for (int j = 0; j < 32; j++) same[(size_t) (i * 32 + j)] += (li == (((high >> j) & 1U) != 0));
        }
    }
    for (size_t i = 0; i < same.size(); i++) {
        const double rate = (double) same[i] / (double) samples;
        EXPECT_NEAR(0.5, rate, 0.01) << "pair=" << i;
    }
}

TEST(gt_ahash, Wy64CollisionSample) {
    const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t charset_n = std::strlen(charset);
    const size_t samples = 1000000;
    std::vector<std::string> keys;
    keys.reserve(samples);
    uint_64 state = 0x1029384756ABCDEFULL;
    std::string key(8, ' ');
    for (size_t s = 0; s < samples; s++) {
        for (size_t i = 0; i < key.size(); i++) key[i] = charset[next_rand(state) % charset_n];
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    std::vector<uint_64> hs;
    hs.reserve(keys.size());
    for (const std::string& k : keys) hs.push_back(W64::eval(k));
    std::sort(hs.begin(), hs.end());
    const size_t collisions = (size_t) (hs.end() - std::unique(hs.begin(), hs.end()));
    EXPECT_EQ(0U, collisions);
}

TEST(gt_ahash, Wy64IntegerMixIsDeterministic) {
    for (uint_64 x = 0; x < 1000; x++) {
        EXPECT_NE(x, hash::wy_64::hash(x));
    }
    EXPECT_FALSE(hash::wy_64::is_hard());
    EXPECT_FALSE(hash::wy_64::is_constexpr());
}

TEST(gt_ahash, Auto64DispatchBoundary) {
    for (const size_t len : s_lens) {
        const std::vector<uint_8> a = make_key(len, 0xA5A5ULL ^ len);
        const uint_64 want = len <= 64 ? hash::wy_64::hash(a.data(), len) : hash::ah_64::hash(a.data(), len);
        EXPECT_EQ(want, hash::auto_64::hash(a.data(), len)) << "len=" << len;
        EXPECT_EQ(want, A64::eval(std::string((const char*) a.data(), len))) << "len=" << len;
    }
    // the two sides of the switch, explicitly
    const std::vector<uint_8> short64 = make_key(64, 0x1111ULL);
    const std::vector<uint_8> long65 = make_key(65, 0x2222ULL);
    EXPECT_EQ(hash::wy_64::hash(short64.data(), 64), hash::auto_64::hash(short64.data(), 64));
    EXPECT_EQ(hash::ah_64::hash(long65.data(), 65), hash::auto_64::hash(long65.data(), 65));
    EXPECT_EQ(hash::wy_64::hash((const void*) nullptr, 0), hash::auto_64::hash((const void*) nullptr, 0));
}

TEST(gt_ahash, Auto64IntegerIsWy64) {
    uint_64 state = 0x3141592653589793ULL;
    for (int i = 0; i < 1000; i++) {
        const uint_64 x = next_rand(state);
        EXPECT_EQ(hash::wy_64::hash(x), hash::auto_64::hash(x));
    }
    EXPECT_EQ(hash::wy_64::hash((uint_64) 0), hash::auto_64::hash((uint_64) 0));
}

TEST(gt_ahash, Auto64PathReporting) {
    EXPECT_EQ(hash::ah_64::is_hard(), hash::auto_64::is_hard());
    EXPECT_FALSE(hash::auto_64::is_constexpr());
}
