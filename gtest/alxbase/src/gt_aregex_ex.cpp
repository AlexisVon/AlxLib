/*****************************************************************/ /**
 * \file   gt_aregex_ex.cpp
 * \brief  Unit tests for regex_ex
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aregex_ex.h"

#include <gtest/gtest.h>

using namespace alx;

TEST(gt_aregex_ex, is_compliant) {
    regex_ex r("a+b");
    EXPECT_TRUE(r.is_valid());
    EXPECT_TRUE(r.is_compliant("aaab"));
    EXPECT_FALSE(r.is_compliant("aaac"));
}

TEST(gt_aregex_ex, find_and_find_all) {
    regex_ex r("[0-9]+");
    EXPECT_EQ(r.find("ab123cd"), "123");

    std::vector<std::string> all = r.find_all("a1b22c333");
    ASSERT_EQ(all.size(), 3U);
    EXPECT_EQ(all[0], "1");
    EXPECT_EQ(all[1], "22");
    EXPECT_EQ(all[2], "333");
}

TEST(gt_aregex_ex, replace) {
    regex_ex r("[0-9]+");
    EXPECT_EQ(r.replace("a1b2", "#"), "a#b2");
    EXPECT_EQ(r.replace_all("a1b2", "#"), "a#b#");
}

TEST(gt_aregex_ex, copy_assign) {
    regex_ex a("x");
    regex_ex b("y");
    a = b;
    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(a.is_compliant("y"));
    EXPECT_FALSE(a.is_compliant("x"));
}

TEST(gt_aregex_ex, copy_assign_from_invalid) {
    regex_ex a("a+");
    ASSERT_TRUE(a.is_valid());
    regex_ex b("(");
    ASSERT_FALSE(b.is_valid());

    a = b;

    EXPECT_FALSE(a.is_valid());
}
