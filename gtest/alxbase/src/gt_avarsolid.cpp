/*****************************************************************/ /**
 * \file   gt_avarsolid.cpp
 * \brief  Unit tests for binary serialization
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aserial.h"
#include "astream.h"
#include "avariant.h"
#include "avarsolid.h"
#include <climits>
#include <gtest/gtest.h>

const alx::varmap& get_def_vmap() {
    static alx::varmap result = []() -> alx::varmap {
        return alx::varmap{
            {"c_val", (char) (1)}, {"c_min", (char) (SCHAR_MIN)}, {"c_max", (char) (SCHAR_MAX)}, {"s_val", (short) (1)}, {"s_min", (short) (SHRT_MIN)}, {"s_max", (short) (SHRT_MAX)}, {"i_val", (int) (1)}, {"i_min", (int) (INT_MIN)}, {"i_max", (int) (INT_MAX)}, {"ll_val", (long long) (1)}, {"ll_min", (long long) (LLONG_MIN)}, {"ll_max", (long long) (LLONG_MAX)}, {"uc_val", (unsigned char) (1)}, {"uc_min", (unsigned char) (0)}, {"uc_max", (unsigned char) (UCHAR_MAX)}, {"us_val", (unsigned short) (1)}, {"us_min", (unsigned short) (0)}, {"us_max", (unsigned short) (USHRT_MAX)}, {"ui_val", (unsigned int) (1)}, {"ui_min", (unsigned int) (0)}, {"ui_max", (unsigned int) (UINT_MAX)}, {"ull_val", (unsigned long long) (1)}, {"ull_min", (unsigned long long) (0)}, {"ull_max", (unsigned long long) (ULLONG_MAX)}, {"f_val", (float) (1)}, {"f_min", (float) (FLT_MIN)}, {"f_max", (float) (FLT_MAX)}, {"d_val", (double) (1)}, {"d_min", (double) (DBL_MIN)}, {"d_max", (double) (DBL_MAX)}, {"b_val", (bool) (1)}, {"b_true", (bool) (true)}, {"b_false", (bool) (false)}, {"str_empty", std::string()}, {"str_test", std::string("test")}, {"bytes_empty", alx::bytes()}, {"bytes_filled", alx::bytes(1024, 127U)}, {"vec_int_empty", std::vector<int>()}, {"vec_int_filled", std::vector<int>(1024, 127)}, {"vec_str_empty", std::vector<std::string>()}, {"vec_str_filled", std::vector<std::string>(1024, "test")}, {"vec_bytes_empty", std::vector<alx::bytes>()}, {"vec_bytes_filled", std::vector<alx::bytes>(1024, alx::bytes(256, 127U))}, {"list_int_empty", std::list<int>()}, {"list_int_filled", std::list<int>(1024, 127)}, {"list_str_empty", std::list<std::string>()}, {"list_str_filled", std::list<std::string>(1024, "test")}, {"null_val", alx::variant()}};
    }();
    return result;
}

const alx::varvec& get_def_vvec() {
    static alx::varvec result = []() -> alx::varvec {
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
            std::list<std::string>(), std::list<std::string>(1024, "test"),
            alx::variant()};
    }();
    return result;
}

const alx::varlst& get_def_vlst() {
    static alx::varlst result = []() -> alx::varlst {
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
            std::list<std::string>(), std::list<std::string>(1024, "test"),
            alx::variant()};
    }();
    return result;
}

const alx::varmap& get_vmap() {
    static alx::varmap result = []() -> alx::varmap {
        alx::varmap result(get_def_vmap());
        result["vvec"] = get_def_vvec();
        result["vlst"] = get_def_vlst();
        alx::varvec def_vec;
        alx::varlst def_list;
        result["vvec"].to<alx::varvec>(def_vec).push_back(get_def_vmap());
        result["vlst"].to<alx::varlst>(def_list).push_back(get_def_vmap());
        return result;
    }();
    return result;
}

TEST(gt_avarsolid, memory) {
    const alx::varmap& vmap = get_vmap();
    ASSERT_EQ(vmap.size(), get_def_vmap().size() + 2);
    ASSERT_EQ(vmap.value("vvec").to<alx::varvec>().size(), get_def_vvec().size() + 1);
    ASSERT_EQ(vmap.value("vlst").to<alx::varlst>().size(), get_def_vlst().size() + 1);

    alx::bytes buffer;
    alx::variant* err{nullptr};
    bool ret = alx::varsolid::to_bytes(vmap, buffer, &err);
    ASSERT_TRUE(ret);
    ASSERT_EQ(err, nullptr);
    ASSERT_FALSE(buffer.empty());

    alx::varmap rmap;
    ret = alx::varsolid::to_varmap(buffer, rmap);
    ASSERT_TRUE(ret);
    ASSERT_EQ(rmap, vmap);
}

TEST(gt_avarsolid, stream) {
    const alx::varmap& vmap = get_vmap();
    ASSERT_EQ(vmap.size(), get_def_vmap().size() + 2);
    ASSERT_EQ(vmap.value("vvec").to<alx::varvec>().size(), get_def_vvec().size() + 1);
    ASSERT_EQ(vmap.value("vlst").to<alx::varlst>().size(), get_def_vlst().size() + 1);

    alx::bytes buffer;
    alx::variant* err{nullptr};
    alx::ostream_buff ostm(buffer);
    bool ret = alx::varsolid::to_bytes(vmap, ostm, &err);
    ASSERT_TRUE(ret);
    ASSERT_EQ(err, nullptr);
    ostm.flush();
    ASSERT_FALSE(buffer.empty());

    alx::varmap rmap;
    ret = alx::varsolid::to_varmap(buffer, rmap);
    ASSERT_TRUE(ret);
    ASSERT_EQ(rmap, vmap);
}

TEST(gt_avarsolid, get_value) {
    const alx::varmap& vmap = get_vmap();
    ASSERT_EQ(vmap.size(), get_def_vmap().size() + 2);
    ASSERT_EQ(vmap.value("vvec").to<alx::varvec>().size(), get_def_vvec().size() + 1);
    ASSERT_EQ(vmap.value("vlst").to<alx::varlst>().size(), get_def_vlst().size() + 1);

    alx::bytes buffer;
    alx::variant* err{nullptr};
    bool ret = alx::varsolid::to_bytes(vmap, buffer, &err);
    ASSERT_TRUE(ret);
    ASSERT_EQ(err, nullptr);
    ASSERT_FALSE(buffer.empty());

    alx::variant var;
    var = alx::varsolid::get_value(buffer, {"c_val"});
    ASSERT_EQ(var, vmap.value("c_val"));

    var = alx::varsolid::get_value(buffer, {"bytes_empty"});
    ASSERT_EQ(var, vmap.value("bytes_empty"));

    var = alx::varsolid::get_value(buffer, {"null_val"});
    ASSERT_EQ(var, vmap.value("null_val"));

    var = alx::varsolid::get_value(buffer, {"vvec"});
    ASSERT_EQ(var, vmap.value("vvec"));

    var = alx::varsolid::get_value(buffer, {"vvec", "0"});
    ASSERT_EQ(var, vmap.value("vvec").to<alx::varvec>()[0]);

    var = alx::varsolid::get_value(buffer, {"vvec", "48", "c_val"});
    ASSERT_EQ(var, vmap.value("vvec").to<alx::varvec>()[48].to_map().value("c_val"));

    var = alx::varsolid::get_value(buffer, {"vvec", "49"});
    ASSERT_EQ(var, alx::variant());

    var = alx::varsolid::get_value(buffer, {"vlst"});
    ASSERT_EQ(var, vmap.value("vlst"));

    var = alx::varsolid::get_value(buffer, {"vlst", "0"});
    ASSERT_EQ(var, vmap.value("vlst").to<alx::varlst>().front());

    var = alx::varsolid::get_value(buffer, {"vlst", "48", "c_val"});
    ASSERT_EQ(var, vmap.value("vlst").to<alx::varlst>().back().to_map().value("c_val"));

    var = alx::varsolid::get_value(buffer, {"vlst", "49"});
    ASSERT_EQ(var, alx::variant());
}

TEST(gt_avarsolid, long_keyname) {
    alx::varmap vmap = get_vmap();
    ASSERT_EQ(vmap.size(), get_def_vmap().size() + 2);
    ASSERT_EQ(vmap.value("vvec").to<alx::varvec>().size(), get_def_vvec().size() + 1);
    ASSERT_EQ(vmap.value("vlst").to<alx::varlst>().size(), get_def_vlst().size() + 1);

    alx::varvec def_vec;
    alx::varmap def_map;
    alx::bytes buffer;
    alx::variant* err{nullptr};

    vmap["vvec"].to_vec(def_vec).back().to_map(def_map).insert(std::string(alx::ser::_MAX_NAME__, 'a'), 1);
    bool ret = alx::varsolid::to_bytes(vmap, buffer, &err);
    ASSERT_TRUE(ret);
    ASSERT_EQ(err, nullptr);
    ASSERT_FALSE(buffer.empty());
    alx::varmap rmap;
    ret = alx::varsolid::to_varmap(buffer, rmap);
    ASSERT_TRUE(ret);
    ASSERT_EQ(rmap, vmap);

    vmap["vvec"].to_vec(def_vec).back().to_map(def_map).insert(std::string(alx::ser::_MAX_NAME__ + 1, 'a'), 1);
    alx::variant* dst = &vmap["vvec"];
    ret = alx::varsolid::to_bytes(vmap, buffer, &err);
    ASSERT_FALSE(ret);
    ASSERT_EQ(err, dst);
}

TEST(gt_avarsolid, from_empty_bytes) {
    alx::bytes empty;
    alx::varmap rmap;
    EXPECT_FALSE(alx::varsolid::to_varmap(empty, rmap));
    EXPECT_TRUE(rmap.empty());
}

TEST(gt_avarsolid, from_garbage) {

    alx::bytes garbage(256, 0xFF);
    alx::varmap rmap;
    EXPECT_FALSE(alx::varsolid::to_varmap(garbage, rmap));
}

TEST(gt_avarsolid, from_truncated_header) {
    alx::bytes truncated(2, 0);
    alx::varmap rmap;
    EXPECT_FALSE(alx::varsolid::to_varmap(truncated, rmap));
}

TEST(gt_avarsolid, from_wrong_magic) {

    alx::bytes buf(20, 0);
    alx::ser::serer::HEAD head;
    head.SIZE = alx::ser::deserer<alx::varmap, alx::variant>::EMPTY_DATA_SIZE;
    memcpy(buf.data(), &head, sizeof(head));
    alx::ser::serer::TAIL tail;
    tail.END = alx::ser::_BLOCK_END___;
    memcpy(buf.data() + sizeof(head) + head.SIZE, &tail, sizeof(tail));

    alx::varmap rmap;
    EXPECT_FALSE(alx::varsolid::to_varmap(alx::bytes_view(buf), rmap));
}

TEST(gt_avarsolid, get_value_missing_key) {
    alx::varmap vmap;
    vmap.insert("a", 1);
    alx::bytes buffer;
    ASSERT_TRUE(alx::varsolid::to_bytes(vmap, buffer));

    alx::variant def = alx::variant(999);
    alx::variant val = alx::varsolid::get_value(buffer, {"nonexistent"}, def);
    EXPECT_EQ(val, def);
}

TEST(gt_avarsolid, get_value_deeply_nested_path) {
    alx::varmap inner;
    inner.insert("x", 42);
    alx::varvec vvec;
    vvec.push_back(inner);
    alx::varmap vmap;
    vmap["v"] = vvec;
    alx::bytes buffer;
    ASSERT_TRUE(alx::varsolid::to_bytes(vmap, buffer));

    alx::variant val = alx::varsolid::get_value(buffer, {"v", "0", "x"});
    EXPECT_EQ(val.to<int>(), 42);
}

TEST(gt_avarsolid, to_bytes_return_value) {

    alx::varmap vmap;
    vmap.insert("a", 1);
    alx::bytes result = alx::varsolid::to_bytes(vmap);
    EXPECT_FALSE(result.empty());

    alx::varmap rmap;
    EXPECT_TRUE(alx::varsolid::to_varmap(result, rmap));
    EXPECT_EQ(rmap["a"].to<int>(), 1);
}

TEST(gt_avarsolid, truncated_data_size) {

    std::vector<long long> vec(8, 0);
    ((unsigned char*) vec.data())[1] = 0X3CU;

    alx::varmap vmap;
    vmap.insert("v", vec);
    alx::bytes blob = alx::varsolid::to_bytes(vmap);
    ASSERT_FALSE(blob.empty());
    ASSERT_TRUE(alx::varsolid::is_valid(blob));

    const uint64_t head_size = 8, datap_ofst = 12;
    long found = -1;
    for (uint64_t i = 0; i + head_size + 4 <= blob.size(); ++i) {
        uint64_t head = 0;
        memcpy(&head, blob.data() + i, head_size);
        if (((head >> 8) & 0XFFFF) == 1 && ((head >> 24) & 0XFFFFFFFFFFLL) == 64 &&
            blob.data()[i + datap_ofst] == 'v') {
            found = (long) i;
            break;
        }
    }
    ASSERT_GE(found, 0);

    for (uint64_t bad : {1ULL, 9ULL}) {
        uint64_t head = 0;
        memcpy(&head, blob.data() + found, head_size);
        head &= ~(0XFFFFFFFFFFLL << 24);
        head |= (bad & 0XFFFFFFFFFFLL) << 24;
        memcpy(blob.data() + found, &head, head_size);

        alx::varmap out;
        alx::varsolid::to_varmap(blob, out);
    }
}

TEST(gt_avarsolid, get_value_numeric_key_bounded) {
    alx::varmap vmap;
    vmap.insert("v", alx::varvec{(int) (1), (int) (2)});
    alx::bytes blob = alx::varsolid::to_bytes(vmap);
    ASSERT_FALSE(blob.empty());

    const alx::variant def((int) (-1));

    EXPECT_EQ(alx::varsolid::get_value(blob, {"v", "99999999999999999999"}, def).to<int>(), -1);
    EXPECT_EQ(alx::varsolid::get_value(blob, {"v", ""}, def).to<int>(), -1);
    EXPECT_EQ(alx::varsolid::get_value(blob, {"v", "1x"}, def).to<int>(), -1);

    EXPECT_EQ(alx::varsolid::get_value(blob, {"v", "0"}, def).to<int>(), 1);
    EXPECT_EQ(alx::varsolid::get_value(blob, {"v", "1"}, def).to<int>(), 2);
}
