/*****************************************************************/ /**
 * \file   gt_avarjson.cpp
 * \brief  Unit tests for JSON ↔ variant cross conversion
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ajson.h"
#include <climits>
#include <gtest/gtest.h>
#include <utility>

alx::json_object& get_def_jobj() {
    static alx::json_object result = []() -> alx::json_object {
        alx::json_object obj;
        obj.insert("int_key", alx::json_value(8LL));
        obj.insert("bool_key", alx::json_value(false));
        obj.insert("double_key", alx::json_value(3.141592653589793));
        obj.insert("string_key", alx::json_value(std::string("hello alxvar")));
        return obj;
    }();
    return result;
}

const alx::json_array& get_def_jarr() {
    static alx::json_array result = []() -> alx::json_array {
        alx::json_array arr;
        arr.append(alx::json_value(1LL));
        arr.append(alx::json_value(std::string("two")));
        arr.append(alx::json_value(true));
        arr.append(alx::json_value(3.14));
        return arr;
    }();
    return result;
}

alx::json_object& get_complex_jobj() {
    static alx::json_object result = []() -> alx::json_object {
        alx::json_object obj(get_def_jobj());
        obj.insert("array", get_def_jarr());
        alx::json_object nested(get_def_jobj());
        obj.insert("nested", nested);
        return obj;
    }();
    return result;
}

TEST(gt_avarjson, object_varmap_roundtrip) {
    alx::json_object& jobj = get_complex_jobj();
    ASSERT_EQ(jobj.size(), 6U);

    alx::varmap vm = jobj.to_varmap();
    ASSERT_EQ(vm.size(), 6U);
    ASSERT_EQ(vm["int_key"].to<long long>(), 8LL);
    ASSERT_EQ(vm["bool_key"].to<bool>(), false);
    ASSERT_EQ(vm["string_key"].to<std::string>(), "hello alxvar");

    alx::json_object obj2 = alx::json_object::from_varmap(vm);
    ASSERT_EQ(obj2.size(), 6U);
    ASSERT_EQ(obj2["int_key"].to_intg(), 8LL);
    ASSERT_EQ(obj2["bool_key"].to_bool(), false);
    ASSERT_EQ(obj2["string_key"].to_string(), "hello alxvar");

    std::string json1 = alx::json_doc::to_json(jobj, true);
    std::string json2 = alx::json_doc::to_json(obj2, true);
    ASSERT_EQ(json1, json2);
}

TEST(gt_avarjson, array_varvec_roundtrip) {
    const alx::json_array& jarr = get_def_jarr();
    ASSERT_EQ(jarr.size(), 4U);

    alx::varvec vv = jarr.to_varvec();
    ASSERT_EQ(vv.size(), 4U);
    ASSERT_EQ(vv[0].to<long long>(), 1LL);
    ASSERT_EQ(vv[1].to<std::string>(), "two");
    ASSERT_EQ(vv[2].to<bool>(), true);

    alx::json_array arr2 = alx::json_array::from_varvec(vv);
    ASSERT_EQ(arr2.size(), 4U);
    ASSERT_EQ(arr2[0].to_intg(), 1LL);
    ASSERT_EQ(arr2[1].to_string(), "two");
    ASSERT_EQ(arr2[2].to_bool(), true);
}

TEST(gt_avarjson, array_varlst_roundtrip) {
    alx::varlst vl{1LL, std::string("hello"), true, 3.14};
    alx::json_array arr = alx::json_array::from_varlst(vl);
    ASSERT_EQ(arr.size(), 4U);
    ASSERT_EQ(arr[0].to_intg(), 1LL);
    ASSERT_EQ(arr[2].to_bool(), true);

    alx::varvec vv = arr.to_varvec();
    ASSERT_EQ(vv.size(), vl.size());
    auto vit = vv.begin();
    auto lit = vl.begin();
    for (; vit != vv.end() && lit != vl.end(); ++vit, ++lit)
        ASSERT_EQ(*vit, *lit);
}

TEST(gt_avarjson, object_varmap_nested) {
    alx::json_object inner;
    inner.insert("x", alx::json_value(1LL));
    inner.insert("y", alx::json_value(std::string("nested")));

    alx::json_object outer;
    outer.insert("inner", inner);
    outer.insert("top", alx::json_value(99LL));

    alx::varmap vm = outer.to_varmap();
    ASSERT_EQ(vm.size(), 2U);

    alx::varmap vmap_def;
    alx::varmap& inner_vm = vm["inner"].to_map(vmap_def);
    ASSERT_EQ(inner_vm["x"].to<long long>(), 1LL);
    ASSERT_EQ(inner_vm["y"].to<std::string>(), "nested");

    alx::json_object obj2 = alx::json_object::from_varmap(vm);
    ASSERT_EQ(obj2.size(), 2U);
    alx::json_object inner_def;
    alx::json_object& inner2 = obj2["inner"].to_object(inner_def);
    ASSERT_EQ(inner2["x"].to_intg(), 1LL);
    ASSERT_EQ(inner2["y"].to_string(), "nested");
}

TEST(gt_avarjson, value_from_variant_scalars) {
    EXPECT_EQ(alx::json_value::from_variant(alx::variant(42)).to_intg(), 42LL);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant(true)).to_bool(), true);
    EXPECT_DOUBLE_EQ(alx::json_value::from_variant(alx::variant(3.14)).to_double(), 3.14);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant(std::string("x"))).to_string(), "x");
    EXPECT_TRUE(alx::json_value::from_variant(alx::variant()).is_null());
}

TEST(gt_avarjson, value_from_variant_narrowing) {
    EXPECT_EQ(alx::json_value::from_variant(alx::variant((char) 65)).to_intg(), 65LL);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant((short) -1)).to_intg(), -1LL);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant(100U)).to_intg(), 100LL);
    EXPECT_DOUBLE_EQ(alx::json_value::from_variant(alx::variant(1.5f)).to_double(), 1.5);
}

TEST(gt_avarjson, value_from_variant_bytes) {
    alx::bytes b(5, 'a');
    alx::json_value jv = alx::json_value::from_variant(alx::variant(b));
    EXPECT_TRUE(jv.is_string());
    EXPECT_EQ(jv.to_string(), b.to_base64());
}

TEST(gt_avarjson, value_from_variant_containers) {

    alx::varvec vv{1LL, std::string("hello"), true};
    alx::json_value jv = alx::json_value::from_variant(alx::variant(vv));
    EXPECT_TRUE(jv.is_array());
    alx::json_array arr_def;
    EXPECT_EQ(jv.to_array(arr_def).size(), 3U);

    alx::varmap vm{{"a", 1}, {"b", std::string("x")}};
    jv = alx::json_value::from_variant(alx::variant(vm));
    EXPECT_TRUE(jv.is_object());

    alx::varlst vl{1LL, std::string("hello")};
    jv = alx::json_value::from_variant(alx::variant(vl));
    EXPECT_TRUE(jv.is_array());

    std::vector<int> vec{1, 2, 3};
    jv = alx::json_value::from_variant(alx::variant(vec));
    EXPECT_TRUE(jv.is_array());

    std::list<std::string> lst{"a", "b"};
    jv = alx::json_value::from_variant(alx::variant(lst));
    EXPECT_TRUE(jv.is_array());
}

TEST(gt_avarjson, value_to_variant) {
    EXPECT_EQ(alx::json_value(42LL).to_variant().to<long long>(), 42LL);
    EXPECT_EQ(alx::json_value(true).to_variant().to<bool>(), true);
    EXPECT_DOUBLE_EQ(alx::json_value(3.14).to_variant().to<double>(), 3.14);
    EXPECT_EQ(alx::json_value(std::string("x")).to_variant().to<std::string>(), "x");
    EXPECT_TRUE(alx::json_value().to_variant().null());
}

TEST(gt_avarjson, value_from_variant_move_string) {
    std::string big(1000, 'x');
    alx::variant v(big);
    const char* payload = v.to<std::string>().data();

    alx::json_value jv = alx::json_value::from_variant(std::move(v));
    EXPECT_EQ(jv.to_string().data(), payload);
    EXPECT_EQ(jv.to_string(), big);
    EXPECT_TRUE(v.to<std::string>().empty());
}

TEST(gt_avarjson, value_from_variant_move_containers) {
    alx::varvec vv{std::string(1000, 'a'), 42LL};
    alx::variant v(std::move(vv));
    const char* payload = v.to<alx::varvec>()[0].to<std::string>().data();

    alx::json_value jv = alx::json_value::from_variant(std::move(v));
    alx::json_array arr_def;
    const alx::json_array& arr = jv.to_array(arr_def);
    ASSERT_EQ(arr.size(), 2U);
    EXPECT_EQ(arr[0].to_string().data(), payload);
    EXPECT_EQ(arr[0].to_string(), std::string(1000, 'a'));
    EXPECT_EQ(arr[1].to_intg(), 42LL);
    EXPECT_TRUE(v.to<alx::varvec>()[0].to<std::string>().empty());

    alx::varmap vm;
    vm.insert("k", std::string(1000, 'b'));
    alx::variant v2(std::move(vm));
    const char* payload2 = v2.to<alx::varmap>().value("k").to<std::string>().data();

    alx::json_value jv2 = alx::json_value::from_variant(std::move(v2));
    alx::json_object obj_def;
    const alx::json_object& obj = jv2.to_object(obj_def);
    EXPECT_EQ(obj.value("k").to_string().data(), payload2);

    alx::varlst vl{std::string(1000, 'c')};
    alx::variant v3(std::move(vl));
    const char* payload3 = v3.to<alx::varlst>().begin()->to<std::string>().data();

    alx::json_value jv3 = alx::json_value::from_variant(std::move(v3));
    alx::json_array arr3_def;
    EXPECT_EQ(jv3.to_array(arr3_def)[0].to_string().data(), payload3);
}

TEST(gt_avarjson, value_from_variant_move_vector_string) {
    alx::variant v(std::vector<std::string>{std::string(1000, 'd'), std::string(1000, 'e')});
    const char* payload = v.to<std::vector<std::string>>()[0].data();

    alx::json_value jv = alx::json_value::from_variant(std::move(v));
    alx::json_array arr_def;
    const alx::json_array& arr = jv.to_array(arr_def);
    ASSERT_EQ(arr.size(), 2U);
    EXPECT_EQ(arr[0].to_string().data(), payload);
    EXPECT_EQ(arr[0].to_string(), std::string(1000, 'd'));
    EXPECT_EQ(arr[1].to_string(), std::string(1000, 'e'));
}

TEST(gt_avarjson, value_from_variant_move_list_string) {
    alx::variant v(std::list<std::string>{std::string(1000, 'f'), std::string(1000, 'g')});
    const char* payload = v.to<std::list<std::string>>().front().data();

    alx::json_value jv = alx::json_value::from_variant(std::move(v));
    alx::json_array arr_def;
    const alx::json_array& arr = jv.to_array(arr_def);
    ASSERT_EQ(arr.size(), 2U);
    EXPECT_EQ(arr[0].to_string().data(), payload);
    EXPECT_EQ(arr[1].to_string(), std::string(1000, 'g'));
}

TEST(gt_avarjson, value_from_variant_move_scalars_and_null) {

    EXPECT_EQ(alx::json_value::from_variant(alx::variant(42)).to_intg(), 42LL);

    EXPECT_EQ(alx::json_value::from_variant(alx::variant((char) 65)).to_intg(), 65LL);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant((short) -1)).to_intg(), -1LL);
    EXPECT_DOUBLE_EQ(alx::json_value::from_variant(alx::variant(1.5f)).to_double(), 1.5);
    EXPECT_EQ(alx::json_value::from_variant(alx::variant(true)).to_bool(), true);
    EXPECT_DOUBLE_EQ(alx::json_value::from_variant(alx::variant(3.14)).to_double(), 3.14);
    EXPECT_TRUE(alx::json_value::from_variant(alx::variant()).is_null());

    alx::bytes b(5, 'a');
    alx::json_value jv = alx::json_value::from_variant(alx::variant(b));
    EXPECT_EQ(jv.to_string(), b.to_base64());
}

TEST(gt_avarjson, value_from_variant_move_keeps_source_shape) {
    alx::variant v(alx::varmap{{"a", std::string(1000, 'x')}, {"n", 7LL}});
    const char* payload = v.to<alx::varmap>().value("a").to<std::string>().data();
    alx::json_value jv = alx::json_value::from_variant(std::move(v));

    EXPECT_EQ(v.to<alx::varmap>().size(), 2U);
    EXPECT_TRUE(v.to<alx::varmap>().value("a").to<std::string>().empty());
    EXPECT_EQ(v.to<alx::varmap>().value("n").to<long long>(), 7LL);
    EXPECT_EQ(jv.to_object().value("a").to_string().data(), payload);
}

TEST(gt_avarjson, value_take_variant_move) {
    alx::json_value jv(std::string(1000, 'y'));
    const char* payload = jv.to_string().data();

    alx::variant v = jv.take_variant();
    EXPECT_EQ(v.to<std::string>().data(), payload);
    EXPECT_EQ(v.to<std::string>(), std::string(1000, 'y'));
    EXPECT_TRUE(jv.to_string().empty());

    alx::json_object obj;
    obj.insert("k", alx::json_value(std::string(1000, 'z')));
    const char* payload2 = obj.value("k").to_string().data();

    alx::json_value jv2(std::move(obj));
    alx::variant v2 = jv2.take_variant();
    EXPECT_EQ(v2.to<alx::varmap>().value("k").to<std::string>().data(), payload2);
    EXPECT_TRUE(jv2.to_object().value("k").to_string().empty());
}

TEST(gt_avarjson, value_take_variant_scalars_and_null) {
    EXPECT_EQ(alx::json_value(42LL).take_variant().to<long long>(), 42LL);
    EXPECT_EQ(alx::json_value(true).take_variant().to<bool>(), true);
    EXPECT_DOUBLE_EQ(alx::json_value(3.14).take_variant().to<double>(), 3.14);
    EXPECT_TRUE(alx::json_value().take_variant().null());
}

TEST(gt_avarjson, container_from_move) {
    alx::varmap vm;
    vm.insert("k", std::string(1000, 'p'));
    const char* payload = vm.value("k").to<std::string>().data();
    alx::json_object obj = alx::json_object::from_varmap(std::move(vm));
    EXPECT_EQ(obj.value("k").to_string().data(), payload);

    alx::varvec vv{std::string(1000, 'q')};
    const char* payload2 = vv[0].to<std::string>().data();
    alx::json_array arr = alx::json_array::from_varvec(std::move(vv));
    EXPECT_EQ(arr[0].to_string().data(), payload2);

    alx::varlst vl{std::string(1000, 'r')};
    const char* payload3 = vl.begin()->to<std::string>().data();
    alx::json_array arr2 = alx::json_array::from_varlst(std::move(vl));
    EXPECT_EQ(arr2[0].to_string().data(), payload3);
}

TEST(gt_avarjson, container_take_move) {
    alx::json_object obj;
    obj.insert("k", alx::json_value(std::string(1000, 'p')));
    const char* payload = obj.value("k").to_string().data();
    alx::varmap vm = obj.take_varmap();
    EXPECT_EQ(vm.value("k").to<std::string>().data(), payload);
    EXPECT_EQ(vm.value("k").to<std::string>(), std::string(1000, 'p'));
    EXPECT_TRUE(obj.value("k").to_string().empty());

    alx::json_array arr;
    arr.append(alx::json_value(std::string(1000, 'q')));
    const char* payload2 = arr[0].to_string().data();
    alx::varvec vv = arr.take_varvec();
    EXPECT_EQ(vv[0].to<std::string>().data(), payload2);

    alx::json_array arr2;
    arr2.append(alx::json_value(std::string(1000, 'r')));
    const char* payload3 = arr2[0].to_string().data();
    alx::varlst vl = arr2.take_varlst();
    EXPECT_EQ(vl.begin()->to<std::string>().data(), payload3);
}

TEST(gt_avarjson, serialization_golden_text) {

    alx::json_object obj;
    obj.insert("a", alx::json_value(1LL));
    obj.insert("b", alx::json_value(alx::json_array{alx::json_value(true), alx::json_value(std::string("x"))}));

    EXPECT_EQ(alx::json_doc::to_json(obj, true), "{\"a\":1,\"b\":[true,\"x\"]}");
    EXPECT_EQ(alx::json_doc::to_json(obj, false), "{\n\t\"a\": 1,\n\t\"b\": [\n\t\ttrue,\n\t\t\"x\"\n\t]\n}");
}

TEST(gt_avarjson, array_from_rvalue_scalar_containers) {

    std::vector<int> vi{1, 2, 3};
    alx::json_array a1 = alx::json_array::from_vector<int, long long>(std::move(vi));
    ASSERT_EQ(a1.size(), 3U);
    EXPECT_EQ(a1[0].to_intg(), 1LL);
    EXPECT_EQ(a1[2].to_intg(), 3LL);

    std::vector<bool> vb{true, false};
    alx::json_array a2 = alx::json_array::from_vector<bool, bool>(std::move(vb));
    ASSERT_EQ(a2.size(), 2U);
    EXPECT_EQ(a2[0].to_bool(), true);
    EXPECT_EQ(a2[1].to_bool(), false);

    std::list<int> li{7, 8};
    alx::json_array a3 = alx::json_array::from_list<int, long long>(std::move(li));
    ASSERT_EQ(a3.size(), 2U);
    EXPECT_EQ(a3[1].to_intg(), 8LL);
    EXPECT_EQ(li.size(), 2U);
}

TEST(gt_avarjson, array_to_varlst) {
    alx::json_array arr;
    arr.append(alx::json_value(1LL));
    arr.append(alx::json_value(std::string(1000, 'x')));

    alx::varlst vl = arr.to_varlst();
    ASSERT_EQ(vl.size(), 2U);
    alx::varlst::iterator it = vl.begin();
    EXPECT_EQ(it->to<long long>(), 1LL);
    ++it;
    EXPECT_EQ(it->to<std::string>(), std::string(1000, 'x'));

    EXPECT_NE(it->to<std::string>().data(), arr[1].to_string().data());
    EXPECT_EQ(arr[1].to_string(), std::string(1000, 'x'));
}

TEST(gt_avarjson, move_roundtrip) {
    alx::varmap vm{{"n", 7LL}, {"s", std::string(1000, 'm')}, {"v", alx::varvec{1LL, 2LL}}};
    const char* payload = vm.value("s").to<std::string>().data();

    alx::json_value jv = alx::json_value::from_variant(std::move(vm));
    alx::variant back = jv.take_variant();

    ASSERT_TRUE(back.is_map());
    EXPECT_EQ(back.to<alx::varmap>().value("n").to<long long>(), 7LL);

    EXPECT_EQ(back.to<alx::varmap>().value("s").to<std::string>().data(), payload);
    EXPECT_EQ(back.to<alx::varmap>().value("s").to<std::string>(), std::string(1000, 'm'));
    ASSERT_TRUE(back.to<alx::varmap>().value("v").is_vec());
    EXPECT_EQ(back.to<alx::varmap>().value("v").to<alx::varvec>().size(), 2U);
    EXPECT_EQ(back.to<alx::varmap>().value("v").to<alx::varvec>()[1].to<long long>(), 2LL);
}

TEST(gt_avarjson, to_json_move_matches_const_and_eats) {
    alx::json_object obj = get_complex_jobj();

    const std::string copy_text = alx::json_doc::to_json(obj, true);
    EXPECT_FALSE(obj.empty());

    const std::string eat_text = alx::json_doc::to_json(std::move(obj), true);
    EXPECT_EQ(eat_text, copy_text);
    EXPECT_TRUE(obj.empty());
}

TEST(gt_avarjson, to_json_move_incompact) {
    alx::json_object obj = get_complex_jobj();

    const std::string copy_text = alx::json_doc::to_json(obj, false);
    EXPECT_FALSE(obj.empty());

    const std::string eat_text = alx::json_doc::to_json(std::move(obj), false);
    EXPECT_EQ(eat_text, copy_text);
    EXPECT_TRUE(obj.empty());
}

TEST(gt_avarjson, to_string_move_matches_const_and_eats) {
    alx::json_value arr(alx::json_array{alx::json_value(1LL), alx::json_value(std::string(1000, 'x'))});
    const std::string copy_text = alx::json_doc::to_string(arr, true);
    EXPECT_FALSE(arr.to_array().empty());

    const std::string eat_text = alx::json_doc::to_string(std::move(arr), true);
    EXPECT_EQ(eat_text, copy_text);
    EXPECT_TRUE(arr.to_array().empty());

    EXPECT_EQ(alx::json_doc::to_string(alx::json_value(42LL), true), "42");
    EXPECT_EQ(alx::json_doc::to_string(alx::json_value(std::string("a\"b")), true), "\"a\\\"b\"");
}

TEST(gt_avarjson, full_json_variant_roundtrip) {
    alx::json_object& jobj = get_complex_jobj();
    ASSERT_EQ(jobj.size(), 6U);
    ASSERT_TRUE(jobj["array"].is_array());
    ASSERT_TRUE(jobj["nested"].is_object());

    alx::varmap vm = jobj.to_varmap();
    alx::json_object obj2 = alx::json_object::from_varmap(vm);

    std::string json1 = alx::json_doc::to_json(jobj, true);
    std::string json2 = alx::json_doc::to_json(obj2, true);
    ASSERT_EQ(json1, json2);
}
