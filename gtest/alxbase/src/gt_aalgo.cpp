/*****************************************************************/ /**
 * \file   gt_aalgo.cpp
 * \brief  Unit tests for search algorithms and PBKDF2
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aalgo.h"
#include "abytes.h"
#include <gtest/gtest.h>
#include <set>
#include <vector>

static const char* kTestData = "01234567898765432100123456789876543210";
static const alx::uint_64 kTestLen = 38;

TEST(gt_aalgo, find_basic) {

    EXPECT_EQ(alx::find(kTestData, kTestLen, "0", 1, 0, alx::uint_64_npos), 0U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "5", 1, 0, alx::uint_64_npos), 5U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "x", 1, 0, alx::uint_64_npos), alx::uint_64_npos);

    EXPECT_EQ(alx::find(kTestData, kTestLen, "01234", 5, 0, alx::uint_64_npos), 0U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "789", 3, 0, alx::uint_64_npos), 7U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "000", 3, 0, alx::uint_64_npos), alx::uint_64_npos);
}

TEST(gt_aalgo, find_with_range) {

    EXPECT_EQ(alx::find(kTestData, kTestLen, "0", 1, 1, alx::uint_64_npos), 18U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "0", 1, 19, alx::uint_64_npos), 19U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "0", 1, 20, alx::uint_64_npos), 37U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "0", 1, 1, 17), alx::uint_64_npos);

    EXPECT_EQ(alx::find(kTestData, kTestLen, "", 0, 0, alx::uint_64_npos), 0U);
    EXPECT_EQ(alx::find(kTestData, kTestLen, "", 0, 5, alx::uint_64_npos), 5U);
}

TEST(gt_aalgo, rfind_basic) {
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "0", 1, alx::uint_64_npos, 0), 37U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "5", 1, alx::uint_64_npos, 0), 32U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "x", 1, alx::uint_64_npos, 0), alx::uint_64_npos);

    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "210", 3, alx::uint_64_npos, 0), 35U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "000", 3, alx::uint_64_npos, 0), alx::uint_64_npos);
}

TEST(gt_aalgo, rfind_with_range) {
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "0", 1, 36, 0), 19U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "0", 1, 17, 0), 0U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "0", 1, 19, 19), 19U);

    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "", 0, alx::uint_64_npos, 0), 37U);
    EXPECT_EQ(alx::rfind(kTestData, kTestLen, "", 0, 10, 0), 10U);
}

TEST(gt_aalgo, find_enum_basic) {
    const char* enums[] = {"012", "345", "678", "987"};
    const void* ptrs[] = {enums[0], enums[1], enums[2], enums[3]};

    auto result = alx::find_enum(kTestData, kTestLen, ptrs, 4, 3, 0, alx::uint_64_npos);
    EXPECT_EQ(result.first, 0U);
    EXPECT_EQ(result.second, 0U);
}

TEST(gt_aalgo, find_enum_second_match) {
    const char* enums[] = {"xxx", "987", "654"};
    const void* ptrs[] = {enums[0], enums[1], enums[2]};

    auto result = alx::find_enum(kTestData, kTestLen, ptrs, 3, 3, 0, alx::uint_64_npos);
    EXPECT_EQ(result.first, 9U);
    EXPECT_EQ(result.second, 1U);
}

TEST(gt_aalgo, find_enum_with_range) {
    const char* enums[] = {"012", "345"};
    const void* ptrs[] = {enums[0], enums[1]};

    auto result = alx::find_enum(kTestData, kTestLen, ptrs, 2, 3, 1, alx::uint_64_npos);
    EXPECT_EQ(result.first, 3U);
    EXPECT_EQ(result.second, 1U);
}

TEST(gt_aalgo, find_enum_not_found) {
    const char* enums[] = {"aaa", "bbb", "ccc"};
    const void* ptrs[] = {enums[0], enums[1], enums[2]};

    auto result = alx::find_enum(kTestData, kTestLen, ptrs, 3, 3, 0, alx::uint_64_npos);
    EXPECT_EQ(result.first, alx::uint_64_npos);
    EXPECT_EQ(result.second, alx::uint_64_npos);
}

TEST(gt_aalgo, rfind_enum_basic) {
    const char* enums[] = {"012", "345", "678", "210"};
    const void* ptrs[] = {enums[0], enums[1], enums[2], enums[3]};

    auto result = alx::rfind_enum(kTestData, kTestLen, ptrs, 4, 3, alx::uint_64_npos, 0);
    EXPECT_EQ(result.first, 35U);
    EXPECT_EQ(result.second, 3U);
}

TEST(gt_aalgo, rfind_enum_with_range) {
    const char* enums[] = {"012", "345"};
    const void* ptrs[] = {enums[0], enums[1]};

    auto result = alx::rfind_enum(kTestData, kTestLen, ptrs, 2, 3, alx::uint_64_npos, 20);
    EXPECT_EQ(result.first, 22U);
    EXPECT_EQ(result.second, 1U);
}

TEST(gt_aalgo, rfind_enum_not_found) {
    const char* enums[] = {"xxx", "yyy"};
    const void* ptrs[] = {enums[0], enums[1]};

    auto result = alx::rfind_enum(kTestData, kTestLen, ptrs, 2, 3, alx::uint_64_npos, 0);
    EXPECT_EQ(result.first, alx::uint_64_npos);
    EXPECT_EQ(result.second, alx::uint_64_npos);
}

TEST(gt_aalgo, find_by_kmp) {
    using alx::find_func::find_by_kmp;
    EXPECT_EQ(find_by_kmp(kTestData, "01234", 5, 0, kTestLen), 0U);
    EXPECT_EQ(find_by_kmp(kTestData, "789", 3, 0, kTestLen), 7U);
    EXPECT_EQ(find_by_kmp(kTestData, "000", 3, 0, kTestLen), alx::uint_64_npos);
    EXPECT_EQ(find_by_kmp(kTestData, "0", 1, 1, kTestLen), 18U);
}

TEST(gt_aalgo, find_by_rabin_karp) {
    using alx::find_func::find_by_rabin_karp;
    EXPECT_EQ(find_by_rabin_karp(kTestData, "01234", 5, 0, kTestLen), 0U);
    EXPECT_EQ(find_by_rabin_karp(kTestData, "789", 3, 0, kTestLen), 7U);
    EXPECT_EQ(find_by_rabin_karp(kTestData, "000", 3, 0, kTestLen), alx::uint_64_npos);
    EXPECT_EQ(find_by_rabin_karp(kTestData, "0", 1, 1, kTestLen), 18U);
}

TEST(gt_aalgo, find_by_boyer_moore) {
    using alx::find_func::find_by_boyer_moore;
    EXPECT_EQ(find_by_boyer_moore(kTestData, "01234", 5, 0, kTestLen), 0U);
    EXPECT_EQ(find_by_boyer_moore(kTestData, "789", 3, 0, kTestLen), 7U);
    EXPECT_EQ(find_by_boyer_moore(kTestData, "000", 3, 0, kTestLen), alx::uint_64_npos);
    EXPECT_EQ(find_by_boyer_moore(kTestData, "012", 3, 19, kTestLen), 19U);
}

TEST(gt_aalgo, find_by_brute_force) {
    using alx::find_func::find_by_brute_force;
    EXPECT_EQ(find_by_brute_force(kTestData, "01234", 5, 0, kTestLen), 0U);
    EXPECT_EQ(find_by_brute_force(kTestData, "789", 3, 0, kTestLen), 7U);
    EXPECT_EQ(find_by_brute_force(kTestData, "000", 3, 0, kTestLen), alx::uint_64_npos);
}

TEST(gt_aalgo, rfind_by_kmp) {
    using alx::find_func::rfind_by_kmp;
    EXPECT_EQ(rfind_by_kmp(kTestData, "210", 3, 37, 0), 35U);
    EXPECT_EQ(rfind_by_kmp(kTestData, "000", 3, 37, 0), alx::uint_64_npos);
    EXPECT_EQ(rfind_by_kmp(kTestData, "0", 1, 36, 0), 19U);
}

TEST(gt_aalgo, rfind_by_rabin_karp) {
    using alx::find_func::rfind_by_rabin_karp;
    EXPECT_EQ(rfind_by_rabin_karp(kTestData, "210", 3, 37, 0), 35U);
    EXPECT_EQ(rfind_by_rabin_karp(kTestData, "000", 3, 37, 0), alx::uint_64_npos);
    EXPECT_EQ(rfind_by_rabin_karp(kTestData, "0", 1, 36, 0), 19U);
}

TEST(gt_aalgo, rfind_by_boyer_moore) {
    using alx::find_func::rfind_by_boyer_moore;
    EXPECT_EQ(rfind_by_boyer_moore(kTestData, "210", 3, 37, 0), 35U);
    EXPECT_EQ(rfind_by_boyer_moore(kTestData, "000", 3, 37, 0), alx::uint_64_npos);
    EXPECT_EQ(rfind_by_boyer_moore(kTestData, "012", 3, 36, 0), 19U);
}

TEST(gt_aalgo, rfind_by_brute_force) {
    using alx::find_func::rfind_by_brute_force;
    EXPECT_EQ(rfind_by_brute_force(kTestData, "210", 3, 37, 0), 35U);
    EXPECT_EQ(rfind_by_brute_force(kTestData, "000", 3, 37, 0), alx::uint_64_npos);
}

TEST(gt_aalgo, bytes_find) {
    alx::bytes b(kTestData, kTestLen);
    EXPECT_EQ(b.find("0", 1), 0U);
    EXPECT_EQ(b.find("0", 1, 1), 18U);
    EXPECT_EQ(b.find("0", 1, 1, 17), alx::uint_64_npos);
    EXPECT_EQ(b.find("789", 3, 0), 7U);
}

TEST(gt_aalgo, bytes_rfind) {
    alx::bytes b(kTestData, kTestLen);
    EXPECT_EQ(b.rfind("0", 1), 37U);
    EXPECT_EQ(b.rfind("0", 1, 17), 0U);
    EXPECT_EQ(b.rfind("210", 3, alx::uint_64_npos), 35U);
}

TEST(gt_aalgo, rand_distinct) {
    EXPECT_NE(alx::rand(), alx::rand());
}

TEST(gt_aalgo, rand_no_repeats) {
    std::set<alx::uint_64> seen;
    for (int i = 0; i < 256; ++i) seen.insert(alx::rand());
    EXPECT_EQ(256U, seen.size());
}

// PBKDF2-HMAC-SHA256 (RFC 8018); the published password/salt rows, c = 1, 2 and 4096
static std::string pbkdf2_hex(const std::string& _pswd, const std::string& _salt, alx::uint_32 _iterations, alx::uint_32 _dklen = 32) {
    if (0 == _iterations || 0 == _dklen) return std::string();
    const alx::bytes pswd(_pswd), salt(_salt);
    std::vector<alx::uint_8> out(_dklen);
    alx::pbkdf2_hmac_sha256(pswd.data(), pswd.size(), salt.data(), salt.size(), _iterations, out.data(), _dklen);
    return alx::bytes(out.data(), out.size()).to_hex();
}

TEST(gt_aalgo, pbkdf2_known_answers) {
    EXPECT_EQ(pbkdf2_hex("password", "salt", 1), "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    EXPECT_EQ(pbkdf2_hex("password", "salt", 2), "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    EXPECT_EQ(pbkdf2_hex("password", "salt", 4096), "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    EXPECT_EQ(pbkdf2_hex("passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40),
              "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");
}

TEST(gt_aalgo, pbkdf2_short_and_long_output) {
    EXPECT_EQ(pbkdf2_hex("password", "salt", 2, 16), "ae4d0c95af6b46d32d0adff928f06dd0");
    EXPECT_EQ(pbkdf2_hex("password", "salt", 2, 64),
              "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43830651afcb5c862f0b249bd031f7a67520d136470f5ec271ece91c07773253d9");
    EXPECT_EQ(pbkdf2_hex("", "", 1), "f7ce0b653d2d72a4108cf5abe912ffdd777616dbbb27a70e8204f3ae2d0f6fad");
}

TEST(gt_aalgo, pbkdf2_zero_parameters) {
    EXPECT_TRUE(pbkdf2_hex("password", "salt", 0).empty());
    EXPECT_TRUE(pbkdf2_hex("password", "salt", 1, 0).empty());
}
