/*****************************************************************/ /**
 * \file   gt_avariant.cpp
 * \brief  Unit tests for variant
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <climits>
#include <gtest/gtest.h>

#define IMPL_CONSTRUCT_TEST_BY_COPY_VALUE(VALUE)                     \
    {                                                                \
        alx::variant var_##VALUE(VALUE);                             \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_CONSTRUCT_TEST_BY_MOVE_VALUE(VALUE)                     \
    {                                                                \
        decltype(VALUE) VALUE##_copy(VALUE);                         \
        alx::variant var_##VALUE(std::move(VALUE##_copy));           \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_CONSTRUCT_TEST_BY_COPY_OBJECT(VALUE)                    \
    {                                                                \
        alx::variant var_##VALUE##_obj(VALUE);                       \
        alx::variant var_##VALUE(var_##VALUE##_obj);                 \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_CONSTRUCT_TEST_BY_MOVE_OBJECT(VALUE)                    \
    {                                                                \
        alx::variant var_##VALUE##_obj(VALUE);                       \
        alx::variant var_##VALUE(std::move(var_##VALUE##_obj));      \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_CONSTRUCT_TEST(VALUE)             \
    IMPL_CONSTRUCT_TEST_BY_COPY_VALUE(VALUE);  \
    IMPL_CONSTRUCT_TEST_BY_MOVE_VALUE(VALUE);  \
    IMPL_CONSTRUCT_TEST_BY_COPY_OBJECT(VALUE); \
    IMPL_CONSTRUCT_TEST_BY_MOVE_OBJECT(VALUE)

TEST(gt_avariant, construct_builtin) {
    char ch_val = 1, ch_min_val = SCHAR_MIN, ch_max_val = SCHAR_MAX;
    IMPL_CONSTRUCT_TEST(ch_val);
    IMPL_CONSTRUCT_TEST(ch_min_val);
    IMPL_CONSTRUCT_TEST(ch_max_val);

    short sh_val = 1, sh_min_val = SHRT_MIN, sh_max_val = SHRT_MAX;
    IMPL_CONSTRUCT_TEST(sh_val);
    IMPL_CONSTRUCT_TEST(sh_min_val);
    IMPL_CONSTRUCT_TEST(sh_max_val);

    int i_val = 1, i_min_val = INT_MIN, i_max_val = INT_MAX;
    IMPL_CONSTRUCT_TEST(i_val);
    IMPL_CONSTRUCT_TEST(i_min_val);
    IMPL_CONSTRUCT_TEST(i_max_val);

    long long ll_val = 1, ll_min_val = LLONG_MIN, ll_max_val = LLONG_MAX;
    IMPL_CONSTRUCT_TEST(ll_val);
    IMPL_CONSTRUCT_TEST(ll_min_val);
    IMPL_CONSTRUCT_TEST(ll_max_val);

    unsigned char uch_val = 1, uch_min_val = 0, uch_max_val = UCHAR_MAX;
    IMPL_CONSTRUCT_TEST(uch_val);
    IMPL_CONSTRUCT_TEST(uch_min_val);
    IMPL_CONSTRUCT_TEST(uch_max_val);

    unsigned short ush_val = 1, ush_min_val = 0, ush_max_val = USHRT_MAX;
    IMPL_CONSTRUCT_TEST(ush_val);
    IMPL_CONSTRUCT_TEST(ush_min_val);
    IMPL_CONSTRUCT_TEST(ush_max_val);

    unsigned int ui_val = 1, ui_min_val = 0, ui_max_val = UINT_MAX;
    IMPL_CONSTRUCT_TEST(ui_val);
    IMPL_CONSTRUCT_TEST(ui_min_val);
    IMPL_CONSTRUCT_TEST(ui_max_val);

    unsigned long long ull_val = 1, ull_min_val = 0, ull_max_val = ULLONG_MAX;
    IMPL_CONSTRUCT_TEST(ull_val);
    IMPL_CONSTRUCT_TEST(ull_min_val);
    IMPL_CONSTRUCT_TEST(ull_max_val);

    float f_val = 1.0f, f_min_val = -FLT_MAX, f_max_val = FLT_MAX;
    IMPL_CONSTRUCT_TEST(f_val);
    IMPL_CONSTRUCT_TEST(f_min_val);
    IMPL_CONSTRUCT_TEST(f_max_val);

    double d_val = 1.0, d_min_val = -DBL_MAX, d_max_val = DBL_MAX;
    IMPL_CONSTRUCT_TEST(d_val);
    IMPL_CONSTRUCT_TEST(d_min_val);
    IMPL_CONSTRUCT_TEST(d_max_val);

    bool b_true = true, b_false = false;
    IMPL_CONSTRUCT_TEST(b_true);
    IMPL_CONSTRUCT_TEST(b_false);
}

TEST(gt_avariant, construct_complex) {
    std::string str_empty, str_val("test");
    IMPL_CONSTRUCT_TEST(str_empty);
    IMPL_CONSTRUCT_TEST(str_val);

    alx::bytes bytes_empty, bytes_val(1024, 127U);
    IMPL_CONSTRUCT_TEST(bytes_empty);
    IMPL_CONSTRUCT_TEST(bytes_val);
}

TEST(gt_avariant, construct_vector) {
    std::vector<int> vec_int_empty, vec_int_val(1024, 127);
    IMPL_CONSTRUCT_TEST(vec_int_empty);
    IMPL_CONSTRUCT_TEST(vec_int_val);

    std::vector<std::string> vec_str_empty, vec_str_val(1024, "test");
    IMPL_CONSTRUCT_TEST(vec_str_empty);
    IMPL_CONSTRUCT_TEST(vec_str_val);

    std::vector<alx::bytes> vec_bytes_empty, vec_bytes_val(1024, alx::bytes(256, 127U));
    IMPL_CONSTRUCT_TEST(vec_bytes_empty);
    IMPL_CONSTRUCT_TEST(vec_bytes_val);
}

TEST(gt_avariant, construct_list) {
    std::list<int> list_int_empty, list_int_val(1024, 127);
    IMPL_CONSTRUCT_TEST(list_int_empty);
    IMPL_CONSTRUCT_TEST(list_int_val);

    std::list<std::string> list_str_empty, list_str_val(1024, "test");
    IMPL_CONSTRUCT_TEST(list_str_empty);
    IMPL_CONSTRUCT_TEST(list_str_val);

    std::list<alx::bytes> list_bytes_empty, list_bytes_val(1024, alx::bytes(256, 127U));
    IMPL_CONSTRUCT_TEST(list_bytes_empty);
    IMPL_CONSTRUCT_TEST(list_bytes_val);
}

#define IMPL_ASSIGN_TEST_BY_COPY_VALUE(VALUE)                        \
    {                                                                \
        alx::variant var_##VALUE;                                    \
        ASSERT_TRUE(var_##VALUE.null());                             \
        var_##VALUE = VALUE;                                         \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_ASSIGN_TEST_BY_MOVE_VALUE(VALUE)                        \
    {                                                                \
        decltype(VALUE) VALUE##_copy(VALUE);                         \
        alx::variant var_##VALUE;                                    \
        ASSERT_TRUE(var_##VALUE.null());                             \
        var_##VALUE = std::move(VALUE##_copy);                       \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_ASSIGN_TEST_BY_COPY_OBJECT(VALUE)                       \
    {                                                                \
        alx::variant var_##VALUE##_obj(VALUE);                       \
        alx::variant var_##VALUE;                                    \
        ASSERT_TRUE(var_##VALUE.null());                             \
        var_##VALUE = var_##VALUE##_obj;                             \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_ASSIGN_TEST_BY_MOVE_OBJECT(VALUE)                       \
    {                                                                \
        alx::variant var_##VALUE##_obj(VALUE);                       \
        alx::variant var_##VALUE;                                    \
        ASSERT_TRUE(var_##VALUE.null());                             \
        var_##VALUE = std::move(var_##VALUE##_obj);                  \
        ASSERT_STREQ(var_##VALUE.type_name(), typeid(VALUE).name()); \
        ASSERT_TRUE(var_##VALUE.is<decltype(VALUE)>());              \
        ASSERT_EQ(var_##VALUE.to<decltype(VALUE)>(), VALUE);         \
        ASSERT_EQ(var_##VALUE, alx::variant(VALUE));                 \
    }

#define IMPL_ASSIGN_TEST(VALUE)             \
    IMPL_ASSIGN_TEST_BY_COPY_VALUE(VALUE);  \
    IMPL_ASSIGN_TEST_BY_MOVE_VALUE(VALUE);  \
    IMPL_ASSIGN_TEST_BY_COPY_OBJECT(VALUE); \
    IMPL_ASSIGN_TEST_BY_MOVE_OBJECT(VALUE)

TEST(gt_avariant, assign_builtin) {
    char ch_val = 1, ch_min_val = SCHAR_MIN, ch_max_val = SCHAR_MAX;
    IMPL_ASSIGN_TEST(ch_val);
    IMPL_ASSIGN_TEST(ch_min_val);
    IMPL_ASSIGN_TEST(ch_max_val);

    short sh_val = 1, sh_min_val = SHRT_MIN, sh_max_val = SHRT_MAX;
    IMPL_ASSIGN_TEST(sh_val);
    IMPL_ASSIGN_TEST(sh_min_val);
    IMPL_ASSIGN_TEST(sh_max_val);

    int i_val = 1, i_min_val = INT_MIN, i_max_val = INT_MAX;
    IMPL_ASSIGN_TEST(i_val);
    IMPL_ASSIGN_TEST(i_min_val);
    IMPL_ASSIGN_TEST(i_max_val);

    long long ll_val = 1, ll_min_val = LLONG_MIN, ll_max_val = LLONG_MAX;
    IMPL_ASSIGN_TEST(ll_val);
    IMPL_ASSIGN_TEST(ll_min_val);
    IMPL_ASSIGN_TEST(ll_max_val);

    unsigned char uch_val = 1, uch_min_val = 0, uch_max_val = UCHAR_MAX;
    IMPL_ASSIGN_TEST(uch_val);
    IMPL_ASSIGN_TEST(uch_min_val);
    IMPL_ASSIGN_TEST(uch_max_val);

    unsigned short ush_val = 1, ush_min_val = 0, ush_max_val = USHRT_MAX;
    IMPL_ASSIGN_TEST(ush_val);
    IMPL_ASSIGN_TEST(ush_min_val);
    IMPL_ASSIGN_TEST(ush_max_val);

    unsigned int ui_val = 1, ui_min_val = 0, ui_max_val = UINT_MAX;
    IMPL_ASSIGN_TEST(ui_val);
    IMPL_ASSIGN_TEST(ui_min_val);
    IMPL_ASSIGN_TEST(ui_max_val);

    unsigned long long ull_val = 1, ull_min_val = 0, ull_max_val = ULLONG_MAX;
    IMPL_ASSIGN_TEST(ull_val);
    IMPL_ASSIGN_TEST(ull_min_val);
    IMPL_ASSIGN_TEST(ull_max_val);

    float f_val = 1.0f, f_min_val = -FLT_MAX, f_max_val = FLT_MAX;
    IMPL_ASSIGN_TEST(f_val);
    IMPL_ASSIGN_TEST(f_min_val);
    IMPL_ASSIGN_TEST(f_max_val);

    double d_val = 1.0, d_min_val = -DBL_MAX, d_max_val = DBL_MAX;
    IMPL_ASSIGN_TEST(d_val);
    IMPL_ASSIGN_TEST(d_min_val);
    IMPL_ASSIGN_TEST(d_max_val);

    bool b_true = true, b_false = false;
    IMPL_ASSIGN_TEST(b_true);
    IMPL_ASSIGN_TEST(b_false);
}

TEST(gt_avariant, assign_complex) {
    std::string str_empty, str_val("test");
    IMPL_ASSIGN_TEST(str_empty);
    IMPL_ASSIGN_TEST(str_val);

    alx::bytes bytes_empty, bytes_val(1024, 127U);
    IMPL_ASSIGN_TEST(bytes_empty);
    IMPL_ASSIGN_TEST(bytes_val);
}

TEST(gt_avariant, assign_vector) {
    std::vector<int> vec_int_empty, vec_int_val(1024, 127);
    IMPL_ASSIGN_TEST(vec_int_empty);
    IMPL_ASSIGN_TEST(vec_int_val);

    std::vector<std::string> vec_str_empty, vec_str_val(1024, "test");
    IMPL_ASSIGN_TEST(vec_str_empty);
    IMPL_ASSIGN_TEST(vec_str_val);

    std::vector<alx::bytes> vec_bytes_empty, vec_bytes_val(1024, alx::bytes(256, 127U));
    IMPL_ASSIGN_TEST(vec_bytes_empty);
    IMPL_ASSIGN_TEST(vec_bytes_val);
}

TEST(gt_avariant, assign_list) {
    std::list<int> list_int_empty, list_int_val(1024, 127);
    IMPL_ASSIGN_TEST(list_int_empty);
    IMPL_ASSIGN_TEST(list_int_val);

    std::list<std::string> list_str_empty, list_str_val(1024, "test");
    IMPL_ASSIGN_TEST(list_str_empty);
    IMPL_ASSIGN_TEST(list_str_val);

    std::list<alx::bytes> list_bytes_empty, list_bytes_val(1024, alx::bytes(256, 127U));
    IMPL_ASSIGN_TEST(list_bytes_empty);
    IMPL_ASSIGN_TEST(list_bytes_val);
}

TEST(gt_avariant, null) {
    alx::variant var;
    ASSERT_TRUE(var.null());
    ASSERT_EQ(var.type(), -1);
    ASSERT_STREQ(var.type_name(), "");

    ASSERT_EQ(var.to<char>(), 0);
    ASSERT_EQ(var.to<short>(), 0);
    ASSERT_EQ(var.to<int>(), 0);
    ASSERT_EQ(var.to<long long>(), 0LL);

    ASSERT_EQ(var.to<unsigned char>(), 0U);
    ASSERT_EQ(var.to<unsigned short>(), 0U);
    ASSERT_EQ(var.to<unsigned int>(), 0U);
    ASSERT_EQ(var.to<unsigned long long>(), 0LLU);

    ASSERT_EQ(var.to<float>(), float());
    ASSERT_EQ(var.to<double>(), double());
    ASSERT_EQ(var.to<bool>(), bool());

    ASSERT_EQ(&var.to<std::string>(), &alx::variant::def_val<std::string>());
    ASSERT_EQ(&var.to<alx::bytes>(), &alx::variant::def_val<alx::bytes>());

    ASSERT_EQ(&var.to<alx::variant>(), &var);

    ASSERT_EQ(&var.to<std::vector<char>>(), &alx::variant::def_val<std::vector<char>>());
    ASSERT_EQ(&var.to<std::vector<short>>(), &alx::variant::def_val<std::vector<short>>());
    ASSERT_EQ(&var.to<std::vector<int>>(), &alx::variant::def_val<std::vector<int>>());
    ASSERT_EQ(&var.to<std::vector<long long>>(), &alx::variant::def_val<std::vector<long long>>());

    ASSERT_EQ(&var.to<std::vector<unsigned char>>(), &alx::variant::def_val<std::vector<unsigned char>>());
    ASSERT_EQ(&var.to<std::vector<unsigned short>>(), &alx::variant::def_val<std::vector<unsigned short>>());
    ASSERT_EQ(&var.to<std::vector<unsigned int>>(), &alx::variant::def_val<std::vector<unsigned int>>());
    ASSERT_EQ(&var.to<std::vector<unsigned long long>>(), &alx::variant::def_val<std::vector<unsigned long long>>());

    ASSERT_EQ(&var.to<std::vector<float>>(), &alx::variant::def_val<std::vector<float>>());
    ASSERT_EQ(&var.to<std::vector<double>>(), &alx::variant::def_val<std::vector<double>>());
    ASSERT_EQ(&var.to<std::vector<bool>>(), &alx::variant::def_val<std::vector<bool>>());
    ASSERT_EQ(&var.to<std::vector<alx::bytes>>(), &alx::variant::def_val<std::vector<alx::bytes>>());
    ASSERT_EQ(&var.to<std::vector<std::string>>(), &alx::variant::def_val<std::vector<std::string>>());

    ASSERT_EQ(&var.to<std::list<char>>(), &alx::variant::def_val<std::list<char>>());
    ASSERT_EQ(&var.to<std::list<short>>(), &alx::variant::def_val<std::list<short>>());
    ASSERT_EQ(&var.to<std::list<int>>(), &alx::variant::def_val<std::list<int>>());
    ASSERT_EQ(&var.to<std::list<long long>>(), &alx::variant::def_val<std::list<long long>>());

    ASSERT_EQ(&var.to<std::list<unsigned char>>(), &alx::variant::def_val<std::list<unsigned char>>());
    ASSERT_EQ(&var.to<std::list<unsigned short>>(), &alx::variant::def_val<std::list<unsigned short>>());
    ASSERT_EQ(&var.to<std::list<unsigned int>>(), &alx::variant::def_val<std::list<unsigned int>>());
    ASSERT_EQ(&var.to<std::list<unsigned long long>>(), &alx::variant::def_val<std::list<unsigned long long>>());

    ASSERT_EQ(&var.to<std::list<float>>(), &alx::variant::def_val<std::list<float>>());
    ASSERT_EQ(&var.to<std::list<double>>(), &alx::variant::def_val<std::list<double>>());
    ASSERT_EQ(&var.to<std::list<bool>>(), &alx::variant::def_val<std::list<bool>>());
    ASSERT_EQ(&var.to<std::list<alx::bytes>>(), &alx::variant::def_val<std::list<alx::bytes>>());
    ASSERT_EQ(&var.to<std::list<std::string>>(), &alx::variant::def_val<std::list<std::string>>());
}

#define IMPL_LOSSLESS_NUMBER_TEST(T, VALUE)                                                                             \
    {                                                                                                                   \
        alx::variant var((T) VALUE);                                                                                    \
        ASSERT_TRUE(var.is_number());                                                                                   \
        ASSERT_EQ(var.is_integer(), alx::is_integer<T>);                                                                \
        ASSERT_EQ(var.is_uinteger(), alx::is_uinteger<T>);                                                              \
        ASSERT_EQ(var.to_number(), (double) (T) VALUE);                                                                 \
        ASSERT_EQ(var.to<char>(), alx::cond_value<T>(alx::is_lossless<T, char>, VALUE, 0));                             \
        ASSERT_EQ(var.to<short>(), alx::cond_value<T>(alx::is_lossless<T, short>, VALUE, 0));                           \
        ASSERT_EQ(var.to<int>(), alx::cond_value<T>(alx::is_lossless<T, int>, VALUE, 0));                               \
        ASSERT_EQ(var.to<long long>(), alx::cond_value<T>(alx::is_lossless<T, long long>, VALUE, 0));                   \
        ASSERT_EQ(var.to<unsigned char>(), alx::cond_value<T>(alx::is_lossless<T, unsigned char>, VALUE, 0));           \
        ASSERT_EQ(var.to<unsigned short>(), alx::cond_value<T>(alx::is_lossless<T, unsigned short>, VALUE, 0));         \
        ASSERT_EQ(var.to<unsigned int>(), alx::cond_value<T>(alx::is_lossless<T, unsigned int>, VALUE, 0));             \
        ASSERT_EQ(var.to<unsigned long long>(), alx::cond_value<T>(alx::is_lossless<T, unsigned long long>, VALUE, 0)); \
    }

TEST(gt_avariant, lossless_number) {
    IMPL_LOSSLESS_NUMBER_TEST(char, -127);
    IMPL_LOSSLESS_NUMBER_TEST(short, -127);
    IMPL_LOSSLESS_NUMBER_TEST(int, -127);
    IMPL_LOSSLESS_NUMBER_TEST(long long, -127);
    IMPL_LOSSLESS_NUMBER_TEST(unsigned char, 127);
    IMPL_LOSSLESS_NUMBER_TEST(unsigned short, 127);
    IMPL_LOSSLESS_NUMBER_TEST(unsigned int, 127);
    IMPL_LOSSLESS_NUMBER_TEST(unsigned long long, 127);
}

TEST(gt_avariant, to_type_const) {
    alx::variant var("123");
    ASSERT_EQ(var.to<int>(), 0);
    ASSERT_EQ(var.to<std::string>(), "123");
    ASSERT_EQ(&var.to<alx::bytes>(), &alx::variant::def_val<alx::bytes>());
}

TEST(gt_avariant, to_type_reference) {
    alx::variant var("123");
    std::string def;
    std::string& str = var.to<std::string>(def);
    ASSERT_EQ(str, "123");
    ASSERT_NE(&str, &def);
    alx::bytes def2;
    alx::bytes& bytes = var.to<alx::bytes>(def2);
    ASSERT_EQ(&bytes, &def2);
}

TEST(gt_avariant, as_type) {
    alx::variant var;
    var.as<int>() = 123;
    ASSERT_EQ(var.to<int>(), 123);
    var.as<std::string>() = "123";
    ASSERT_EQ(var.to<std::string>(), "123");

    alx::bytes isnot_bytes(10, 0X77U);
    var.as(std::move(isnot_bytes));
    ASSERT_EQ(var.to<alx::bytes>(), alx::bytes(10, 0X77U));
    ASSERT_TRUE(isnot_bytes.empty());

    std::string isnot_string("123");
    var.as(isnot_string);
    ASSERT_EQ(var.to<std::string>(), "123");
    ASSERT_EQ(isnot_string, "123");
}
