/*****************************************************************/ /**
 * \file   gt_astring.cpp
 * \brief  Unit tests for strutil: format, split, join, find, encoding detection
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "astring.h"
#include <gtest/gtest.h>
#include <list>
#include <string>
#include <vector>

using namespace alx;
using namespace alx::strutil;

TEST(gt_astring, format_basic) {
    std::vector<std::string> args = {"hello", "world"};
    std::string result = alx::strutil::format("%1 %2", args);
    EXPECT_EQ(result, "hello world");
}

TEST(gt_astring, format_positional) {
    std::vector<std::string> args = {"first", "second"};
    std::string result = alx::strutil::format("%2 %1", args);
    EXPECT_EQ(result, "second first");
}

TEST(gt_astring, format_single) {
    std::vector<std::string> args = {"solo"};
    std::string result = alx::strutil::format("[%1]", args);
    EXPECT_EQ(result, "[solo]");
}

TEST(gt_astring, split_basic) {
    std::list<std::string> parts = split("a,b,c", ",");
    ASSERT_EQ(parts.size(), 3);
    auto it = parts.begin();
    EXPECT_EQ(*it++, "a");
    EXPECT_EQ(*it++, "b");
    EXPECT_EQ(*it, "c");
}

TEST(gt_astring, split_empty) {
    std::list<std::string> parts = split("", ",");
    EXPECT_TRUE(parts.empty() || parts.size() == 1);
}

TEST(gt_astring, split_no_match) {
    std::list<std::string> parts = split("hello", ",");
    ASSERT_EQ(parts.size(), 1);
    EXPECT_EQ(parts.front(), "hello");
}

TEST(gt_astring, split_empty_separator) {

    std::list<std::string> parts = split("abc", "");
    ASSERT_EQ(parts.size(), 5U);
    auto it = parts.begin();
    EXPECT_EQ(*it++, "");
    EXPECT_EQ(*it++, "a");
    EXPECT_EQ(*it++, "b");
    EXPECT_EQ(*it++, "c");
    EXPECT_EQ(*it++, "");

    std::list<std::string> one = split(",", "");
    ASSERT_EQ(one.size(), 3U);
    auto it2 = one.begin();
    EXPECT_EQ(*it2++, "");
    EXPECT_EQ(*it2++, ",");
    EXPECT_EQ(*it2++, "");

    EXPECT_EQ(split("", "").size(), 2U);
}

TEST(gt_astring, join_basic) {
    std::list<std::string> parts = {"a", "b", "c"};
    std::string result = join(parts, ",");
    EXPECT_EQ(result, "a,b,c");
}

TEST(gt_astring, join_empty) {
    std::list<std::string> parts;
    std::string result = join(parts, ",");
    EXPECT_EQ(result, "");
}

TEST(gt_astring, left_right) {
    std::string s = "hello world";
    EXPECT_EQ(left(s, 4), "hello");
    EXPECT_EQ(right(s, 6), "world");
}

TEST(gt_astring, left_beyond_length) {
    std::string s = "hi";

    EXPECT_EQ(left(s, 10), "");
    EXPECT_EQ(left(s, 1), "hi");
}

TEST(gt_astring, check_prefix) {
    EXPECT_TRUE(check("hello world", "hello"));
    EXPECT_FALSE(check("hello world", "world"));
}

TEST(gt_astring, find_rfind) {
    std::string s = "hello hello";
    EXPECT_EQ(find(s, "hello"), 0);
    EXPECT_GT(rfind(s, "hello"), 0);
    EXPECT_EQ(find(s, "xyz"), uint_64_npos);
}

TEST(gt_astring, compute_lps_basic) {
    std::vector<size_t> lps = compute_lps("ABABC");
    ASSERT_GE(lps.size(), 5);
    EXPECT_EQ(lps[0], 0);
    EXPECT_EQ(lps[1], 0);
    EXPECT_EQ(lps[2], 1);
    EXPECT_EQ(lps[3], 2);
    EXPECT_EQ(lps[4], 0);
}

TEST(gt_astring, stringfy) {
    EXPECT_EQ(stringfy("hello"), "\"hello\"");
}

TEST(gt_astring, remove_all) {
    std::string s = "a b c";
    remove_all(s, ' ');
    EXPECT_EQ(s, "abc");
}

TEST(gt_astring, detect_format_utf8) {
    std::string data = "hello";
    CODE_FORMAT fmt = detect_format(data.data(), data.size());

    EXPECT_NE(fmt & PROPERTY_MULTI, 0);
}

TEST(gt_astring, detect_format_utf8_bom) {
    const char bom[] = "\xEF\xBB\xBFhello";
    CODE_FORMAT fmt = detect_format(bom, sizeof(bom) - 1);
    EXPECT_NE(fmt & PROPERTY_BOM, 0);
    EXPECT_EQ(fmt & ENCODE_MASK, ENCODE_UTF8);
}

TEST(gt_astring, to_wstring_from_wstring) {
    std::string original = "hello";
    std::wstring wide = to_wstring(original);
    EXPECT_EQ(wide, L"hello");

    std::string back = from_wstring(wide);
    EXPECT_EQ(back, original);
}

TEST(gt_astring, code_conver_utf8_to_utf16le) {
    std::string utf8 = "hello";
    std::string result = code_conver(utf8.data(), utf8.size(), UTF16_LE, UTF8);

    EXPECT_GE(result.size(), utf8.size() * 2);
}

TEST(gt_astring, code_conver_identity) {
    std::string data = "test";
    std::string result = code_conver(data.data(), data.size(), UTF8, UTF8);
    EXPECT_EQ(result, data);
}

TEST(gt_astring, locale_format_valid) {
    CODE_FORMAT fmt = locale_format();

    EXPECT_NE(fmt & ENCODE_MASK, 0);
}
