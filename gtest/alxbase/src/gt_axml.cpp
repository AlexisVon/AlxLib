/*****************************************************************/ /**
 * \file   gt_axml.cpp
 * \brief  Unit tests for XML value / object / parse / serialize
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "axml.h"
#include <gtest/gtest.h>

TEST(gt_axml_value, construct_valid) {
    alx::xml_value elem("root");
    EXPECT_TRUE(elem.valid());
    EXPECT_EQ(elem.name(), "root");
    EXPECT_TRUE(elem.attr().empty());
    EXPECT_TRUE(elem.cont().empty());
}

TEST(gt_axml_value, construct_empty) {
    alx::xml_value elem("");
    EXPECT_FALSE(elem.valid());
}

TEST(gt_axml_value, move_construct) {
    alx::xml_value a("root");
    a.attr().insert("k", "v");
    alx::xml_value b(std::move(a));
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.attr().value("k"), "v");
}

TEST(gt_axml_value, move_assign) {
    alx::xml_value a("root");
    a.attr().insert("k", "v");
    alx::xml_value b("other");
    b = std::move(a);
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.attr().value("k"), "v");
}

TEST(gt_axml_value, attr_insert_and_value) {
    alx::xml_value elem("root");
    elem.attr().insert("id", "42");
    elem.attr().insert("class", "main");
    EXPECT_TRUE(elem.attr().contain("id"));
    EXPECT_TRUE(elem.attr().contain("class"));
    EXPECT_FALSE(elem.attr().contain("none"));
    EXPECT_EQ(elem.attr().value("id"), "42");
    EXPECT_EQ(elem.attr().value("class"), "main");
}

TEST(gt_axml_value, attr_insert_overwrite) {
    alx::xml_value elem("root");
    elem.attr().insert("k", "v1");
    elem.attr().insert("k", "v2");
    EXPECT_EQ(elem.attr().value("k"), "v2");
}

TEST(gt_axml_value, attr_remove) {
    alx::xml_value elem("root");
    elem.attr().insert("k", "v");
    elem.attr().remove("k");
    EXPECT_FALSE(elem.attr().contain("k"));
}

TEST(gt_axml_value, attr_clear) {
    alx::xml_value elem("root");
    elem.attr().insert("a", "1");
    elem.attr().insert("b", "2");
    elem.attr().clear();
    EXPECT_TRUE(elem.attr().empty());
}

TEST(gt_axml_value, attr_missing_key) {
    alx::xml_value elem("root");
    EXPECT_EQ(elem.attr().value("nope"), "");
}

TEST(gt_axml_value, content_add_text) {
    alx::xml_value elem("root");
    std::string* p = elem.cont().add_text("hello");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, "hello");
    EXPECT_EQ(elem.cont().size(), 1U);
}

TEST(gt_axml_value, content_add_elem) {
    alx::xml_value elem("root");
    alx::xml_value* child = elem.cont().add_elem("child");
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->name(), "child");
}

TEST(gt_axml_value, content_add_elem_full) {
    alx::xml_value elem("root");
    alx::xml_value* child = elem.cont().add_elem("child",
                                                 {{"id", "1"}, {"class", "box"}},
                                                 {"text content"});
    EXPECT_EQ(child->attr().value("id"), "1");
    EXPECT_EQ(child->attr().value("class"), "box");
    EXPECT_EQ(child->cont().size(), 1U);
}

TEST(gt_axml_value, content_get_elem) {
    alx::xml_value elem("root");
    elem.cont().add_elem("a");
    elem.cont().add_elem("b");
    elem.cont().add_elem("a");
    EXPECT_EQ(elem.cont().get_elem("a").size(), 2U);
    EXPECT_EQ(elem.cont().get_elem("b").size(), 1U);
    EXPECT_TRUE(elem.cont().get_elem("c").empty());
}

TEST(gt_axml_value, content_add_note) {
    alx::xml_value elem("root");
    std::string* n = elem.cont().add_note(" comment ");
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(*n, " comment ");
}

TEST(gt_axml_value, content_add_cdat) {
    alx::xml_value elem("root");
    std::string* c = elem.cont().add_cdat("<raw>");
    EXPECT_EQ(*c, "<raw>");
}

TEST(gt_axml_value, content_add_pi) {
    alx::xml_value elem("root");
    std::string* p = elem.cont().add_pi__("xml version=\"1.0\"");
    EXPECT_EQ(*p, "xml version=\"1.0\"");
}

TEST(gt_axml_value, content_add_enty) {
    alx::xml_value elem("root");
    std::string* e = elem.cont().add_enty("custom_entity");
    EXPECT_EQ(*e, "custom_entity");
}

TEST(gt_axml_value, content_type_values) {
    EXPECT_EQ(static_cast<alx::uint_8>(alx::xml_value::NONE), 0U);
    EXPECT_EQ(static_cast<alx::uint_8>(alx::xml_value::ELEM), 0x0FU);
    EXPECT_EQ(static_cast<alx::uint_8>(alx::xml_value::STRI), 0xF0U);
    EXPECT_GT(static_cast<alx::uint_8>(alx::xml_value::TEXT), 0xF0U);
    EXPECT_GT(static_cast<alx::uint_8>(alx::xml_value::NOTE), 0xF0U);
}

TEST(gt_axml_value, escape_special) {
    EXPECT_EQ(alx::xml_value::escape("&"), "&amp;");
    EXPECT_EQ(alx::xml_value::escape("<"), "&lt;");
    EXPECT_EQ(alx::xml_value::escape(">"), "&gt;");
    EXPECT_EQ(alx::xml_value::escape("\""), "&quot;");
    EXPECT_EQ(alx::xml_value::escape("'"), "&apos;");
}

TEST(gt_axml_value, escape_combined) {
    EXPECT_EQ(alx::xml_value::escape("<tag attr=\"val\">"),
              "&lt;tag attr=&quot;val&quot;&gt;");
}

TEST(gt_axml_value, escape_noop) {
    EXPECT_EQ(alx::xml_value::escape("hello"), "hello");
    EXPECT_EQ(alx::xml_value::escape(""), "");
}

TEST(gt_axml_value, to_bytes_compact_empty) {
    alx::xml_value elem("br");
    EXPECT_EQ(elem.to_bytes(true).to_string(), "<br/>");
}

TEST(gt_axml_value, to_bytes_compact_text) {
    alx::xml_value elem("title");
    elem.cont().add_text("Hello");
    EXPECT_EQ(elem.to_bytes(true).to_string(), "<title>Hello</title>");
}

TEST(gt_axml_value, to_bytes_compact_attrs) {
    alx::xml_value elem("div");
    elem.attr().insert("id", "main");
    elem.attr().insert("class", "container");
    std::string result = elem.to_bytes(true).to_string();
    EXPECT_NE(result.find("id=\"main\""), std::string::npos);
    EXPECT_NE(result.find("class=\"container\""), std::string::npos);
}

TEST(gt_axml_value, to_bytes_compact_nested) {
    alx::xml_value outer("outer");
    alx::xml_value* inner = outer.cont().add_elem("inner");
    inner->cont().add_text("val");
    EXPECT_EQ(outer.to_bytes(true).to_string(), "<outer><inner>val</inner></outer>");
}

TEST(gt_axml_value, to_bytes_compact_escape) {
    alx::xml_value elem("code");
    elem.cont().add_text("x < y && a > b");
    std::string result = elem.to_bytes(true).to_string();
    EXPECT_NE(result.find("&lt;"), std::string::npos);
    EXPECT_NE(result.find("&gt;"), std::string::npos);
    EXPECT_NE(result.find("&amp;"), std::string::npos);
}

TEST(gt_axml_value, to_bytes_pretty_empty) {
    alx::xml_value elem("br");
    EXPECT_EQ(elem.to_bytes(false).to_string(), "<br/>\n");
}

TEST(gt_axml_value, to_bytes_pretty_text) {
    alx::xml_value elem("p");
    elem.cont().add_text("Hello");
    std::string result = elem.to_bytes(false).to_string();
    EXPECT_NE(result.find("<p>Hello</p>"), std::string::npos);
}

TEST(gt_axml_value, to_bytes_pretty_nested) {
    alx::xml_value outer("outer");
    alx::xml_value* inner = outer.cont().add_elem("inner");
    inner->cont().add_text("val");
    std::string result = outer.to_bytes(false).to_string();
    EXPECT_NE(result.find('\n'), std::string::npos);
}

TEST(gt_axml_value, to_bytes_pretty_note) {
    alx::xml_value elem("root");
    elem.cont().add_note(" a note ");
    std::string result = elem.to_bytes(false).to_string();
    EXPECT_NE(result.find("<!--"), std::string::npos);
}

TEST(gt_axml_value, to_bytes_pretty_cdat) {
    alx::xml_value elem("root");
    elem.cont().add_cdat("<raw>");
    std::string result = elem.to_bytes(false).to_string();
    EXPECT_NE(result.find("<![CDATA["), std::string::npos);
}

TEST(gt_axml_value, to_json_text_only) {
    alx::xml_value elem("key");
    elem.cont().add_text("value");
    alx::json_value j = elem.to_json();
    EXPECT_TRUE(j.is_string());
    EXPECT_EQ(j.to_string(), "value");
}

TEST(gt_axml_value, to_json_with_attrs) {
    alx::xml_value elem("elem");
    elem.attr().insert("id", "1");
    elem.cont().add_text("body");
    alx::json_value j = elem.to_json();
    EXPECT_TRUE(j.is_object());
    alx::json_object obj_def;
    alx::json_object& obj = j.to_object(obj_def);
    EXPECT_EQ(obj["@_id"].to_string(), "1");
    EXPECT_EQ(obj["#text"].to_string(), "body");
}

TEST(gt_axml_value, to_json_duplicate_child) {
    alx::xml_value elem("list");
    elem.cont().add_elem("item", {}, {"a"});
    elem.cont().add_elem("item", {}, {"b"});
    alx::json_value j = elem.to_json();
    alx::json_object obj_def;
    alx::json_object& obj = j.to_object(obj_def);
    EXPECT_TRUE(obj["item"].is_array());
}

TEST(gt_axml_value, to_json_nested) {
    alx::xml_value outer("outer");
    alx::xml_value* inner = outer.cont().add_elem("inner");
    inner->cont().add_text("x");
    alx::json_value j = outer.to_json();
    alx::json_object obj_def;
    alx::json_object& obj = j.to_object(obj_def);
    EXPECT_EQ(obj["inner"].to_string(), "x");
}

TEST(gt_axml_object, from_bytes_simple) {
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes("<root/>"));
    EXPECT_TRUE(doc.valid());
    EXPECT_EQ(doc.name(), "root");
}

TEST(gt_axml_object, from_bytes_text) {
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes("<root>hello</root>"));
    EXPECT_EQ(doc.name(), "root");
    EXPECT_EQ(doc.cont().size(), 1U);
}

TEST(gt_axml_object, from_bytes_attrs) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root id=\"1\" class=\"main\"/>"));
    EXPECT_TRUE(doc.attr().contain("id"));
    EXPECT_EQ(doc.attr().value("id"), "1");
    EXPECT_EQ(doc.attr().value("class"), "main");
}

TEST(gt_axml_object, from_bytes_attr_single_quote) {
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes("<root k='v'/>"));
    EXPECT_EQ(doc.attr().value("k"), "v");
}

TEST(gt_axml_object, from_bytes_nested) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root><child id=\"1\">x</child><child id=\"2\">y</child></root>"));
    EXPECT_EQ(doc.cont().get_elem("child").size(), 2U);
}

TEST(gt_axml_object, from_bytes_comment) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root><!-- a comment --><x/></root>"));
    EXPECT_EQ(doc.cont().size(), 2U);
}

TEST(gt_axml_object, from_bytes_cdata) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root><![CDATA[raw <text>]]></root>"));
    EXPECT_EQ(doc.cont().size(), 1U);
}

TEST(gt_axml_object, from_bytes_pi_prefix) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<?xml version=\"1.0\"?><root/>"));
    EXPECT_TRUE(doc.valid());
}

TEST(gt_axml_object, from_bytes_doctype_prefix) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<!DOCTYPE html><root/>"));
    EXPECT_TRUE(doc.valid());
}

TEST(gt_axml_object, from_bytes_entity_ref) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root>a &lt; b</root>"));
    EXPECT_GT(doc.cont().size(), 0U);
}

TEST(gt_axml_object, from_bytes_self_closing_space) {
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes("<root />"));
    EXPECT_TRUE(doc.valid());
}

TEST(gt_axml_object, from_bytes_skip_blanks_off) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root>  \n  </root>"), false);
    EXPECT_GT(doc.cont().size(), 0U);
}

TEST(gt_axml_object, from_bytes_skip_blanks_on) {
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root>  \n  </root>"), true);
    EXPECT_EQ(doc.cont().size(), 0U);
}

TEST(gt_axml_object, from_bytes_error_unclosed) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes("<root>"), true, &ok);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(doc.valid());
}

TEST(gt_axml_object, from_bytes_error_mismatched_tag) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root></wrong>"), true, &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_axml_object, from_bytes_error_unclosed_child) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root><child>text</root>"), true, &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_axml_object, from_bytes_error_attr_without_value) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("<root><a b/></root>"), true, &ok);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(doc.valid());
}

TEST(gt_axml_object, from_bytes_error_empty) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(""), true, &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_axml_object, from_bytes_error_garbage) {
    bool ok = true;
    alx::xml_object doc = alx::xml_object::from_bytes(
        alx::bytes("not xml at all"), true, &ok);
    EXPECT_FALSE(ok);
}

TEST(gt_axml_object, to_bytes_compact_pi) {
    alx::xml_object doc("root");
    doc.add_pi__("xml version=\"1.0\"");
    std::string result = doc.to_bytes(true).to_string();
    EXPECT_NE(result.find("<?xml version=\"1.0\"?>"), std::string::npos);
    EXPECT_NE(result.find("<root/>"), std::string::npos);
}

TEST(gt_axml_object, to_bytes_pretty_note) {
    alx::xml_object doc("root");
    doc.add_note(" license info ");
    std::string result = doc.to_bytes(false).to_string();
    EXPECT_NE(result.find("<!--"), std::string::npos);
}

TEST(gt_axml_object, to_bytes_compact_doctype) {
    alx::xml_object doc("root");
    doc.add_dtd_("DOCTYPE html");
    std::string result = doc.to_bytes(true).to_string();
    EXPECT_NE(result.find("<!DOCTYPE html>"), std::string::npos);
}

TEST(gt_axml_object, to_json_with_global_pi) {
    alx::xml_object doc("root");
    doc.add_pi__("xml version=\"1.0\"");
    doc.cont().add_text("body");
    alx::json_object j = doc.to_json();
    EXPECT_EQ(j.size(), 2U);
}

TEST(gt_axml_object, roundtrip_simple) {
    std::string xml = "<root><child>val</child></root>";
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(xml));
    ASSERT_TRUE(doc.valid());
    std::string regenerated = doc.to_bytes(true).to_string();
    alx::xml_object doc2 = alx::xml_object::from_bytes(alx::bytes(regenerated));
    EXPECT_TRUE(doc2.valid());
    EXPECT_EQ(doc2.cont().size(), doc.cont().size());
}

TEST(gt_axml_object, roundtrip_with_attrs) {
    std::string xml = "<root id=\"1\" flag='y'><item k=\"v\"/></root>";
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(xml));
    ASSERT_TRUE(doc.valid());
    std::string regenerated = doc.to_bytes(true).to_string();
    alx::xml_object doc2 = alx::xml_object::from_bytes(alx::bytes(regenerated));
    EXPECT_TRUE(doc2.valid());
}

TEST(gt_axml_object, roundtrip_with_cdata_comment) {
    std::string xml = "<root><!-- note --><![CDATA[raw]]><x/></root>";
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(xml));
    ASSERT_TRUE(doc.valid());
    std::string regenerated = doc.to_bytes(true).to_string();
    alx::xml_object doc2 = alx::xml_object::from_bytes(alx::bytes(regenerated));
    EXPECT_TRUE(doc2.valid());
}

TEST(gt_axml_object, roundtrip_deep_nesting) {
    std::string xml = "<a><b><c>deep</c></b></a>";
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(xml));
    ASSERT_TRUE(doc.valid());
    std::string compact = doc.to_bytes(true).to_string();
    EXPECT_EQ(compact, xml);
}

TEST(gt_axml_object, roundtrip_siblings) {
    std::string xml =
        "<root>"
        "<a><a1>x</a1><a2>y</a2></a>"
        "<b><b1>z</b1></b>"
        "</root>";
    alx::xml_object doc = alx::xml_object::from_bytes(alx::bytes(xml));
    ASSERT_TRUE(doc.valid());
    EXPECT_EQ(doc.cont().get_elem("a").size(), 1U);
    EXPECT_EQ(doc.cont().get_elem("b").size(), 1U);
}

TEST(gt_axml_value, copy_construct) {
    alx::xml_value a("root");
    a.attr().insert("k", "v");
    a.cont().add_text("body");
    alx::xml_value b(a);
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.attr().value("k"), "v");
    EXPECT_EQ(b.cont().size(), 1U);

    a.name() = "changed";
    a.attr().insert("k", "v2");
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.attr().value("k"), "v");
}

TEST(gt_axml_value, copy_assign) {
    alx::xml_value a("root");
    a.attr().insert("k", "v");
    alx::xml_value b("other");
    EXPECT_EQ(b.name(), "other");
    b = a;
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.attr().value("k"), "v");

    b = b;
    EXPECT_EQ(b.name(), "root");
}

TEST(gt_axml_value, content_clear_and_empty) {
    alx::xml_value elem("root");
    EXPECT_TRUE(elem.cont().empty());
    elem.cont().add_text("x");
    EXPECT_FALSE(elem.cont().empty());
    elem.cont().clear();
    EXPECT_TRUE(elem.cont().empty());
    EXPECT_EQ(elem.cont().size(), 0U);
}

TEST(gt_axml_value, content_add_elem_ptr) {
    alx::xml_value elem("root");
    auto* child = new alx::xml_value("child");
    child->attr().insert("id", "1");
    alx::xml_value* ret = elem.cont().add_elem(child);
    EXPECT_EQ(ret, child);
    EXPECT_EQ(elem.cont().size(), 1U);
    EXPECT_EQ(elem.cont().get_elem("child").front()->attr().value("id"), "1");
}

TEST(gt_axml_value, content_add_meta) {
    alx::xml_value::content_meta meta;
    meta.set_stri("hello", alx::xml_value::TEXT);
    alx::xml_value elem("root");
    elem.cont().add_meta(std::move(meta));
    EXPECT_EQ(elem.cont().size(), 1U);
    EXPECT_EQ(elem.to_bytes(true).to_string(), "<root>hello</root>");
}

TEST(gt_axml_value, content_container) {
    alx::xml_value elem("root");
    elem.cont().add_text("a");
    elem.cont().add_text("b");
    const auto& c = elem.cont().container();
    EXPECT_EQ(c.size(), 2U);
}

TEST(gt_axml_value, get_elem_const) {
    const alx::xml_value elem = [] {
        alx::xml_value e("root");
        e.cont().add_elem("child");
        return e;
    }();
    auto list = elem.cont().get_elem("child");
    EXPECT_EQ(list.size(), 1U);
}

TEST(gt_axml_value, content_meta_default_null) {
    alx::xml_value::content_meta meta;
    EXPECT_TRUE(meta.is_null());
    EXPECT_EQ(meta.type(), alx::xml_value::NONE);
}

TEST(gt_axml_value, content_meta_set_stri) {
    alx::xml_value::content_meta meta;
    meta.set_stri("test", alx::xml_value::TEXT);
    EXPECT_FALSE(meta.is_null());
    EXPECT_EQ(meta.type(), alx::xml_value::TEXT);
    EXPECT_TRUE(meta.is_type(alx::xml_value::TEXT));
    EXPECT_FALSE(meta.is_type(alx::xml_value::ELEM));
}

TEST(gt_axml_value, content_meta_set_elem) {
    alx::xml_value::content_meta meta;
    meta.set_elem("child");
    EXPECT_FALSE(meta.is_null());
    EXPECT_EQ(meta.type(), alx::xml_value::ELEM);
    EXPECT_TRUE(meta.is_type(alx::xml_value::ELEM));
    EXPECT_FALSE(meta.is_type(alx::xml_value::TEXT));
}

TEST(gt_axml_value, content_meta_move_construct) {
    alx::xml_value::content_meta a;
    a.set_stri("test", alx::xml_value::NOTE);
    alx::xml_value::content_meta b(std::move(a));
    EXPECT_EQ(b.type(), alx::xml_value::NOTE);
    EXPECT_TRUE(a.is_null());
}

TEST(gt_axml_value, content_meta_move_assign) {
    alx::xml_value::content_meta a, b;
    a.set_stri("test", alx::xml_value::CDAT);
    b = std::move(a);
    EXPECT_EQ(b.type(), alx::xml_value::CDAT);
    EXPECT_TRUE(a.is_null());
}

TEST(gt_axml_value, content_meta_copy_construct) {
    alx::xml_value::content_meta a;
    a.set_elem("child");
    alx::xml_value::content_meta b(a);
    EXPECT_EQ(b.type(), alx::xml_value::ELEM);
    EXPECT_FALSE(a.is_null());
}

TEST(gt_axml_value, attr_container) {
    alx::xml_value elem("root");
    elem.attr().insert("a", "1");
    elem.attr().insert("b", "2");
    const auto& c = elem.attr().container();
    EXPECT_EQ(c.size(), 2U);
    EXPECT_EQ(c.at("a"), "1");
}

TEST(gt_axml_object, default_construct) {
    alx::xml_object doc;
    EXPECT_FALSE(doc.valid());
    EXPECT_TRUE(doc.name().empty());
}

TEST(gt_axml_object, copy_construct) {
    alx::xml_object a("root");
    a.add_pi__("xml version=\"1.0\"");
    a.cont().add_text("body");
    alx::xml_object b(a);
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.cont().size(), 1U);
    EXPECT_EQ(b.gcont().size(), 1U);
}

TEST(gt_axml_object, copy_assign) {
    alx::xml_object a("root");
    a.add_pi__("xml version=\"1.0\"");
    alx::xml_object b("other");
    b = a;
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.gcont().size(), 1U);
}

TEST(gt_axml_object, move_construct) {
    alx::xml_object a("root");
    a.add_note(" note ");
    alx::xml_object b(std::move(a));
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.gcont().size(), 1U);
}

TEST(gt_axml_object, move_assign) {
    alx::xml_object a("root");
    a.add_dtd_("DOCTYPE html");
    alx::xml_object b("other");
    b = std::move(a);
    EXPECT_EQ(b.name(), "root");
    EXPECT_EQ(b.gcont().size(), 1U);
}

TEST(gt_axml_object, gcont_accessor) {
    alx::xml_object doc("root");
    doc.add_note(" copyright ");
    doc.add_pi__("target");
    EXPECT_EQ(doc.gcont().size(), 2U);
}
