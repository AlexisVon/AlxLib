/*****************************************************************/ /**
 * \file   gt_ascript_utils.cpp
 * \brief  Unit tests for ascript_utils — variant conversion, comparison, trace
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_utils.h"
#include <cmath>
#include <limits>
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

TEST(gt_ascript_utils, type_name) {
    EXPECT_STREQ("int", type_name_script(variant(int_64(1))));
    EXPECT_STREQ("float", type_name_script(variant(3.14)));
    EXPECT_STREQ("bool", type_name_script(variant(true)));
    EXPECT_STREQ("string", type_name_script(variant(std::string("hi"))));
    EXPECT_STREQ("bytes", type_name_script(variant(bytes("ab"))));
    EXPECT_STREQ("vec", type_name_script(variant(varvec{})));
    EXPECT_STREQ("map", type_name_script(variant(varmap{})));
    EXPECT_STREQ("lst", type_name_script(variant(varlst{})));
    EXPECT_STREQ("null", type_name_script(variant()));
}

TEST(gt_ascript_utils, type_name_handles) {
    EXPECT_STREQ("func", type_name_script(variant(anyptr_ex<call_able>::make(new call_able()))));
    EXPECT_STREQ("import", type_name_script(variant(anyptr_ex<impl_import>::make(new impl_import()))));
    EXPECT_STREQ("link", type_name_script(variant(anyptr_ex<impl_link>::make(new impl_link()))));
    EXPECT_STREQ("area", type_name_script(variant(anyptr_ex<link_area>::make(new link_area()))));
    EXPECT_STREQ("unknown", type_name_script(variant(anyptr_ex<int_64>::make(new int_64(7)))));

    anyptr held = anyptr_ex<call_able>::make(new call_able());
    anyptr taken = std::move(held);
    variant moved_from(std::move(held));
    EXPECT_STREQ("null", type_name_script(moved_from));
    EXPECT_STREQ("func", type_name_script(variant(std::move(taken))));

    EXPECT_STREQ("null", type_name_script(variant(anyptr())));
    EXPECT_STREQ("unknown", type_name_script(variant(1.5f)));
    EXPECT_STREQ("unknown", type_name_script(variant(uint_64(3))));
}

static const char* utils_type_ex(const variant& _v, void* _ud) {
    if (_v.is<float>()) return "float32";
    return _ud ? static_cast<const char*>(_ud) : nullptr;
}

TEST(gt_ascript_utils, type_name_ex) {
    char named[] = "named";
    EXPECT_STREQ("float32", type_name_script(variant(1.5f), utils_type_ex, nullptr));
    EXPECT_STREQ("unknown", type_name_script(variant(uint_64(3)), utils_type_ex, nullptr));
    EXPECT_STREQ("named", type_name_script(variant(anyptr_ex<int_64>::make(new int_64(7))), utils_type_ex, named));
    EXPECT_STREQ("int", type_name_script(variant(int_64(1)), utils_type_ex, named));
    EXPECT_STREQ("func", type_name_script(variant(anyptr_ex<call_able>::make(new call_able())), utils_type_ex, named));
    EXPECT_STREQ("null", type_name_script(variant(anyptr()), utils_type_ex, named));
}

TEST(gt_ascript_utils, to_int_strict) {
    EXPECT_EQ(42, to_int_strict(variant(int_64(42))));
    EXPECT_THROW(to_int_strict(variant(3.14)), script_exception);
    EXPECT_THROW(to_int_strict(variant(true)), script_exception);
}

TEST(gt_ascript_utils, to_float_strict) {
    EXPECT_DOUBLE_EQ(3.14, to_float_strict(variant(3.14)));
    EXPECT_THROW(to_float_strict(variant(int_64(42))), script_exception);
}

TEST(gt_ascript_utils, to_bool_strict) {
    EXPECT_EQ(true, to_bool_strict(variant(true)));
    EXPECT_EQ(false, to_bool_strict(variant(false)));

    EXPECT_EQ(true, to_bool_strict(variant(int_64(1))));
    EXPECT_EQ(true, to_bool_strict(variant(int_64(-1))));
    EXPECT_EQ(false, to_bool_strict(variant(int_64(0))));

    EXPECT_EQ(true, to_bool_strict(variant(3.14)));
    EXPECT_EQ(false, to_bool_strict(variant(0.0)));

    EXPECT_THROW(to_bool_strict(variant(std::string("x"))), script_exception);
    EXPECT_THROW(to_bool_strict(variant()), script_exception);
}

TEST(gt_ascript_utils, cov_bool_direct) {
    EXPECT_TRUE(cov_bool(variant(true)));
    EXPECT_FALSE(cov_bool(variant(false)));
}

TEST(gt_ascript_utils, cov_bool_from_int) {
    EXPECT_TRUE(cov_bool(variant(int_64(1))));
    EXPECT_TRUE(cov_bool(variant(int_64(-1))));
    EXPECT_FALSE(cov_bool(variant(int_64(0))));
}

TEST(gt_ascript_utils, cov_bool_from_float) {
    EXPECT_TRUE(cov_bool(variant(3.14)));
    EXPECT_FALSE(cov_bool(variant(0.0)));
}

TEST(gt_ascript_utils, cov_bool_from_string) {
    EXPECT_TRUE(cov_bool(variant(std::string("true"))));
    EXPECT_TRUE(cov_bool(variant(std::string("TRUE"))));
    EXPECT_TRUE(cov_bool(variant(std::string("false"))));
    EXPECT_TRUE(cov_bool(variant(std::string("hello"))));
    EXPECT_FALSE(cov_bool(variant(std::string())));
}

TEST(gt_ascript_utils, cov_bool_from_bytes) {
    EXPECT_TRUE(cov_bool(variant(bytes(1, 0x00))));
    EXPECT_FALSE(cov_bool(variant(bytes())));
}

TEST(gt_ascript_utils, cov_bool_from_containers) {
    EXPECT_TRUE(cov_bool(variant(varvec{variant(int_64(1))})));
    EXPECT_FALSE(cov_bool(variant(varvec{})));
    EXPECT_TRUE(cov_bool(variant(varlst{variant(int_64(1))})));
    EXPECT_FALSE(cov_bool(variant(varlst{})));
}

TEST(gt_ascript_utils, cov_bool_from_null) {
    EXPECT_FALSE(cov_bool(variant()));
}

TEST(gt_ascript_utils, cov_string_direct) {
    EXPECT_EQ("hello", cov_string(variant(std::string("hello"))));
}

TEST(gt_ascript_utils, cov_string_from_int) {
    EXPECT_EQ("42", cov_string(variant(int_64(42))));
    EXPECT_EQ("-7", cov_string(variant(int_64(-7))));
}

TEST(gt_ascript_utils, cov_string_from_float) {
    auto s = cov_string(variant(3.140000));
    EXPECT_TRUE(s.find("3.14") == 0);
}

TEST(gt_ascript_utils, cov_string_from_bool) {
    EXPECT_EQ("true", cov_string(variant(true)));
    EXPECT_EQ("false", cov_string(variant(false)));
}

TEST(gt_ascript_utils, cov_string_from_vec_all_string) {
    varvec v;
    v.push_back(variant(std::string("a")));
    v.push_back(variant(std::string("b")));
    EXPECT_EQ("ab", cov_string(variant(v)));
}

TEST(gt_ascript_utils, cov_string_from_vec_all_int_utf8) {
    varvec v;
    v.push_back(variant(int_64(65)));
    v.push_back(variant(int_64(66)));
    EXPECT_EQ("AB", cov_string(variant(v)));
}

TEST(gt_ascript_utils, cov_string_from_null_default) {
    EXPECT_EQ("", cov_string(variant()));
}

TEST(gt_ascript_utils, cov_map_direct) {
    varmap m;
    EXPECT_EQ(0u, cov_map(variant(m)).size());
}

TEST(gt_ascript_utils, cov_map_from_other_throws) {
    EXPECT_THROW(cov_map(variant(int_64(1))), script_exception);
}

TEST(gt_ascript_utils, variant_to_display_scalars) {
    EXPECT_EQ("hello", variant_to_display(variant(std::string("hello"))));
    EXPECT_EQ("42", variant_to_display(variant(int_64(42))));
    EXPECT_EQ("true", variant_to_display(variant(true)));
    EXPECT_EQ("null", variant_to_display(variant()));
}

TEST(gt_ascript_utils, variant_to_display_containers) {
    EXPECT_EQ("[]:3", variant_to_display(variant(varvec{variant(int_64(1)), variant(int_64(2)), variant(int_64(3))})));
    EXPECT_EQ("():" + std::to_string(varlst{}.size()), variant_to_display(variant(varlst{})));
}

TEST(gt_ascript_utils, cov_int_direct) {
    EXPECT_EQ(42, cov_int(variant(int_64(42))));
}

TEST(gt_ascript_utils, cov_int_from_float) {
    EXPECT_EQ(3, cov_int(variant(3.14)));
    EXPECT_EQ(-1, cov_int(variant(-1.9)));

    EXPECT_EQ(std::numeric_limits<int_64>::min(), cov_int(variant(-9223372036854775808.0)));

    EXPECT_EQ(int_64(9223372036854774784), cov_int(variant(9223372036854774784.0)));
}

TEST(gt_ascript_utils, cov_int_from_float_overflow) {

    EXPECT_THROW(cov_int(variant(1e300)), script_exception);
    EXPECT_THROW(cov_int(variant(-1e300)), script_exception);
    EXPECT_THROW(cov_int(variant(9223372036854775808.0)), script_exception);
    EXPECT_THROW(cov_int(variant(std::numeric_limits<double>::infinity())), script_exception);
    EXPECT_THROW(cov_int(variant(std::numeric_limits<double>::quiet_NaN())), script_exception);
}

TEST(gt_ascript_utils, cov_int_from_bool) {
    EXPECT_EQ(1, cov_int(variant(true)));
    EXPECT_EQ(0, cov_int(variant(false)));
}

TEST(gt_ascript_utils, cov_int_from_string) {
    EXPECT_EQ(42, cov_int(variant(std::string("42"))));
    EXPECT_EQ(-7, cov_int(variant(std::string("-7"))));
    EXPECT_EQ(255, cov_int(variant(std::string("0xff"))));
    EXPECT_EQ(8, cov_int(variant(std::string("0o10"))));
    EXPECT_EQ(3, cov_int(variant(std::string("0b11"))));
    EXPECT_THROW(cov_int(variant(std::string(""))), script_exception);
    EXPECT_THROW(cov_int(variant(std::string("abc"))), script_exception);
}

TEST(gt_ascript_utils, cov_int_from_single_element_containers) {
    EXPECT_EQ(42, cov_int(variant(varvec{variant(int_64(42))})));
    EXPECT_EQ(42, cov_int(variant(varlst{variant(int_64(42))})));
    EXPECT_THROW(cov_int(variant(varvec{variant(int_64(1)), variant(int_64(2))})), script_exception);
}

TEST(gt_ascript_utils, cov_int_from_null_default) {
    EXPECT_EQ(0, cov_int(variant()));
}

TEST(gt_ascript_utils, cov_float_direct) {
    EXPECT_DOUBLE_EQ(3.14, cov_float(variant(3.14)));
}

TEST(gt_ascript_utils, cov_float_from_int) {
    EXPECT_DOUBLE_EQ(42.0, cov_float(variant(int_64(42))));
}

TEST(gt_ascript_utils, cov_float_from_bool) {
    EXPECT_DOUBLE_EQ(1.0, cov_float(variant(true)));
    EXPECT_DOUBLE_EQ(0.0, cov_float(variant(false)));
}

TEST(gt_ascript_utils, cov_float_from_string) {
    EXPECT_DOUBLE_EQ(3.14, cov_float(variant(std::string("3.14"))));
    EXPECT_DOUBLE_EQ(-1.5, cov_float(variant(std::string("-1.5"))));
    EXPECT_TRUE(std::isinf(cov_float(variant(std::string("inf")))));
    EXPECT_TRUE(std::isinf(cov_float(variant(std::string("+inf")))));
    EXPECT_TRUE(std::isinf(cov_float(variant(std::string("-inf")))));
    EXPECT_TRUE(std::isnan(cov_float(variant(std::string("nan")))));
    EXPECT_THROW(cov_float(variant(std::string("1e999"))), script_exception);
    EXPECT_THROW(cov_float(variant(std::string("abc"))), script_exception);
}

TEST(gt_ascript_utils, cov_float_from_single_element_containers) {
    EXPECT_DOUBLE_EQ(3.14, cov_float(variant(varvec{variant(3.14)})));
    EXPECT_DOUBLE_EQ(3.14, cov_float(variant(varlst{variant(3.14)})));
}

TEST(gt_ascript_utils, cov_vec_direct) {
    varvec v;
    v.push_back(variant(int_64(1)));
    auto result = cov_vec(variant(v));
    EXPECT_EQ(1u, result.size());
}

TEST(gt_ascript_utils, cov_vec_from_lst) {
    varlst l;
    l.push_back(variant(int_64(1)));
    l.push_back(variant(int_64(2)));
    auto result = cov_vec(variant(l));
    EXPECT_EQ(2u, result.size());
}

TEST(gt_ascript_utils, cov_vec_from_string_utf8) {
    auto result = cov_vec(variant(std::string("AB")));
    EXPECT_EQ(2u, result.size());
    EXPECT_EQ(65, result[0].to<int_64>());
    EXPECT_EQ(66, result[1].to<int_64>());
}

TEST(gt_ascript_utils, cov_vec_from_invalid_utf8_throws) {
    EXPECT_THROW(cov_vec(variant(std::string("\xff\xfe"))), script_exception);
}

TEST(gt_ascript_utils, cov_vec_from_scalar) {
    auto result = cov_vec(variant(int_64(42)));
    EXPECT_EQ(1u, result.size());
    EXPECT_EQ(42, result[0].to<int_64>());
}

TEST(gt_ascript_utils, cov_lst_direct) {
    varlst l;
    l.push_back(variant(int_64(1)));
    auto result = cov_lst(variant(l));
    EXPECT_EQ(1u, result.size());
}

TEST(gt_ascript_utils, cov_lst_from_vec) {
    varvec v;
    v.push_back(variant(int_64(1)));
    v.push_back(variant(int_64(2)));
    auto result = cov_lst(variant(v));
    EXPECT_EQ(2u, result.size());
}

TEST(gt_ascript_utils, cov_lst_from_string_utf8) {
    auto result = cov_lst(variant(std::string("AB")));
    EXPECT_EQ(2u, result.size());
}

TEST(gt_ascript_utils, cov_lst_from_scalar) {
    auto result = cov_lst(variant(int_64(42)));
    EXPECT_EQ(1u, result.size());
}

TEST(gt_ascript_utils, eq_cmp_null) {
    EXPECT_TRUE(eq_cmp(variant(), variant()));
    EXPECT_FALSE(eq_cmp(variant(), variant(int_64(0))));
}

TEST(gt_ascript_utils, eq_cmp_int_float_cross) {
    EXPECT_TRUE(eq_cmp(variant(int_64(1)), variant(1.0)));
}

TEST(gt_ascript_utils, eq_cmp_string_number_cross) {

    EXPECT_THROW(eq_cmp(variant(int_64(42)), variant(std::string("42"))), script_exception);
    EXPECT_THROW(eq_cmp(variant(std::string("3.14")), variant(3.14)), script_exception);
}

TEST(gt_ascript_utils, eq_cmp_vec) {
    varvec va, vb;
    va.push_back(variant(int_64(1)));
    vb.push_back(variant(int_64(1)));
    EXPECT_TRUE(eq_cmp(variant(va), variant(vb)));
    vb[0] = variant(int_64(2));
    EXPECT_FALSE(eq_cmp(variant(va), variant(vb)));
}

TEST(gt_ascript_utils, eq_cmp_lst) {
    varlst la, lb;
    la.push_back(variant(int_64(1)));
    lb.push_back(variant(int_64(1)));
    EXPECT_TRUE(eq_cmp(variant(la), variant(lb)));
    lb.push_back(variant(int_64(2)));
    EXPECT_FALSE(eq_cmp(variant(la), variant(lb)));
}

TEST(gt_ascript_utils, eq_cmp_different_types_throws) {
    EXPECT_THROW(eq_cmp(variant(int_64(1)), variant(true)), script_exception);
}
