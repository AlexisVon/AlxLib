/*****************************************************************/ /**
 * \file   gt_ascript_trace.cpp
 * \brief  Tests for call-stack trace compression (compress_trace / format_trace)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_trace.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace alx;
using namespace alx::script;

TEST(gt_ascript_trace, compress_trace_no_repeat) {
    std::vector<std::string> seq = {"a", "b", "c"};
    auto items = compress_trace(seq);
    ASSERT_EQ(3u, items.size());
    EXPECT_EQ("a", items[0].text);
    EXPECT_EQ("b", items[1].text);
    EXPECT_EQ("c", items[2].text);
}

TEST(gt_ascript_trace, compress_trace_repeating) {
    std::vector<std::string> seq = {"x", "x", "x"};
    auto items = compress_trace(seq);
    ASSERT_EQ(1u, items.size());
    EXPECT_EQ(3u, items[0].repeat);
}

TEST(gt_ascript_trace, compress_trace_empty) {
    std::vector<std::string> seq;
    auto items = compress_trace(seq);
    EXPECT_EQ(0u, items.size());
}

TEST(gt_ascript_trace, compress_trace_pattern_prefix) {
    std::vector<std::string> seq = {"a", "b", "a", "b", "c"};
    auto items = compress_trace(seq);

    EXPECT_EQ(2u, items[0].repeat);

    EXPECT_EQ("c", items[1].text);
}

TEST(gt_ascript_trace, format_trace_basic) {
    std::vector<trace_item> items;
    items.push_back(trace_item{"func1", 1, {}});
    items.push_back(trace_item{"func2", 1, {}});
    std::string out;
    format_trace(items, out, "");
    EXPECT_TRUE(out.find("func1") != std::string::npos);
    EXPECT_TRUE(out.find("func2") != std::string::npos);
}

TEST(gt_ascript_trace, format_trace_repeat) {
    std::vector<trace_item> children;
    children.push_back(trace_item{"f", 1, {}});
    std::vector<trace_item> items;
    items.push_back(trace_item{std::string(), 3, children});
    std::string out;
    format_trace(items, out, "");
    EXPECT_TRUE(out.find("(3x)") != std::string::npos);
    EXPECT_TRUE(out.find("f") != std::string::npos);
}

TEST(gt_ascript_trace, compress_trace_middle_run) {
    std::vector<std::string> seq = {"a", "b", "c", "c", "c", "c", "d"};
    auto items = compress_trace(seq);
    ASSERT_EQ(4u, items.size());
    EXPECT_EQ(4u, items[2].repeat);
    EXPECT_EQ(1u, items[2].children.size());
    EXPECT_EQ("c", items[2].children[0].text);
}

TEST(gt_ascript_trace, compress_trace_two_runs) {
    std::vector<std::string> seq = {"a", "a", "b", "b"};
    auto items = compress_trace(seq);
    ASSERT_EQ(2u, items.size());
    EXPECT_EQ(2u, items[0].repeat);
    EXPECT_EQ(2u, items[1].repeat);
}

TEST(gt_ascript_trace, compress_trace_nested_cycle) {
    std::vector<std::string> seq = {"a", "a", "b", "a", "a", "b", "a", "a", "b"};
    auto items = compress_trace(seq);
    ASSERT_EQ(1u, items.size());
    EXPECT_EQ(3u, items[0].repeat);
    ASSERT_EQ(2u, items[0].children.size());
    EXPECT_EQ(2u, items[0].children[0].repeat);
    EXPECT_EQ("b", items[0].children[1].text);
}

TEST(gt_ascript_trace, compress_trace_recursion_cycle) {
    std::vector<std::string> seq = {"[::] ?:5:9", "[::] rec:3:5"};
    for (int i = 0; i < 6; i++) seq.push_back("[::] rec:7:5");
    seq.push_back("[::] ?:13:5");
    seq.push_back("[::]:11:1");
    auto items = compress_trace(seq);
    ASSERT_EQ(5u, items.size());
    EXPECT_EQ("[::] ?:5:9", items[0].text);
    EXPECT_EQ(6u, items[2].repeat);
    EXPECT_EQ("[::] rec:7:5", items[2].children[0].text);
}

static void expand_trace(const std::vector<trace_item>& _items, std::vector<std::string>& _out) {
    for (auto& it : _items) {
        if (it.children.empty()) {
            for (size_t k = 0; k < it.repeat; k++) _out.push_back(it.text);
            continue;
        }
        std::vector<std::string> one;
        expand_trace(it.children, one);
        for (size_t k = 0; k < it.repeat; k++) _out.insert(_out.end(), one.begin(), one.end());
    }
}

TEST(gt_ascript_trace, compress_trace_round_trip) {
    const char* names[] = {"[::] a", "[::] b", "[::] c", "[::] d", "[::] e"};
    unsigned long long rng = 88172645463325252ULL;
    for (int iter = 0; iter < 4000; iter++) {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        size_t n = rng % 40, alpha_size = 2 + rng % 4;
        std::vector<std::string> seq;
        for (size_t i = 0; i < n; i++) {
            rng ^= rng << 13;
            rng ^= rng >> 7;
            rng ^= rng << 17;
            seq.push_back(names[rng % alpha_size]);
        }
        std::vector<std::string> flat;
        expand_trace(compress_trace(seq), flat);
        ASSERT_EQ(seq, flat) << "iteration " << iter;
    }
}
