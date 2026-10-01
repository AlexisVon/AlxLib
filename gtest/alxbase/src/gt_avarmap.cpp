/*****************************************************************/ /**
 * \file   gt_avarmap.cpp
 * \brief  Unit tests for varmap
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <climits>
#include <gtest/gtest.h>

static alx::varmap make_test_data() {
    return alx::varmap{
        {"c_val", (char) (1)}, {"c_min", (char) (SCHAR_MIN)}, {"c_max", (char) (SCHAR_MAX)}, {"s_val", (short) (1)}, {"s_min", (short) (SHRT_MIN)}, {"s_max", (short) (SHRT_MAX)}, {"i_val", (int) (1)}, {"i_min", (int) (INT_MIN)}, {"i_max", (int) (INT_MAX)}, {"ll_val", (long long) (1)}, {"ll_min", (long long) (LLONG_MIN)}, {"ll_max", (long long) (LLONG_MAX)}, {"uc_val", (unsigned char) (1)}, {"uc_min", (unsigned char) (0)}, {"uc_max", (unsigned char) (UCHAR_MAX)}, {"us_val", (unsigned short) (1)}, {"us_min", (unsigned short) (0)}, {"us_max", (unsigned short) (USHRT_MAX)}, {"ui_val", (unsigned int) (1)}, {"ui_min", (unsigned int) (0)}, {"ui_max", (unsigned int) (UINT_MAX)}, {"ull_val", (unsigned long long) (1)}, {"ull_min", (unsigned long long) (0)}, {"ull_max", (unsigned long long) (ULLONG_MAX)}, {"f_val", (float) (1)}, {"f_min", (float) (FLT_MIN)}, {"f_max", (float) (FLT_MAX)}, {"d_val", (double) (1)}, {"d_min", (double) (DBL_MIN)}, {"d_max", (double) (DBL_MAX)}, {"b_val", (bool) (1)}, {"b_true", (bool) (true)}, {"b_false", (bool) (false)}, {"str_empty", std::string()}, {"str_test", std::string("test")}, {"bytes_empty", alx::bytes()}, {"bytes_filled", alx::bytes(1024, 127U)}, {"vec_int_empty", std::vector<int>()}, {"vec_int_filled", std::vector<int>(1024, 127)}, {"vec_str_empty", std::vector<std::string>()}, {"vec_str_filled", std::vector<std::string>(1024, "test")}, {"vec_bytes_empty", std::vector<alx::bytes>()}, {"vec_bytes_filled", std::vector<alx::bytes>(1024, alx::bytes(256, 127U))}, {"list_int_empty", std::list<int>()}, {"list_int_filled", std::list<int>(1024, 127)}, {"list_str_empty", std::list<std::string>()}, {"list_str_filled", std::list<std::string>(1024, "test")}};
}

TEST(gt_avarmap, initial_construct) {
    alx::varmap var = make_test_data();
    ASSERT_EQ(var.size(), 47);

    ASSERT_EQ(var["c_val"].to<char>(), (char) (1));
    ASSERT_EQ(var["b_true"].to<bool>(), (bool) (true));
    ASSERT_EQ(var["b_false"].to<bool>(), (bool) (false));
    ASSERT_EQ(var["str_empty"].to<std::string>(), std::string());
    ASSERT_EQ(var["str_test"].to<std::string>(), std::string("test"));
    ASSERT_EQ(var["bytes_empty"].to<alx::bytes>(), alx::bytes());
    ASSERT_EQ(var["bytes_filled"].to<alx::bytes>(), alx::bytes(1024, 127U));
}

TEST(gt_avarmap, copy_construct) {
    alx::varmap var = make_test_data();
    alx::varmap var_copy(var);
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var_copy["c_val"].to<char>(), (char) (1));
    ASSERT_EQ(var_copy["str_test"].to<std::string>(), std::string("test"));
}

TEST(gt_avarmap, operator_equal) {
    alx::varmap var = make_test_data();
    alx::varmap var_copy(var);

    ASSERT_NE(var, alx::varmap());
    ASSERT_EQ(var, var_copy);

    var_copy["c_val"] = char(2);
    ASSERT_NE(var, var_copy);
    var_copy["c_val"] = char(1);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarmap, move_construct) {
    alx::varmap var = make_test_data();
    alx::varmap var_copy(var);
    alx::varmap var_move(std::move(var_copy));
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarmap, copy_assign) {
    alx::varmap var = make_test_data();
    alx::varmap var_copy;
    ASSERT_TRUE(var_copy.empty());
    var_copy = var;
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarmap, move_assign) {
    alx::varmap var = make_test_data();
    alx::varmap var_copy(var);
    alx::varmap var_move;
    ASSERT_TRUE(var_move.empty());
    var_move = std::move(var_copy);
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarmap, map_construct) {
    {
        alx::varmap vmap(std::map<std::string, int>{{"0", 127}, {"1", 255}, {"2", 511}});
        ASSERT_EQ(vmap.size(), 3);
        ASSERT_EQ(vmap["0"].to<int>(), 127);
        ASSERT_EQ(vmap["2"].to<int>(), 511);
    }
    {
        alx::varmap vmap(std::map<std::string, std::string>{{"0", "127"}, {"1", "255"}, {"2", "511"}});
        ASSERT_EQ(vmap.size(), 3);
        ASSERT_EQ(vmap["0"].to<std::string>(), "127");
        ASSERT_EQ(vmap["2"].to<std::string>(), "511");
    }
    {
        alx::varmap vmap(std::map<std::string, alx::bytes>{
            {"empty", alx::bytes()},
            {"filled128", alx::bytes(128, 127U)},
            {"filled256", alx::bytes(256, 255U)}});
        ASSERT_EQ(vmap.size(), 3);
        ASSERT_EQ(vmap["empty"].to<alx::bytes>(), alx::bytes());
        ASSERT_EQ(vmap["filled128"].to<alx::bytes>(), alx::bytes(128, 127U));
        ASSERT_EQ(vmap["filled256"].to<alx::bytes>(), alx::bytes(256, 255U));
    }
}

TEST(gt_avarmap, collect_varvec) {
    alx::varmap var = make_test_data();
    alx::varmap vmap{{"0", var}, {"1", var}, {"2", var}};
    ASSERT_EQ(vmap.size(), 3);
    ASSERT_EQ(vmap["0"], var);
    ASSERT_EQ(vmap["1"], var);
    ASSERT_EQ(vmap["2"], var);
}

TEST(gt_avarmap, iterator) {
    alx::varmap var = make_test_data();
    ASSERT_EQ(var.size(), 47);

    size_t i = 0;
    for (auto it = var.begin(); it != var.end(); ++it) i++;
    ASSERT_EQ(i, 47);

    i = 0;
    for (auto cit = var.cbegin(); cit != var.cend(); ++cit) i++;
    ASSERT_EQ(i, 47);

    i = 0;
    for (auto pair : var) i++;
    ASSERT_EQ(i, 47);
}

TEST(gt_avarmap, insert) {
    alx::varmap vmap;
    vmap.insert("0", 1);
    vmap.insert("1", 0);
    vmap.insert("2", "test");
    ASSERT_EQ(vmap.size(), 3);
    ASSERT_EQ(vmap["0"].to<int>(), 1);
    ASSERT_EQ(vmap["1"].to<int>(), 0);
    ASSERT_EQ(vmap["2"].to<std::string>(), "test");
    auto t = vmap.insert("2", 3.3);
    ASSERT_EQ(t.first.key(), "2");
    ASSERT_NE(t.first.value().to<double>(), 3.3);
    ASSERT_EQ(t.first.value().to<std::string>(), "test");
    ASSERT_FALSE(t.second);
    vmap["2"] = 3.3;
    ASSERT_EQ(vmap["2"].to<double>(), 3.3);
}

TEST(gt_avarmap, mixed_operations) {
    alx::varmap vmap{{"0", "hello"}, {"1", "world"}};
    ASSERT_EQ(vmap.size(), 2);

    constexpr double PI = 3.14159265358979323846;
    vmap["1"] = PI;
    ASSERT_EQ(vmap["0"].to<std::string>(), "hello");
    ASSERT_EQ(vmap["1"].to<double>(), PI);
    vmap["0"] = vmap["1"];
    ASSERT_EQ(vmap["0"].to<double>(), PI);
    vmap["2"] = "test";
    ASSERT_EQ(vmap.size(), 3);
    ASSERT_EQ(vmap["2"].to<std::string>(), "test");
    vmap.erase("2");
    ASSERT_EQ(vmap.size(), 2);
    vmap["null"] = alx::variant();
    ASSERT_TRUE(vmap["null"].null());
}
