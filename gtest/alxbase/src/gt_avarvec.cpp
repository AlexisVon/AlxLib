/*****************************************************************/ /**
 * \file   gt_avarvec.cpp
 * \brief  Unit tests for varvec
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <climits>
#include <gtest/gtest.h>

static alx::varvec make_test_data() {
    return alx::varvec{
        (char) (1), (char) (SCHAR_MIN), (char) (SCHAR_MAX),
        (short) (1), (short) (SHRT_MIN), (short) (SHRT_MAX),
        (int) (1), (int) (INT_MIN), (int) (INT_MAX),
        (long long) (1), (long long) (LLONG_MIN), (long long) (LLONG_MAX),
        (unsigned char) (1), (unsigned char) (0), (unsigned char) (UCHAR_MAX),
        (unsigned short) (1), (unsigned short) (0), (unsigned short) (USHRT_MAX),
        (unsigned int) (1), (unsigned int) (0), (unsigned int) (UINT_MAX),
        (unsigned long long) (1), (unsigned long long) (0), (unsigned long long) (ULLONG_MAX),
        (float) (1), (float) (FLT_MIN), (float) (FLT_MAX),
        (double) (1), (double) (DBL_MIN), (double) (DBL_MAX),
        (bool) (1), (bool) (true), (bool) (false),
        std::string(), std::string("test"),
        alx::bytes(), alx::bytes(1024, 127U),
        std::vector<int>(), std::vector<int>(1024, 127),
        std::vector<std::string>(), std::vector<std::string>(1024, "test"),
        std::vector<alx::bytes>(), std::vector<alx::bytes>(1024, alx::bytes(256, 127U)),
        std::list<int>(), std::list<int>(1024, 127),
        std::list<std::string>(), std::list<std::string>(1024, "test")};
}

TEST(gt_avarvec, initial_construct) {
    alx::varvec var = make_test_data();
    ASSERT_EQ(var.size(), 47);

    ASSERT_EQ(var[0].to<char>(), (char) (1));
    ASSERT_EQ(var[1].to<char>(), (char) (SCHAR_MIN));
    ASSERT_EQ(var[2].to<char>(), (char) (SCHAR_MAX));

    ASSERT_EQ(var[30].to<bool>(), (bool) (1));
    ASSERT_EQ(var[31].to<bool>(), (bool) (true));
    ASSERT_EQ(var[32].to<bool>(), (bool) (false));

    ASSERT_EQ(var[33].to<std::string>(), std::string());
    ASSERT_EQ(var[34].to<std::string>(), std::string("test"));

    ASSERT_EQ(var[35].to<alx::bytes>(), alx::bytes());
    ASSERT_EQ(var[36].to<alx::bytes>(), alx::bytes(1024, 127U));
}

TEST(gt_avarvec, copy_construct) {
    alx::varvec var = make_test_data();
    alx::varvec var_copy(var);
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var_copy[0].to<char>(), (char) (1));
    ASSERT_EQ(var_copy[46].to<std::list<std::string>>(), std::list<std::string>(1024, "test"));
}

TEST(gt_avarvec, operator_equal) {
    alx::varvec var = make_test_data();
    alx::varvec var_copy(var);

    ASSERT_NE(var, alx::varvec());
    ASSERT_EQ(var, var_copy);

    var_copy[0] = char(2);
    ASSERT_NE(var, var_copy);
    var_copy[0] = char(1);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarvec, move_construct) {
    alx::varvec var = make_test_data();
    alx::varvec var_copy(var);
    alx::varvec var_move(std::move(var_copy));
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarvec, copy_assign) {
    alx::varvec var = make_test_data();
    alx::varvec var_copy;
    ASSERT_TRUE(var_copy.empty());
    var_copy = var;
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarvec, move_assign) {
    alx::varvec var = make_test_data();
    alx::varvec var_copy(var);
    alx::varvec var_move;
    ASSERT_TRUE(var_move.empty());
    var_move = std::move(var_copy);
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarvec, vector_construct) {
    {
        alx::varvec vec(std::vector<int>(256, 127));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<int>(), 127);
        ASSERT_EQ(vec[255].to<int>(), 127);
    }
    {
        alx::varvec vec(std::vector<std::string>(256, "test"));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<std::string>(), "test");
        ASSERT_EQ(vec[127].to<std::string>(), "test");
    }
    {
        alx::varvec vec(std::vector<alx::bytes>(256, alx::bytes(256, 127U)));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<alx::bytes>().size(), 256U);
        ASSERT_EQ(vec[0].to<alx::bytes>()[0], 127U);
        ASSERT_EQ(vec[63].to<alx::bytes>().size(), 256U);
        ASSERT_EQ(vec[63].to<alx::bytes>()[255], 127U);
    }
}

TEST(gt_avarvec, list_construct) {
    {
        alx::varvec vec(std::list<int>(256, 127));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<int>(), 127);
        ASSERT_EQ(vec[255].to<int>(), 127);
    }
    {
        alx::varvec vec(std::list<std::string>(256, "test"));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<std::string>(), "test");
        ASSERT_EQ(vec[127].to<std::string>(), "test");
    }
    {
        alx::varvec vec(std::list<alx::bytes>(256, alx::bytes(256, 127U)));
        ASSERT_EQ(vec.size(), 256);
        ASSERT_EQ(vec[0].to<alx::bytes>().size(), 256U);
        ASSERT_EQ(vec[0].to<alx::bytes>()[0], 127U);
        ASSERT_EQ(vec[63].to<alx::bytes>().size(), 256U);
        ASSERT_EQ(vec[63].to<alx::bytes>()[255], 127U);
    }
}

TEST(gt_avarvec, collect_varvec) {
    alx::varvec var = make_test_data();
    alx::varvec vec(3, var);
    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[0], var);
    ASSERT_EQ(vec[1], var);
    ASSERT_EQ(vec[2], var);
}

TEST(gt_avarvec, mixed_operations) {
    alx::varvec vec{"hello", "world"};
    ASSERT_EQ(vec.size(), 2);

    constexpr double PI = 3.14159265358979323846;
    vec[1] = PI;
    ASSERT_EQ(vec[0].to<std::string>(), "hello");
    ASSERT_EQ(vec[1].to<double>(), PI);
    vec[0] = vec[1];
    ASSERT_EQ(vec[0].to<double>(), PI);
    vec.push_back("test");
    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[2].to<std::string>(), "test");
    vec.pop_back();
    ASSERT_EQ(vec.size(), 2);
    vec.push_back(alx::variant());
    ASSERT_EQ(vec.size(), 3);
    ASSERT_TRUE(vec[2].null());
}
