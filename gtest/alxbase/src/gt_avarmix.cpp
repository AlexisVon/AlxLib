/*****************************************************************/ /**
 * \file   gt_avarmix.cpp
 * \brief  Unit tests for cross-type collection
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <gtest/gtest.h>

TEST(gt_avarmix, varvec_collect_varmap) {
    alx::varmap vmap{
        {"int", 127},
        {"string", "hello world"},
        {"double", 3.14},
        {"bool", true},
        {"char", 'a'},
        {"bytes", alx::bytes(32, 127U)}};
    alx::varvec vvec{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U), vmap};

    ASSERT_EQ(vmap.size(), 6);
    ASSERT_EQ(vvec.size(), 7);
    ASSERT_EQ(vvec.back(), vmap);

    vvec.push_back(vmap);
    ASSERT_EQ(vvec.size(), 8);
    ASSERT_EQ(vvec.back(), vmap);

    vvec.pop_back();
    ASSERT_EQ(vvec.size(), 7);

    alx::variant var = std::move(vvec), val;
    val = var.select({"0"});
    ASSERT_EQ(val, 127);
    val = var.select({"1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "string"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "bytes"});
    ASSERT_EQ(val, alx::bytes(32, 127U));
    val = var.select({"6", "not_exists"});
    ASSERT_EQ(val, alx::variant());
    val = var.select({"7"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, varmap_collect_varvec) {
    alx::varvec vvec{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U)};
    alx::varmap vmap{
        {"int", 127},
        {"string", "hello world"},
        {"double", 3.14},
        {"bool", true},
        {"char", 'a'},
        {"bytes", alx::bytes(32, 127U)},
        {"varvec", vvec}};

    ASSERT_EQ(vvec.size(), 6);
    ASSERT_EQ(vmap.size(), 7);
    ASSERT_EQ(vmap["varvec"], vvec);

    vmap.insert("varvec2", vvec);
    ASSERT_EQ(vmap.size(), 8);
    ASSERT_EQ(vmap["varvec2"], vvec);

    vmap.erase("varvec2");
    ASSERT_EQ(vmap.size(), 7);

    alx::variant var = std::move(vmap), val;
    val = var.select({"int"});
    ASSERT_EQ(val, 127);
    val = var.select({"string"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"varvec", "0"});
    ASSERT_EQ(val, 127);
    val = var.select({"varvec", "1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"varvec", "5"});
    ASSERT_EQ(val, alx::bytes(32, 127U));
    val = var.select({"nonexistent"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, varlst_collect_varmap) {
    alx::varmap vmap{
        {"int", 127},
        {"string", "hello world"},
        {"double", 3.14},
        {"bool", true},
        {"char", 'a'},
        {"bytes", alx::bytes(32, 127U)}};
    alx::varlst vlst{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U), vmap};

    ASSERT_EQ(vmap.size(), 6);
    ASSERT_EQ(vlst.size(), 7);
    ASSERT_EQ(vlst.back(), vmap);

    vlst.push_back(vmap);
    ASSERT_EQ(vlst.size(), 8);
    ASSERT_EQ(vlst.back(), vmap);

    vlst.pop_back();
    ASSERT_EQ(vlst.size(), 7);

    alx::variant var = std::move(vlst), val;
    val = var.select({"0"});
    ASSERT_EQ(val, 127);
    val = var.select({"1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "string"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "int"});
    ASSERT_EQ(val, 127);
    val = var.select({"6", "not_exists"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, varmap_collect_varlst) {
    alx::varlst vlst{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U)};
    alx::varmap vmap{
        {"int", 127},
        {"string", "hello world"},
        {"double", 3.14},
        {"bool", true},
        {"char", 'a'},
        {"bytes", alx::bytes(32, 127U)},
        {"varlst", vlst}};

    ASSERT_EQ(vlst.size(), 6);
    ASSERT_EQ(vmap.size(), 7);
    ASSERT_EQ(vmap["varlst"], vlst);

    vmap.insert("varlst2", vlst);
    ASSERT_EQ(vmap.size(), 8);
    ASSERT_EQ(vmap["varlst2"], vlst);

    vmap.erase("varlst2");
    ASSERT_EQ(vmap.size(), 7);

    alx::variant var = std::move(vmap), val;
    val = var.select({"int"});
    ASSERT_EQ(val, 127);
    val = var.select({"string"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"varlst", "0"});
    ASSERT_EQ(val, 127);
    val = var.select({"varlst", "1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"varlst", "5"});
    ASSERT_EQ(val, alx::bytes(32, 127U));
    val = var.select({"nonexistent"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, varvec_collect_varlst) {
    alx::varlst vlst{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U)};
    alx::varvec vvec{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U), vlst};

    ASSERT_EQ(vlst.size(), 6);
    ASSERT_EQ(vvec.size(), 7);
    ASSERT_EQ(vvec.back(), vlst);

    vvec.push_back(vlst);
    ASSERT_EQ(vvec.size(), 8);
    ASSERT_EQ(vvec.back(), vlst);

    vvec.pop_back();
    ASSERT_EQ(vvec.size(), 7);

    alx::variant var = std::move(vvec), val;
    val = var.select({"0"});
    ASSERT_EQ(val, 127);
    val = var.select({"1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "0"});
    ASSERT_EQ(val, 127);
    val = var.select({"6", "1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "5"});
    ASSERT_EQ(val, alx::bytes(32, 127U));
    val = var.select({"7"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, varlst_collect_varvec) {
    alx::varvec vvec{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U)};
    alx::varlst vlst{127, "hello world", 3.14, true, 'a', alx::bytes(32, 127U), vvec};

    ASSERT_EQ(vvec.size(), 6);
    ASSERT_EQ(vlst.size(), 7);
    ASSERT_EQ(vlst.back(), vvec);

    vlst.push_back(vvec);
    ASSERT_EQ(vlst.size(), 8);
    ASSERT_EQ(vlst.back(), vvec);

    vlst.pop_back();
    ASSERT_EQ(vlst.size(), 7);

    alx::variant var = std::move(vlst), val;
    val = var.select({"0"});
    ASSERT_EQ(val, 127);
    val = var.select({"1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "0"});
    ASSERT_EQ(val, 127);
    val = var.select({"6", "1"});
    ASSERT_EQ(val, "hello world");
    val = var.select({"6", "5"});
    ASSERT_EQ(val, alx::bytes(32, 127U));
    val = var.select({"7"});
    ASSERT_EQ(val, alx::variant());
}

TEST(gt_avarmix, select_rejects_malformed_index) {
    alx::varvec vvec{100, 200};
    alx::variant var = vvec;
    const alx::variant def((int) (-1));

    EXPECT_EQ(var.select({"99999999999999999999999"}, def), def);
    EXPECT_EQ(var.select({""}, def), def);
    EXPECT_EQ(var.select({"1x"}, def), def);
    EXPECT_EQ(var.select({"1"}, def), 200);
}
