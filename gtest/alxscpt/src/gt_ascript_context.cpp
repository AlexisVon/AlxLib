/** ****************************************************************
 * \file   gt_ascript_context.cpp
 * \brief  fwrap / walk_state unit tests — load, call, raise
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include "ascript_fwrap.h"
#include "ascript_lex.h"
#include "ascript_parse.h"
#include "ascript_modmng.h"
#include "ascript_resmng.h"
#include "ascript_walk.h"
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

static varvec parse_src(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    return p.parse();
}

struct test_harness {
    res_mng mgr;
    mod_mng mod;
    engine_config cfg;
    walker w;

    test_harness() : w(cfg) {
        mod.m_res = &mgr;
        w.mgr = &mod;
        w.reset();
        w.m_root_fly.m_path = ".";
    }
};

TEST(gt_ascript_context, Load_Variable) {
    test_harness h;
    h.w.state.push_frame();
    auto& sf = h.w.state.frames.back();
    sf.store("x", variant(42));
    variant* p = h.w.state.var_ptr("x");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->to<int_64>(), 42);
}

TEST(gt_ascript_context, Load_ScopedVariable) {
    test_harness h;
    auto& sf1 = h.w.state.push_frame();
    sf1.store("outer", variant(1));

    auto& sf2 = h.w.state.push_frame();
    sf2.store("inner", variant(2));

    EXPECT_NE(h.w.state.var_ptr("inner"), nullptr);
    EXPECT_EQ(h.w.state.var_ptr("inner")->to<int_64>(), 2);
    EXPECT_NE(h.w.state.var_ptr("outer"), nullptr);
    EXPECT_EQ(h.w.state.var_ptr("outer")->to<int_64>(), 1);

    h.w.state.pop_frame();
    EXPECT_EQ(h.w.state.var_ptr("inner"), nullptr);
    EXPECT_NE(h.w.state.var_ptr("outer"), nullptr);
}

TEST(gt_ascript_context, Load_NotFound) {
    test_harness h;
    h.w.state.push_frame();
    EXPECT_EQ(h.w.state.var_ptr("no_such_var"), nullptr);
}

TEST(gt_ascript_context, Load_NoFrame) {
    test_harness h;
    EXPECT_EQ(h.w.state.var_ptr("x"), nullptr);
}

TEST(gt_ascript_context, Call_UserFunction) {
    test_harness h;
    h.w.state.push_frame();

    varvec def_tree;
    def_tree.push_back(variant(OPTYPE(O_DEF)));
    def_tree.push_back(variant("add2"));
    varvec params;
    params.push_back(variant("a"));
    params.push_back(variant("b"));
    def_tree.push_back(variant(std::move(params)));
    def_tree.push_back(variant(varvec{}));
    varvec body;
    body.push_back(variant(OPTYPE(O_BLOCK)));
    varvec ret_stmt;
    ret_stmt.push_back(variant(OPTYPE(O_RETURN)));
    varvec add_expr;
    add_expr.push_back(variant(OPTYPE(O_ADD)));
    varvec load_a;
    load_a.push_back(variant(OPTYPE(O_LOAD)));
    load_a.push_back(variant("a"));
    varvec load_b;
    load_b.push_back(variant(OPTYPE(O_LOAD)));
    load_b.push_back(variant("b"));
    add_expr.push_back(variant(std::move(load_a)));
    add_expr.push_back(variant(std::move(load_b)));
    ret_stmt.push_back(variant(std::move(add_expr)));
    body.push_back(variant(std::move(ret_stmt)));
    def_tree.push_back(variant(std::move(body)));
    h.w.walk_tree(def_tree);

    variant ret_slot;
    fwrap_impl fw(std::vector<variant*>(), &ret_slot, &h.w.m_root.m_store, &h.w);
    variant result = fw.call("add2", varvec{variant(static_cast<int_64>(3)), variant(static_cast<int_64>(4))});
    EXPECT_EQ(result.to<int_64>(), 7);
}

TEST(gt_ascript_context, Call_UserFunction_NoArgs) {
    test_harness h;
    h.w.state.push_frame();

    varvec def_tree;
    def_tree.push_back(variant(OPTYPE(O_DEF)));
    def_tree.push_back(variant("the_answer"));
    def_tree.push_back(variant(varvec{}));
    def_tree.push_back(variant(varvec{}));
    varvec body;
    body.push_back(variant(OPTYPE(O_BLOCK)));
    varvec ret_stmt;
    ret_stmt.push_back(variant(OPTYPE(O_RETURN)));
    varvec ret_val;
    ret_val.push_back(variant(static_cast<int_64>(42)));
    ret_stmt.push_back(variant(std::move(ret_val)));
    body.push_back(variant(std::move(ret_stmt)));
    def_tree.push_back(variant(std::move(body)));
    h.w.walk_tree(def_tree);

    variant ret_slot;
    fwrap_impl fw(std::vector<variant*>(), &ret_slot, &h.w.m_root.m_store, &h.w);
    variant result = fw.call("the_answer", varvec());
    EXPECT_EQ(result.to<int_64>(), 42);
}

TEST(gt_ascript_context, Call_Undefined) {
    test_harness h;
    h.w.state.push_frame();

    variant ret_slot;
    fwrap_impl fw(std::vector<variant*>(), &ret_slot, &h.w.m_root.m_store, &h.w);
    EXPECT_THROW(fw.call(variant(std::string("no_such_func")), varvec()), script_exception);
}

TEST(gt_ascript_context, Walk_Builtin_Add) {
    test_harness h;

    varvec tree;
    tree.push_back(variant(OPTYPE(O_ADD)));
    tree.push_back(variant(static_cast<int_64>(3)));
    tree.push_back(variant(static_cast<int_64>(4)));
    variant result = h.w.walk_tree(tree);
    EXPECT_EQ(result.to<int_64>(), 7);
}

TEST(gt_ascript_context, Call_ScriptDef) {
    test_harness h;
    h.w.state.push_frame();

    varvec ast = parse_src("def answer() { return 42; } answer();");
    variant result = h.w.walk_forest(ast);
    EXPECT_EQ(result.to<int_64>(), 42);
}

TEST(gt_ascript_context, Raise_ThrowsScriptException) {
    test_harness h;
    variant ret_slot;
    fwrap_impl fw(std::vector<variant*>(), &ret_slot, &h.w.m_root.m_store, &h.w);
    bool caught = false;
    try {
        fw.raise(variant(std::string("test error")));
    } catch (const script_exception& e) {
        caught = true;
        EXPECT_EQ(e.info, "test error");
    }
    EXPECT_TRUE(caught);
}

TEST(gt_ascript_context, Raise_VariantValue) {

    test_harness h;
    variant ret_slot;
    fwrap_impl fw(std::vector<variant*>(), &ret_slot, &h.w.m_root.m_store, &h.w);
    try {
        fw.raise(variant(static_cast<int_64>(404)));
    } catch (const script_exception& e) {
        EXPECT_EQ(e.info, "404");
    }
}

TEST(gt_ascript_context, Args_ReadWriteView) {

    test_harness h;
    variant a1(static_cast<int_64>(1));
    variant a2(static_cast<int_64>(2));
    std::vector<variant*> ptrs = {&a1, &a2};
    variant ret_slot;
    fwrap_impl fw(std::move(ptrs), &ret_slot, &h.w.m_root.m_store, &h.w);

    EXPECT_EQ(fw.size(), 2u);
    EXPECT_EQ(fw[0].to<int_64>(), 1);
    EXPECT_EQ(fw[1].to<int_64>(), 2);

    fw[0] = variant(static_cast<int_64>(10));
    EXPECT_EQ(a1.to<int_64>(), 10);
    EXPECT_EQ(fw[0].to<int_64>(), 10);
    EXPECT_EQ(a2.to<int_64>(), 2);
}

TEST(gt_ascript_context, Args_IndexOutOfRange) {

    test_harness h;
    variant a1(static_cast<int_64>(1));
    std::vector<variant*> ptrs = {&a1};
    variant ret_slot;
    fwrap_impl fw(std::move(ptrs), &ret_slot, &h.w.m_root.m_store, &h.w);

    EXPECT_THROW(fw[1], script_exception);
    EXPECT_THROW(fw[5], script_exception);
    try {
        fw[1];
        FAIL() << "expected IndexError";
    } catch (const script_exception& e) {
        EXPECT_EQ(e.type, error_type::IndexError);
    }
}

TEST(gt_ascript_context, Args_ReadBeforeCallback) {

    test_harness h;
    h.w.state.push_frame();

    varvec def_tree;
    def_tree.push_back(variant(OPTYPE(O_DEF)));
    def_tree.push_back(variant("the_answer"));
    def_tree.push_back(variant(varvec{}));
    def_tree.push_back(variant(varvec{}));
    varvec body;
    body.push_back(variant(OPTYPE(O_BLOCK)));
    varvec ret_stmt;
    ret_stmt.push_back(variant(OPTYPE(O_RETURN)));
    varvec ret_val;
    ret_val.push_back(variant(static_cast<int_64>(42)));
    ret_stmt.push_back(variant(std::move(ret_val)));
    body.push_back(variant(std::move(ret_stmt)));
    def_tree.push_back(variant(std::move(body)));
    h.w.walk_tree(def_tree);

    variant a1(static_cast<int_64>(7));
    std::vector<variant*> ptrs = {&a1};
    variant ret_slot;
    fwrap_impl fw(std::move(ptrs), &ret_slot, &h.w.m_root.m_store, &h.w);

    EXPECT_EQ(fw[0].to<int_64>(), 7);
    variant result = fw.call("the_answer", varvec());
    EXPECT_EQ(result.to<int_64>(), 42);
}

TEST(gt_ascript_context, Args_EndValue_S1_LaterArgAssigns) {

    test_harness h;
    h.w.state.push_frame();
    varvec ast = parse_src(
        "var x; x = 1;\n"
        "def echo_first(a, b) { return a; }\n"
        "echo_first(x, x = 5);");
    variant result = h.w.walk_forest(ast);
    EXPECT_EQ(result.to<int_64>(), 5);
}

TEST(gt_ascript_context, Args_EndValue_S2_LaterArgMutates) {

    test_harness h;
    h.w.state.push_frame();
    varvec ast = parse_src(
        "var count; count = 1;\n"
        "def next_count() { count = count + 1; return count; }\n"
        "def echo_first(a, b) { return a; }\n"
        "echo_first(count, next_count());");
    variant result = h.w.walk_forest(ast);
    EXPECT_EQ(result.to<int_64>(), 2);
}

TEST(gt_ascript_context, Args_EndValue_S4_ErrorOrdering) {

    test_harness h;
    h.w.state.push_frame();
    varvec ast = parse_src(
        "var ran; ran = 0;\n"
        "def g() { ran = 1; return 0; }\n"
        "def echo_first(a, b) { return a; }\n"
        "echo_first(no_such_var, g());");
    EXPECT_THROW(h.w.walk_forest(ast), script_exception);
    variant* p = h.w.state.var_ptr("ran");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->to<int_64>(), 0);
}
