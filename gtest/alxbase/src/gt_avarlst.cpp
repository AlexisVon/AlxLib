/*****************************************************************/ /**
 * \file   gt_avarlst.cpp
 * \brief  Unit tests for varlst
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <climits>
#include <gtest/gtest.h>

static alx::varlst make_test_data() {
    return alx::varlst{
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

TEST(gt_avarlst, initial_construct) {
    alx::varlst var = make_test_data();
    ASSERT_EQ(var.size(), 47);

    auto iter = var.begin();
    ASSERT_EQ((*iter++).to<char>(), (char) (1));
    ASSERT_EQ((*iter++).to<char>(), (char) (SCHAR_MIN));
    ASSERT_EQ((*iter++).to<char>(), (char) (SCHAR_MAX));
}

TEST(gt_avarlst, copy_construct) {
    alx::varlst var = make_test_data();
    alx::varlst var_copy(var);
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var_copy.front().to<char>(), (char) (1));
}

TEST(gt_avarlst, operator_equal) {
    alx::varlst var = make_test_data();
    alx::varlst var_copy(var);

    ASSERT_NE(var, alx::varlst());
    ASSERT_EQ(var, var_copy);

    var_copy.front() = char(2);
    ASSERT_NE(var, var_copy);
    var_copy.front() = char(1);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarlst, move_construct) {
    alx::varlst var = make_test_data();
    alx::varlst var_copy(var);
    alx::varlst var_move(std::move(var_copy));
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarlst, copy_assign) {
    alx::varlst var = make_test_data();
    alx::varlst var_copy;
    ASSERT_TRUE(var_copy.empty());
    var_copy = var;
    ASSERT_EQ(var_copy.size(), 47);
    ASSERT_EQ(var, var_copy);
}

TEST(gt_avarlst, move_assign) {
    alx::varlst var = make_test_data();
    alx::varlst var_copy(var);
    alx::varlst var_move;
    ASSERT_TRUE(var_move.empty());
    var_move = std::move(var_copy);
    ASSERT_EQ(var_move.size(), 47);
    ASSERT_EQ(var, var_move);
}

TEST(gt_avarlst, vector_construct) {
    {
        alx::varlst lst(std::vector<int>(256, 127));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<int>(), 127);
        ASSERT_EQ(lst.back().to<int>(), 127);
    }
    {
        alx::varlst lst(std::vector<std::string>(256, "test"));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<std::string>(), "test");
        ASSERT_EQ(lst.back().to<std::string>(), "test");
    }
    {
        alx::varlst lst(std::vector<alx::bytes>(256, alx::bytes(256, 127U)));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<alx::bytes>().size(), 256U);
        ASSERT_EQ(lst.front().to<alx::bytes>()[0], 127U);
        ASSERT_EQ(lst.back().to<alx::bytes>().size(), 256U);
        ASSERT_EQ(lst.back().to<alx::bytes>()[255], 127U);
    }
}

TEST(gt_avarlst, list_construct) {
    {
        alx::varlst lst(std::list<int>(256, 127));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<int>(), 127);
        ASSERT_EQ(lst.back().to<int>(), 127);
    }
    {
        alx::varlst lst(std::list<std::string>(256, "test"));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<std::string>(), "test");
        ASSERT_EQ(lst.back().to<std::string>(), "test");
    }
    {
        alx::varlst lst(std::list<alx::bytes>(256, alx::bytes(256, 127U)));
        ASSERT_EQ(lst.size(), 256);
        ASSERT_EQ(lst.front().to<alx::bytes>().size(), 256U);
        ASSERT_EQ(lst.front().to<alx::bytes>()[0], 127U);
        ASSERT_EQ(lst.back().to<alx::bytes>().size(), 256U);
        ASSERT_EQ(lst.back().to<alx::bytes>()[255], 127U);
    }
}

TEST(gt_avarlst, collect_varvec) {
    alx::varlst var = make_test_data();
    alx::varlst lst(3, var);
    ASSERT_EQ(lst.size(), 3);
    ASSERT_EQ(lst.front(), var);
    ASSERT_EQ(*(++lst.begin()), var);
    ASSERT_EQ(lst.back(), var);
}

TEST(gt_avarlst, mixed_operations) {
    alx::varlst lst{"hello", "world"};
    ASSERT_EQ(lst.size(), 2);

    constexpr double PI = 3.14159265358979323846;
    lst.back() = PI;
    ASSERT_EQ(lst.front().to<std::string>(), "hello");
    ASSERT_EQ(lst.back().to<double>(), PI);
    lst.front() = lst.back();
    ASSERT_EQ(lst.front().to<double>(), PI);
    lst.push_back("test");
    ASSERT_EQ(lst.size(), 3);
    ASSERT_EQ(lst.back().to<std::string>(), "test");
    lst.pop_back();
    ASSERT_EQ(lst.size(), 2);
    lst.push_back(alx::variant());
    ASSERT_EQ(lst.size(), 3);
    ASSERT_TRUE(lst.back().null());
}
