/*****************************************************************/ /**
 * \file   gt_ajson.cpp
 * \brief  Unit tests for JSON value / object / array / doc
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ajson.h"
#include "astream.h"
#include <gtest/gtest.h>
#include <utility>

TEST(gt_ajson_value, default_null) {
    alx::json_value v;
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_intg());
    EXPECT_FALSE(v.is_bool());
    EXPECT_FALSE(v.is_double());
    EXPECT_FALSE(v.is_string());
    EXPECT_FALSE(v.is_array());
    EXPECT_FALSE(v.is_object());
}

TEST(gt_ajson_value, intg) {
    alx::json_value v(42LL);
    EXPECT_TRUE(v.is_intg());
    EXPECT_EQ(v.to_intg(), 42LL);
}

TEST(gt_ajson_value, bool_true) {
    alx::json_value v(true);
    EXPECT_TRUE(v.is_bool());
    EXPECT_EQ(v.to_bool(), true);
}

TEST(gt_ajson_value, bool_false) {
    alx::json_value v(false);
    EXPECT_EQ(v.to_bool(), false);
}

TEST(gt_ajson_value, double_val) {
    alx::json_value v(3.14159);
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.to_double(), 3.14159);
}

TEST(gt_ajson_value, string_val) {
    alx::json_value v(std::string("hello"));
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.to_string(), "hello");
}

TEST(gt_ajson_value, cstr) {
    alx::json_value v("hello alxvar");
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.to_string(), "hello alxvar");
}

TEST(gt_ajson_value, narrowing_constructors) {
    EXPECT_EQ(alx::json_value((char) 65).to_intg(), 65LL);
    EXPECT_EQ(alx::json_value((unsigned char) 66).to_intg(), 66LL);
    EXPECT_EQ(alx::json_value((short) -1).to_intg(), -1LL);
    EXPECT_EQ(alx::json_value((unsigned short) 100).to_intg(), 100LL);
    EXPECT_EQ(alx::json_value(42).to_intg(), 42LL);
    EXPECT_EQ(alx::json_value(100U).to_intg(), 100LL);
    EXPECT_EQ(alx::json_value(999999999999ULL).to_intg(), 999999999999LL);
}

TEST(gt_ajson_value, from_bytes) {
    alx::bytes b(3, 'x');
    alx::json_value v(b);
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.to_string(), b.to_base64());
}

TEST(gt_ajson_value, ref_conversion_ok) {
    alx::json_value v(123LL);
    long long def = 0;
    bool ok = false;
    long long& ref = v.to_intg(def, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ref, 123LL);
}

TEST(gt_ajson_value, ref_conversion_wrong_type) {
    alx::json_value v(123LL);
    double def = 0;
    bool ok = true;
    v.to_double(def, &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(def, 0);
}

TEST(gt_ajson_object, empty) {
    alx::json_object obj;
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson_object, insert_and_access) {
    alx::json_object obj;
    obj.insert("int_key", alx::json_value(42LL));
    obj.insert("str_key", alx::json_value(std::string("hello")));
    obj.insert("bool_key", alx::json_value(true));
    obj.insert("double_key", alx::json_value(3.14));
    obj.insert("null_key", alx::json_value());

    EXPECT_EQ(obj.size(), 5U);
    EXPECT_TRUE(obj["int_key"].is_intg());
    EXPECT_EQ(obj["int_key"].to_intg(), 42LL);
    EXPECT_EQ(obj["str_key"].to_string(), "hello");
    EXPECT_EQ(obj["bool_key"].to_bool(), true);
    EXPECT_DOUBLE_EQ(obj["double_key"].to_double(), 3.14);
    EXPECT_TRUE(obj["null_key"].is_null());
}

TEST(gt_ajson_object, nested) {
    alx::json_object inner;
    inner.insert("x", alx::json_value(1LL));

    alx::json_object outer;
    outer.insert("inner", inner);

    EXPECT_TRUE(outer["inner"].is_object());
    alx::json_object inner_def;
    alx::json_object& retrieved = outer["inner"].to_object(inner_def);
    EXPECT_EQ(retrieved.size(), 1U);
    EXPECT_EQ(retrieved["x"].to_intg(), 1LL);
}

TEST(gt_ajson_array, append_and_index) {
    alx::json_array arr;
    arr.append(alx::json_value(1LL));
    arr.append(alx::json_value(std::string("two")));
    arr.append(alx::json_value(true));
    arr.append(alx::json_value(3.14));

    EXPECT_EQ(arr.size(), 4U);
    EXPECT_EQ(arr[0].to_intg(), 1LL);
    EXPECT_EQ(arr[1].to_string(), "two");
    EXPECT_EQ(arr[2].to_bool(), true);
    EXPECT_DOUBLE_EQ(arr[3].to_double(), 3.14);
}

TEST(gt_ajson_array, initializer_list) {
    alx::json_array arr{alx::json_value(1LL), alx::json_value(2LL), alx::json_value(3LL)};
    EXPECT_EQ(arr.size(), 3U);
    EXPECT_EQ(arr[0].to_intg(), 1LL);
    EXPECT_EQ(arr[1].to_intg(), 2LL);
    EXPECT_EQ(arr[2].to_intg(), 3LL);
}

TEST(gt_ajson_array, at_access) {
    alx::json_array arr{alx::json_value(1LL), alx::json_value(2LL)};
    EXPECT_EQ(arr.at(0).to_intg(), 1LL);
    EXPECT_EQ(arr.at(1).to_intg(), 2LL);
}

TEST(gt_ajson_array, nested) {
    alx::json_array inner;
    inner.append(alx::json_value(1LL));
    inner.append(alx::json_value(2LL));

    alx::json_array outer;
    outer.append(inner);

    EXPECT_TRUE(outer[0].is_array());
    const alx::json_array& retrieved = outer[0].to_array(alx::json_array());
    EXPECT_EQ(retrieved.size(), 2U);
    EXPECT_EQ(retrieved[0].to_intg(), 1LL);
}

TEST(gt_ajson_array, copy_is_independent) {
    alx::json_array src;
    src.append(alx::json_value(std::string("first")));

    alx::json_array copy = src;
    src[0] = alx::json_value(std::string("changed"));
    EXPECT_EQ(copy[0].to_string(), "first");

    alx::json_array assigned;
    assigned = src;
    src[0] = alx::json_value(std::string("again"));
    EXPECT_EQ(assigned[0].to_string(), "changed");
}

TEST(gt_ajson_array, move_steals_elements) {
    alx::json_array src;
    src.append(alx::json_value(1LL));
    src.append(alx::json_value(2LL));

    alx::json_array moved = std::move(src);
    EXPECT_EQ(moved.size(), 2U);
    EXPECT_EQ(moved[1].to_intg(), 2LL);
    EXPECT_TRUE(src.empty());

    alx::json_array target;
    target.append(alx::json_value(9LL));
    target = std::move(moved);
    EXPECT_EQ(target.size(), 2U);
    EXPECT_EQ(target[0].to_intg(), 1LL);
    EXPECT_TRUE(moved.empty());
}

TEST(gt_ajson_array, equal_compares_elements) {
    alx::json_array a{alx::json_value(1LL), alx::json_value(std::string("x"))};
    alx::json_array b{alx::json_value(1LL), alx::json_value(std::string("x"))};
    alx::json_array c{alx::json_value(1LL), alx::json_value(std::string("y"))};
    alx::json_array d{alx::json_value(1LL)};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a != d);
    EXPECT_TRUE(alx::json_value(a) == alx::json_value(b));
}

TEST(gt_ajson_array, self_assignment) {
    alx::json_array arr;
    arr.append(alx::json_value(7LL));

    alx::json_array& self = arr;
    arr = self;
    EXPECT_EQ(arr.size(), 1U);
    EXPECT_EQ(arr[0].to_intg(), 7LL);
}

TEST(gt_ajson_doc, to_json_compact_empty) {
    alx::json_object obj;
    EXPECT_EQ(alx::json_doc::to_json(obj, true), "{}");
}

TEST(gt_ajson_doc, to_json_compact_simple) {
    alx::json_object obj;
    obj.insert("key", alx::json_value(42LL));
    EXPECT_EQ(alx::json_doc::to_json(obj, true), "{\"key\":42}");
}

TEST(gt_ajson_doc, to_json_compact_multi_types) {
    alx::json_object obj;
    obj.insert("int_key", alx::json_value(8LL));
    obj.insert("bool_key", alx::json_value(false));
    obj.insert("double_key", alx::json_value(3.141592653589793));
    obj.insert("string_key", alx::json_value(std::string("hello alxvar")));
    std::string result = alx::json_doc::to_json(obj, true);
    EXPECT_NE(result.find("\"int_key\":8"), std::string::npos);
    EXPECT_NE(result.find("\"bool_key\":false"), std::string::npos);
    EXPECT_NE(result.find("\"string_key\":\"hello alxvar\""), std::string::npos);
}

TEST(gt_ajson_doc, to_json_compact_array) {
    alx::json_object obj;
    alx::json_array arr;
    arr.append(alx::json_value(1LL));
    arr.append(alx::json_value(2LL));
    arr.append(alx::json_value(3LL));
    obj.insert("numbers", arr);
    EXPECT_EQ(alx::json_doc::to_json(obj, true), "{\"numbers\":[1,2,3]}");
}

TEST(gt_ajson_doc, to_json_compact_nested) {
    alx::json_object inner;
    inner.insert("x", alx::json_value(1LL));
    alx::json_object outer;
    outer.insert("inner", inner);
    EXPECT_EQ(alx::json_doc::to_json(outer, true), "{\"inner\":{\"x\":1}}");
}

static std::string json_of_double(double _v) {
    alx::json_object obj;
    obj.insert("v", alx::json_value(_v));
    return alx::json_doc::to_json(obj, true);
}

TEST(gt_ajson_doc, to_json_double_shortest) {
    EXPECT_EQ(json_of_double(1.5), "{\"v\":1.5}");
    EXPECT_EQ(json_of_double(0.1), "{\"v\":0.1}");
    EXPECT_EQ(json_of_double(100.0), "{\"v\":100.0}");
    EXPECT_EQ(json_of_double(2.0), "{\"v\":2.0}");
    EXPECT_EQ(json_of_double(1e20), "{\"v\":1e+20}");
    EXPECT_EQ(json_of_double(3.14159265358979), "{\"v\":3.14159265358979}");
}

TEST(gt_ajson_doc, to_json_double_round_trip) {
    double vals[] = {0.0, 1.5, 2.0, 0.1, -0.5, 3.14159265358979, 1e20, 1e-7, 0.1 + 0.2,
                     12345.6, 1e15};
    for (double v : vals) {
        bool ok = false;
        alx::json_object doc = alx::json_doc::from_value(json_of_double(v), &ok).to_object();
        ASSERT_TRUE(ok) << v;
        ASSERT_TRUE(doc["v"].is_double()) << v;
        EXPECT_EQ(doc["v"].to_double(), v) << v;
    }
}

TEST(gt_ajson_doc, from_json_bare_number_at_end) {
    bool ok = false;
    EXPECT_EQ(alx::json_doc::from_value("42", &ok).to_intg(), 42);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(alx::json_doc::from_value("1.5", &ok).is_double());
    EXPECT_TRUE(ok);
    EXPECT_EQ(alx::json_doc::from_value("1.5", &ok).to_double(), 1.5);
    EXPECT_EQ(alx::json_doc::from_value("1e3", &ok).to_double(), 1000.0);
    EXPECT_EQ(alx::json_doc::from_value("1.5e3", &ok).to_double(), 1500.0);
    EXPECT_TRUE(alx::json_doc::from_value("1.", &ok).is_null());
}

TEST(gt_ajson_doc, to_json_double_non_finite) {

    EXPECT_EQ(json_of_double(1.0 / 0.0), "{\"v\":null}");
    EXPECT_EQ(json_of_double(-1.0 / 0.0), "{\"v\":null}");
    EXPECT_EQ(json_of_double(0.0 / 0.0), "{\"v\":null}");
}

TEST(gt_ajson_doc, to_json_pretty) {
    alx::json_object obj;
    obj.insert("key", alx::json_value(42LL));
    std::string result = alx::json_doc::to_json(obj, false);
    EXPECT_NE(result.find('\n'), std::string::npos);
    EXPECT_NE(result.find("key"), std::string::npos);
    EXPECT_NE(result.find("42"), std::string::npos);
}

TEST(gt_ajson_doc, escape_quote) {
    std::string out;
    alx::json_doc::escape("hello\"world", out, 0);
    EXPECT_EQ(out, "hello\\\"world");
}

TEST(gt_ajson_doc, escape_in_place) {
    std::string s = "hello\"world";
    alx::json_doc::escape(s, s, 0);
    EXPECT_EQ(s, "hello\\\"world");
}

TEST(gt_ajson_doc, escape_offset_appends) {
    std::string out = "prefix";
    alx::json_doc::escape("a\"b", out, out.size());
    EXPECT_EQ(out, "prefixa\\\"b");
}

TEST(gt_ajson_doc, escape_keeps_prefix_before_ofst) {
    std::string mid = "PREFIXxx";
    alx::json_doc::escape("a\"b", mid, 6);
    EXPECT_EQ(mid, "PREFIXa\\\"b");

    std::string gap = "PFX";
    alx::json_doc::escape("a\"b", gap, 6);
    EXPECT_EQ(gap, std::string("PFX\0\0\0", 6) + "a\\\"b");

    std::string back = "KEEPxx";
    alx::json_doc::descape("a\\\"b", back, 4);
    EXPECT_EQ(back, "KEEPa\"b");
}

TEST(gt_ajson_doc, escape_control_chars) {
    std::string out;
    alx::json_doc::escape(std::string(1, '\x01') + "\b\f", out, 0);
    EXPECT_EQ(out, "\\u0001\\b\\f");
}

TEST(gt_ajson_doc, descape_quote) {
    std::string s = "hello\\\"world";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, "hello\"world");
}

TEST(gt_ajson_doc, descape_short_forms) {
    std::string s = "\\u0001\\b\\f";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, std::string(1, '\x01') + "\b\f");
}

TEST(gt_ajson_doc, descape_offset_appends) {
    std::string out = "prefix";
    alx::json_doc::descape("a\\\"b", out, out.size());
    EXPECT_EQ(out, "prefixa\"b");
}

TEST(gt_ajson_doc, escape_descape_roundtrip) {
    std::string original = "line1\nline2\rline3\ttab";
    std::string s = original;
    alx::json_doc::escape(s, s, 0);
    EXPECT_NE(s, original);
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, original);
}

TEST(gt_ajson_doc, escape_descape_roundtrip_all_c0) {
    std::string original;
    for (int c = 0; c < 0X20; c++) original += (char) c;
    original += "plain \xE4\xB8\xAD\xE6\x96\x87";
    std::string s = original;
    alx::json_doc::escape(s, s, 0);
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, original);
}

TEST(gt_ajson_doc, to_json_escapes_keys) {
    alx::json_object obj;
    obj.insert("a\"b", alx::json_value(1LL));
    EXPECT_EQ(alx::json_doc::to_json(obj, true), "{\"a\\\"b\":1}");
}

TEST(gt_ajson_doc, to_json_pretty_escapes_keys) {
    alx::json_object obj;
    obj.insert("a\"b", alx::json_value(1LL));
    EXPECT_EQ(alx::json_doc::to_json(obj, false), "{\n\t\"a\\\"b\": 1\n}");
}

TEST(gt_ajson_doc, escaped_key_roundtrip) {
    bool ok = false;
    alx::json_object doc = alx::json_doc::from_json("{\"a\\\"b\": 1}", &ok);
    ASSERT_TRUE(ok);
    const std::string out = alx::json_doc::to_json(doc, true);
    EXPECT_EQ(out, "{\"a\\\"b\":1}");
    alx::json_doc::from_json(out, &ok);
    EXPECT_TRUE(ok);
}

TEST(gt_ajson_doc, escape_every_c0_form) {
    for (int c = 0; c < 0X20; c++) {
        std::string out;
        alx::json_doc::escape(std::string(1, (char) c), out, 0);
        std::string want;
        switch (c) {
        case '\b': want = "\\b"; break;
        case '\f': want = "\\f"; break;
        case '\n': want = "\\n"; break;
        case '\r': want = "\\r"; break;
        case '\t': want = "\\t"; break;
        default: {
            const char hex[] = "0123456789abcdef";
            want = "\\u00";
            want += hex[c >> 4];
            want += hex[c & 0X0F];
            break;
        }
        }
        EXPECT_EQ(out, want) << "control " << c;
    }
}

TEST(gt_ajson_doc, escape_high_bytes_pass_through) {
    std::string original;
    for (int c = 0X80; c <= 0XFF; c++) original += (char) c;
    std::string out;
    alx::json_doc::escape(original, out, 0);
    EXPECT_EQ(out, original);
}

TEST(gt_ajson_doc, escape_descape_empty) {
    std::string out = "dropped";
    alx::json_doc::escape("", out, 0);
    EXPECT_TRUE(out.empty());
    out = "dropped";
    alx::json_doc::descape("", out, 0);
    EXPECT_TRUE(out.empty());

    std::string s;
    alx::json_doc::escape(s, s, 0);
    alx::json_doc::descape(s, s, 0);
    EXPECT_TRUE(s.empty());
}

TEST(gt_ajson_doc, descape_uppercase_unicode) {
    std::string s = "\\u004A\\u00E9";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, std::string("J\xC3\xA9"));
}

TEST(gt_ajson_doc, descape_truncated_unicode) {
    std::string s = "\\u00";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, "u00");
    s = "\\u";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, "u");
}

TEST(gt_ajson_doc, descape_trailing_backslash) {
    std::string s = "abc\\";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, "abc\\");
}

TEST(gt_ajson_doc, descape_unknown_escape_keeps_letter) {
    std::string s = "\\q\\/";
    alx::json_doc::descape(s, s, 0);
    EXPECT_EQ(s, "q/");
}

class bounded_ostream : public alx::ostream {
public:
    explicit bounded_ostream(alx::uint_64 _cap)
        : cap_(_cap) {}
    bool append(const void* _ptr, alx::uint_64 _size) override {
        if (size_ + _size > cap_) return false;
        buff_.append((const char*) _ptr, (size_t) _size);
        size_ += _size;
        return true;
    }
    bool flush() override { return true; }
    void reset() override {
        buff_.clear();
        size_ = 0;
    }
    alx::uint_64 total() const override { return size_; }
    const std::string& text() const { return buff_; }

private:
    alx::uint_64 cap_;
    alx::uint_64 size_{0};
    std::string buff_;
};

TEST(gt_ajson_doc, to_json_into_stream_matches_string) {
    alx::json_object obj;
    obj.insert("key", alx::json_value(42LL));
    obj.insert("a\"b", alx::json_value(std::string("x\ny")));

    bounded_ostream compact(1 << 16);
    ASSERT_TRUE(alx::json_doc::to_json(obj, compact, true));
    EXPECT_EQ(compact.text(), alx::json_doc::to_json(obj, true));

    bounded_ostream pretty(1 << 16);
    ASSERT_TRUE(alx::json_doc::to_json(obj, pretty, false));
    EXPECT_EQ(pretty.text(), alx::json_doc::to_json(obj, false));
}

TEST(gt_ajson_doc, to_json_into_bounded_stream_stops) {
    alx::json_object obj;
    obj.insert("key", alx::json_value(std::string("0123456789")));
    const std::string full = alx::json_doc::to_json(obj, true);

    bounded_ostream tiny(full.size() - 1);
    EXPECT_FALSE(alx::json_doc::to_json(obj, tiny, true));
    EXPECT_LE(tiny.total(), full.size() - 1);
    EXPECT_EQ(full.compare(0, (size_t) tiny.total(), tiny.text()), 0);

    bounded_ostream exact(full.size());
    EXPECT_TRUE(alx::json_doc::to_json(obj, exact, true));
    EXPECT_EQ(exact.text(), full);
}

TEST(gt_ajson_doc, to_string_into_stream_covers_shapes) {
    alx::json_array arr;
    arr.append(alx::json_value(1LL));
    arr.append(alx::json_value(std::string("two\"x")));
    const alx::json_value as_array(arr);

    bounded_ostream array_stream(1 << 16);
    ASSERT_TRUE(alx::json_doc::to_string(as_array, array_stream, true));
    EXPECT_EQ(array_stream.text(), alx::json_doc::to_string(as_array, true));

    const alx::json_value scalar(std::string("plain"));
    bounded_ostream scalar_stream(1 << 16);
    ASSERT_TRUE(alx::json_doc::to_string(scalar, scalar_stream, true));
    EXPECT_EQ(scalar_stream.text(), alx::json_doc::to_string(scalar, true));
}

TEST(gt_ajson_doc, to_json_move_into_stream) {
    alx::json_object obj;
    obj.insert("key", alx::json_value(42LL));
    obj.insert("nested", alx::json_value(alx::json_object()));
    const std::string want = alx::json_doc::to_json(obj, true);

    bounded_ostream stream(1 << 16);
    ASSERT_TRUE(alx::json_doc::to_json(std::move(obj), stream, true));
    EXPECT_EQ(stream.text(), want);
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson_doc, refusing_sink_leaves_a_destructible_tree) {
    alx::json_object obj;
    for (int i = 0; i < 8; i++) obj.insert("k" + std::to_string(i), alx::json_value((long long) i));

    bounded_ostream tiny(4);
    EXPECT_FALSE(alx::json_doc::to_json(std::move(obj), tiny, true));

}

TEST(gt_ajson_doc, from_json_empty) {
    alx::json_object obj = alx::json_doc::from_json("{}");
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson_doc, from_json_simple) {
    alx::json_object obj = alx::json_doc::from_json("{\"key\":42}");
    EXPECT_EQ(obj.size(), 1U);
    EXPECT_EQ(obj["key"].to_intg(), 42LL);
}

TEST(gt_ajson_doc, from_json_multi_types) {
    std::string json = R"({"int_key":8,"bool_key":false,"double_key":3.14,"string_key":"hello","null_key":null})";
    alx::json_object obj = alx::json_doc::from_json(json);
    EXPECT_EQ(obj.size(), 5U);
    EXPECT_EQ(obj["int_key"].to_intg(), 8LL);
    EXPECT_EQ(obj["bool_key"].to_bool(), false);
    EXPECT_DOUBLE_EQ(obj["double_key"].to_double(), 3.14);
    EXPECT_EQ(obj["string_key"].to_string(), "hello");
    EXPECT_TRUE(obj["null_key"].is_null());
}

TEST(gt_ajson_doc, from_json_array) {
    std::string json = R"({"values":[1,2,3]})";
    alx::json_object obj = alx::json_doc::from_json(json);
    EXPECT_TRUE(obj["values"].is_array());
    alx::json_array arr_def;
    alx::json_array& arr = obj["values"].to_array(arr_def);
    EXPECT_EQ(arr.size(), 3U);
    EXPECT_EQ(arr[0].to_intg(), 1LL);
    EXPECT_EQ(arr[1].to_intg(), 2LL);
    EXPECT_EQ(arr[2].to_intg(), 3LL);
}

TEST(gt_ajson_doc, from_json_nested_object) {
    std::string json = R"({"outer":{"inner":1}})";
    alx::json_object obj = alx::json_doc::from_json(json);
    EXPECT_TRUE(obj["outer"].is_object());
    alx::json_object inner_def;
    alx::json_object& inner = obj["outer"].to_object(inner_def);
    EXPECT_EQ(inner.size(), 1U);
    EXPECT_EQ(inner["inner"].to_intg(), 1LL);
}

TEST(gt_ajson_doc, from_json_with_whitespace) {
    std::string json = " { \"key\" : 42 , \"key2\" : true } ";
    alx::json_object obj = alx::json_doc::from_json(json);
    EXPECT_EQ(obj.size(), 2U);
    EXPECT_EQ(obj["key"].to_intg(), 42LL);
    EXPECT_EQ(obj["key2"].to_bool(), true);
}

TEST(gt_ajson_doc, from_json_string_escape) {
    std::string json = R"({"msg":"hello\nworld\t!"})";
    alx::json_object obj = alx::json_doc::from_json(json);
    EXPECT_EQ(obj["msg"].to_string(), "hello\nworld\t!");
}

TEST(gt_ajson_doc, from_json_bytes_view) {
    std::string json = R"({"key":42})";
    alx::bytes b(json.size(), (unsigned char) 0);
    memcpy(b.data(), json.data(), json.size());
    alx::json_object obj = alx::json_doc::from_json(alx::bytes_view(b));
    EXPECT_EQ(obj["key"].to_intg(), 42LL);
}

TEST(gt_ajson_doc, from_json_error) {
    bool ok = true;
    alx::json_object obj = alx::json_doc::from_json("{bad json}", &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson_doc, to_string_from_object) {
    alx::json_object obj;
    obj.insert("a", alx::json_value(1LL));
    EXPECT_EQ(alx::json_doc::to_string(alx::json_value(obj), true), "{\"a\":1}");
}

TEST(gt_ajson_doc, to_string_from_array) {
    alx::json_array arr;
    arr.append(alx::json_value(1LL));
    arr.append(alx::json_value(2LL));
    EXPECT_EQ(alx::json_doc::to_string(alx::json_value(arr), true), "[1,2]");
}

TEST(gt_ajson_doc, to_string_from_scalar) {
    EXPECT_EQ(alx::json_doc::to_string(alx::json_value(42LL), true), "42");
}

TEST(gt_ajson_doc, roundtrip_complex) {
    alx::json_object inner;
    inner.insert("int_key", alx::json_value(8LL));
    inner.insert("bool_key", alx::json_value(false));
    inner.insert("double_key", alx::json_value(3.141592653589793));
    inner.insert("string_key", alx::json_value(std::string("hello alxvar")));

    alx::json_object obj;
    for (int i = 0; i < 4; i++)
        obj.insert("key_" + std::to_string(i), inner);

    std::string json = alx::json_doc::to_json(obj, true);
    alx::json_object parsed = alx::json_doc::from_json(json);

    EXPECT_EQ(parsed.size(), 4U);
    alx::json_object inner_def;
    alx::json_object& retrieved = parsed["key_0"].to_object(inner_def);
    EXPECT_EQ(retrieved["int_key"].to_intg(), 8LL);
    EXPECT_EQ(retrieved["bool_key"].to_bool(), false);
    EXPECT_NEAR(retrieved["double_key"].to_double(), 3.141592653589793, 1e-6);
    EXPECT_EQ(retrieved["string_key"].to_string(), "hello alxvar");
}

TEST(gt_ajson_doc, from_json_number_edge_cases) {

    bool ok = false;
    alx::json_object obj = alx::json_doc::from_json(R"({"v":.5})", &ok);
    EXPECT_FALSE(ok);

    ok = false;
    obj = alx::json_doc::from_json(R"({"v":1.})", &ok);
    EXPECT_FALSE(ok);

    obj = alx::json_doc::from_json(R"({"v":-0})");
    EXPECT_EQ(obj["v"].to_intg(), 0LL);

    obj = alx::json_doc::from_json(R"({"v":1.0e+10})");
    EXPECT_DOUBLE_EQ(obj["v"].to_double(), 1.0e+10);

    obj = alx::json_doc::from_json(R"({"v":1E-5})");
    EXPECT_DOUBLE_EQ(obj["v"].to_double(), 1e-5);

    obj = alx::json_doc::from_json(R"({"v":9223372036854775807})");
    EXPECT_EQ(obj["v"].to_intg(), 9223372036854775807LL);

    obj = alx::json_doc::from_json(R"({"v":9223372036854775808})");
    EXPECT_TRUE(obj["v"].is_double());
    EXPECT_DOUBLE_EQ(obj["v"].to_double(), 9.2233720368547758e18);
}

TEST(gt_ajson_doc, from_json_trailing_comma_object) {

    alx::json_object obj = alx::json_doc::from_json(R"({"a":1,})");
    EXPECT_EQ(obj.size(), 1U);
    EXPECT_EQ(obj["a"].to_intg(), 1LL);
}

TEST(gt_ajson_doc, from_json_trailing_comma_array) {

    alx::json_object obj = alx::json_doc::from_json(R"({"a":[1,2,]})");
    EXPECT_EQ(obj.size(), 1U);
    EXPECT_TRUE(obj["a"].is_array());
}

TEST(gt_ajson_doc, from_json_missing_colon) {
    bool ok = false;
    alx::json_object obj = alx::json_doc::from_json(R"({"a" 1})", &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_ajson_doc, from_json_missing_value) {

    alx::json_object obj = alx::json_doc::from_json(R"({"a":})");
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson_doc, from_json_unmatched_bracket) {
    bool ok = false;
    alx::json_object obj = alx::json_doc::from_json(R"({"a":[1,2})", &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_ajson_doc, from_json_garbage_input) {
    bool ok = true;
    alx::json_object obj = alx::json_doc::from_json("not json at all", &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_ajson_doc, from_json_empty_string) {
    bool ok = false;
    alx::json_object obj = alx::json_doc::from_json("", &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_ajson_doc, from_json_unicode_escape) {

    alx::json_object obj = alx::json_doc::from_json(R"({"v":"A"})");
    EXPECT_EQ(obj["v"].to_string(), "A");

    obj = alx::json_doc::from_json(R"({"v":"Ω"})");
    EXPECT_EQ(obj["v"].to_string(), "\xCE\xA9");

    obj = alx::json_doc::from_json(R"({"v":"\u4e16"})");
    EXPECT_EQ(obj["v"].to_string(), "\xE4\xB8\x96");
}

TEST(gt_ajson, FromValueBareArray) {

    alx::json_value jv = alx::json_doc::from_value("[1, 2, 3]");
    EXPECT_TRUE(jv.is_array());
    auto& arr = jv.to_array();
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[0].to_intg(), 1);
    EXPECT_EQ(arr[1].to_intg(), 2);
    EXPECT_EQ(arr[2].to_intg(), 3);

    jv = alx::json_doc::from_value("[[1, 2], [3, 4]]");
    EXPECT_TRUE(jv.is_array());
    EXPECT_EQ(jv.to_array()[0].to_array()[1].to_intg(), 2);

    jv = alx::json_doc::from_value("[]");
    EXPECT_TRUE(jv.is_array());
    EXPECT_EQ(jv.to_array().size(), 0u);
}

TEST(gt_ajson, FromValueBareObject) {

    alx::json_value jv = alx::json_doc::from_value(R"({"a": 1, "b": "hello"})");
    EXPECT_TRUE(jv.is_object());
    EXPECT_TRUE(jv.to_object().contain("a"));
    EXPECT_EQ(jv.to_object().value("a").to_intg(), 1);
    EXPECT_EQ(jv.to_object().value("b").to_string(), "hello");
}

TEST(gt_ajson, FromValueScalar) {

    alx::json_value jv = alx::json_doc::from_value(R"("hello world")");
    EXPECT_TRUE(jv.is_string());
    EXPECT_EQ(jv.to_string(), "hello world");

    jv = alx::json_doc::from_value("42");
    EXPECT_TRUE(jv.is_intg());
    EXPECT_EQ(jv.to_intg(), 42);

    jv = alx::json_doc::from_value("3.14");
    EXPECT_TRUE(jv.is_double());
    EXPECT_DOUBLE_EQ(jv.to_double(), 3.14);

    jv = alx::json_doc::from_value("true");
    EXPECT_TRUE(jv.is_bool());
    EXPECT_TRUE(jv.to_bool());

    jv = alx::json_doc::from_value("false");
    EXPECT_TRUE(jv.is_bool());
    EXPECT_FALSE(jv.to_bool());

    jv = alx::json_doc::from_value("null");
    EXPECT_TRUE(jv.is_null());
}

TEST(gt_ajson, FromValueFail) {
    bool ok = true;
    alx::json_value jv = alx::json_doc::from_value("{broken", &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(jv.is_null());

    alx::json_object obj = alx::json_doc::from_json("[1,2,3]");
    EXPECT_TRUE(obj.empty());
}

TEST(gt_ajson, FromValueRoundTrip) {

    alx::json_value jv = alx::json_doc::from_value("[1, \"hi\", true, null, [2], {}]");
    std::string s = alx::json_doc::to_string(jv, true);
    alx::json_value jv2 = alx::json_doc::from_value(s);
    EXPECT_TRUE(jv2.is_array());
    EXPECT_EQ(jv2.to_array().size(), 6u);

    jv = alx::json_doc::from_value("42");
    s = alx::json_doc::to_string(jv, true);
    jv2 = alx::json_doc::from_value(s);
    EXPECT_EQ(jv2.to_intg(), 42);
}
