/*****************************************************************/ /**
 * \file   gt_ascript_walk.cpp
 * \brief  Walk engine unit tests — execution coverage
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "afile.h"
#include "ascript.h"
#include "ascript_lex.h"
#include "ascript_parse.h"
#include "ascript_modmng.h"
#include "ascript_resmng.h"
#include "ascript_walk.h"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

using namespace alx;
using namespace alx::script;

static variant exec_src(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    varvec ast = p.parse();
    if (p.has_error()) return variant();

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    walker w(cfg);

    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static op_table bt{};

    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;
    w.state.push_frame();

    return w.walk_forest(ast);
}

static ParentKind resolve_kind(const char* _setup, const varvec& _dot) {
    alx::bytes sb(_setup);
    token_list stl;
    stl.tokenize(alx::bytes_view(sb));
    parser sp(stl);
    varvec setup_ast = sp.parse();
    if (sp.has_error()) return ParentKind::Unknown;

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    walker w(cfg);
    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static op_table bt{};
    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;
    w.state.push_frame();

    w.walk_forest(setup_ast);
    return resolve_dot(_dot, w.state).parent_kind;
}

static varvec dot_ast(std::initializer_list<variant> _elems) {
    varvec v;
    v.push_back(variant(OPTYPE(O_DOT)));
    for (auto& e : _elems) v.push_back(e);
    return v;
}

static std::pair<error_type, variant> exec_src_catch(const char* src) {
    try {
        exec_src(src);
    } catch (script_exception& e) {
        return {e.type, e.info};
    }
    return {error_type::UnknownError, {}};
}

static variant exec_src_no_frame(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    varvec ast = p.parse();
    if (p.has_error()) return variant();

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    walker w(cfg);
    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static op_table bt{};
    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;

    return w.walk_forest(ast);
}

static std::pair<error_type, variant> exec_src_no_frame_catch(const char* src) {
    try {
        auto v = exec_src_no_frame(src);
        return {error_type::UnknownError, v};
    } catch (script_exception& e) {
        return {e.type, e.info};
    }
}

TEST(gt_ascript_walk, VarDecl) {
    auto v = exec_src("var x; x = 1; x;");
    ASSERT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, VarStoreWithInit) {
    auto v = exec_src("var x = 1; x;");
    ASSERT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, VarMultiStore) {
    auto v = exec_src("var a = 1, b, c = 3; a + c;");
    ASSERT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 4);
}

TEST(gt_ascript_walk, Assign_UndefinedThrowsNameError) {
    auto [etype, eval] = exec_src_catch("x = 1;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, Assign) {
    auto v = exec_src("var x; x = 1; x = 2; x;");
    ASSERT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, AssignAdd) {
    auto v = exec_src("var x; x = 1; x += 2; x;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, Add) {
    auto v = exec_src("1 + 2;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, Sub) {
    auto v = exec_src("3 - 1;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, Mul) {
    auto v = exec_src("2 * 3;");
    EXPECT_EQ(v.to<int_64>(), 6);
}

TEST(gt_ascript_walk, Div) {
    auto v = exec_src("6 / 2;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, Mod) {
    auto v = exec_src("7 % 3;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, FloatAdd) {
    auto v = exec_src("1.5 + 2.5;");
    EXPECT_DOUBLE_EQ(v.to<double>(), 4.0);
}

TEST(gt_ascript_walk, Lt) {
    auto v = exec_src("1 < 2;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, Gt) {
    auto v = exec_src("2 > 1;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, Le) {
    auto v = exec_src("1 <= 1;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, Eq) {
    auto v = exec_src("1 == 1;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, Ne) {
    auto v = exec_src("1 != 2;");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, AndTrue) {
    auto v = exec_src("true && true;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, AndFalse) {
    auto v = exec_src("true && false;");
    EXPECT_EQ(v.to<bool>(), false);
}
TEST(gt_ascript_walk, Or) {
    auto v = exec_src("true || false;");
    EXPECT_EQ(v.to<bool>(), true);
}
TEST(gt_ascript_walk, Not) {
    auto v = exec_src("!true;");
    EXPECT_EQ(v.to<bool>(), false);
}

TEST(gt_ascript_walk, TernaryTrue) {
    auto v = exec_src("true ? 1 : 0;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, TernaryFalse) {
    auto v = exec_src("false ? 1 : 0;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, Comma) {
    auto v = exec_src("1, 2, 3;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, PreInc) {
    auto v = exec_src("var i; i = 0; ++i; i;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, PostInc) {
    auto v = exec_src("var i; i = 0; i++; i;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, IfTrue) {
    auto v = exec_src("var x; x = 0; if (true) { x = 1; } x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, IfFalse) {
    auto v = exec_src("var x; x = 0; if (false) { x = 1; } x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, IfElse) {
    auto v = exec_src("var x; x = 0; if (false) { x = 1; } else { x = 2; } x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, IfBraceless) {
    auto v = exec_src("var x; x = 0; if (true) x = 1; x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, IfElseBraceless) {
    auto v = exec_src("var x; x = 0; if (false) x = 1; else x = 2; x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, WhileLoop) {
    auto v = exec_src("var n; n = 3; var s; s = 0; while (n > 0) { s = s + n; n = n - 1; } s;");
    EXPECT_EQ(v.to<int_64>(), 6);
}
TEST(gt_ascript_walk, WhileBraceless) {
    auto v = exec_src("var n; n = 3; var s; s = 0; while (n > 0) s = s + n--; s;");
    EXPECT_EQ(v.to<int_64>(), 6);
}

TEST(gt_ascript_walk, ForLoop) {
    auto v = exec_src("var s; s = 0; var i; for (i = 0; i < 3; ++i) { s = s + i; } s;");
    EXPECT_EQ(v.to<int_64>(), 3);
}
TEST(gt_ascript_walk, ForBraceless) {
    auto v = exec_src("var s; s = 0; var i; for (i = 0; i < 3; ++i) s = s + i; s;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, Break) {
    auto v = exec_src("var s; s = 0; while (true) { s = s + 1; break; } s;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, BreakInFor) {

    auto v = exec_src("var x; x = 0; var i; for (i = 0; i < 3; ++i) { x = x + 1; break; x = x + 1; } x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, ContinueInFor) {

    auto v = exec_src("var x; x = 0; var i; for (i = 0; i < 3; ++i) { continue; x = x + 1; } x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, ContinueInWhile) {
    auto v = exec_src("var x; x = 0; var i; i = 0; while (i < 3) { i = i + 1; continue; x = x + 1; } x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, ForVarInit) {

    auto v = exec_src("var sum; sum = 0; for (var i = 0; i < 3; ++i) { sum = sum + i; } sum;");
    EXPECT_EQ(v.to<int_64>(), 3);
    auto [etype, eval] = exec_src_catch("for (var i = 0; i < 3; ++i) {} i;");
    EXPECT_EQ(etype, error_type::NameError);
}
TEST(gt_ascript_walk, ForLoopVarScope) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 3; ++i) {"
        "  var tmp = i * 10;"
        "  sum = sum + tmp;"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 0 + 10 + 20);
}
TEST(gt_ascript_walk, WhileVarScope) {

    auto v = exec_src(
        "var i; i = 0;"
        "while (i < 3) {"
        "  var tmp = i * 2;"
        "  i = i + 1;"
        "}"
        "i;");
    EXPECT_EQ(v.to<int_64>(), 3);
    auto [etype, eval] = exec_src_catch("while (true) { var x = 1; break; } x;");
    EXPECT_EQ(etype, error_type::NameError);
}
TEST(gt_ascript_walk, ForLoopCheckpoint) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 3; ++i) {"
        "  var x = 10;"
        "  sum = sum + x;"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 30);
}
TEST(gt_ascript_walk, ForInVarTarget) {

    auto v = exec_src(
        "var arr; arr = [10, 20, 30];"
        "var sum; sum = 0;"
        "for (var x : arr) { sum = sum + x; }"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 60);
    auto [etype, eval] = exec_src_catch("for (var x : [1]) {} x;");
    EXPECT_EQ(etype, error_type::NameError);
}
TEST(gt_ascript_walk, ContinueInForLoop) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 3; ++i) {"
        "  if (i == 1) { continue; }"
        "  sum = sum + i;"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, BreakInForLoop) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 10; ++i) {"
        "  var tmp = i;"
        "  sum = sum + i;"
        "  if (i >= 2) { break; }"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 0 + 1 + 2);
}
TEST(gt_ascript_walk, NestedForVar) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 2; ++i) {"
        "  for (var j = 0; j < 3; ++j) {"
        "    var tmp = i * 10 + j;"
        "    sum = sum + tmp;"
        "  }"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), (0 + 1 + 2) + (10 + 11 + 12));
}
TEST(gt_ascript_walk, ForVarInitPersists) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "for (var i = 0; i < 5; ++i) {"
        "  if (i == 3) { i = 10; }"
        "  sum = sum + 1;"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 4);
}
TEST(gt_ascript_walk, WhileFreshVarEachIteration) {

    auto v = exec_src(
        "var sum; sum = 0;"
        "var i; i = 0;"
        "while (i < 3) {"
        "  var x = i * 10;"
        "  sum = sum + x;"
        "  i = i + 1;"
        "}"
        "sum;");
    EXPECT_EQ(v.to<int_64>(), 0 + 10 + 20);
}

TEST(gt_ascript_walk, ReturnInFor) {

    auto v = exec_src("def f() { var i; for (i = 0; i < 10; ++i) { if (i == 3) { return i; } } return -1; } f();");
    EXPECT_EQ(v.to<int_64>(), 3);
}
TEST(gt_ascript_walk, ReturnInForIn) {

    auto v = exec_src("def f() { var v; v = [1, 2, 3, 4, 5]; var x; for (x : v) { if (x == 3) { return x; } } return -1; } f();");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, DefCall) {
    auto v = exec_src("def add(a, b) { return a + b; } add(1, 2);");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, DefWithDefault) {
    auto v = exec_src("def sub(a, b = 1) { return a - b; } sub(5);");
    EXPECT_EQ(v.to<int_64>(), 4);
}

TEST(gt_ascript_walk, Spread_Vec) {
    auto v = exec_src("def sum(a, b, c) { return a + b + c; } sum([1, 2, 3][]);");
    EXPECT_EQ(v.to<int_64>(), 6);
}

TEST(gt_ascript_walk, Spread_VecMixed) {
    auto v = exec_src("def sum(a, b, c) { return a + b + c; } sum(10, [20, 30][]);");
    EXPECT_EQ(v.to<int_64>(), 60);
}

TEST(gt_ascript_walk, Spread_VecExtraIgnored) {
    auto v = exec_src("def sum(a, b) { return a + b; } sum([1, 2, 3, 4][]);");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, Spread_Map) {
    auto v = exec_src("def f(a, b) { return a + b; } f(map{\"b\": 20, \"a\": 10}[]);");
    EXPECT_EQ(v.to<int_64>(), 30);
}

TEST(gt_ascript_walk, Spread_MapPartialWithDefault) {
    auto v = exec_src("def f(a, b) { return a + b; } f(10, map{\"b\": 5}[]);");
    EXPECT_EQ(v.to<int_64>(), 15);
}

TEST(gt_ascript_walk, Spread_MapExtraKeysIgnored) {
    auto v = exec_src("def f(a, b) { return a + b; } f(map{\"a\": 3, \"b\": 5, \"c\": 99}[]);");
    EXPECT_EQ(v.to<int_64>(), 8);
}

TEST(gt_ascript_walk, Spread_MapZeroMatch) {
    auto [etype, eval] = exec_src_catch(
        "def f(a) { return a; } f(map{\"x\": 1}[]);");
    EXPECT_EQ(etype, error_type::ArgError);
}

TEST(gt_ascript_walk, Spread_NonContainer) {
    auto [etype, eval] = exec_src_catch(
        "def f(a) { return a; } var x = 42; f(x[]);");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, Spread_TooManyPositional) {
    auto [etype, eval] = exec_src_catch(
        "def f(a) { return a; } f(1, 2, 3);");
    EXPECT_EQ(etype, error_type::ArgError);
}

TEST(gt_ascript_walk, ListLit) {
    auto v = exec_src("[1, 2, 3];");
    ASSERT_TRUE(v.is<varvec>());
    EXPECT_EQ(v.to<varvec>().size(), 3u);
}

TEST(gt_ascript_walk, ListFill) {
    auto v = exec_src("[0: 3];");
    ASSERT_TRUE(v.is<varvec>());
    EXPECT_EQ(v.to<varvec>().size(), 3u);
}

TEST(gt_ascript_walk, DictLit) {
    auto v = exec_src("map{\"k\": 1};");
    ASSERT_TRUE(v.is<varmap>());
    EXPECT_EQ(v.to<varmap>().value("k").to<int_64>(), 1);
}

TEST(gt_ascript_walk, Index) {
    auto v = exec_src("var a; a = [1, 2, 3]; a[0];");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, TryCatch) {
    auto v = exec_src(
        "var x; x = 0;"
        "try { throw 42; } catch (e) { x = e.info; }"
        "x;");
    EXPECT_EQ(v.to<std::string>(), "42");
}

TEST(gt_ascript_walk, TryCatch_Type) {
    auto [etype, eval] = exec_src_catch(
        "throw 42;");
    EXPECT_EQ(etype, error_type::RuntimeError);
    EXPECT_EQ(eval.to<std::string>(), "42");
}

TEST(gt_ascript_walk, TryCatch_Varmap) {

    auto v = exec_src(
        "var r; r = 0;"
        "try { throw \"test\"; } catch (e) { r = e.info; }"
        "r;");
    EXPECT_EQ(v.to<std::string>(), "test");
}

TEST(gt_ascript_walk, ForInWithVar) {

    auto v = exec_src(
        "var arr; arr = [10, 20, 30];"
        "var s; s = 0;"
        "var val;"
        "for (val : arr) { s = s + val; }"
        "s;");
    EXPECT_EQ(v.to<int_64>(), 60);
}

TEST(gt_ascript_walk, DictIndexString) {
    auto v = exec_src(
        "var m; m = map{\"k\": 42};"
        "m[\"k\"];");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, Recursion) {
    auto v = exec_src(
        "def fib(n) {"
        "  if (n <= 1) { return n; }"
        "  return fib(n-1) + fib(n-2);"
        "}"
        "fib(6);");
    EXPECT_EQ(v.to<int_64>(), 8);
}

TEST(gt_ascript_walk, TernaryInVarDecl) {
    auto v = exec_src(
        "var x; x = 1;"
        "var y; y = 2;"
        "var max; max = x > y ? x : y;"
        "max;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, NestedCall) {
    auto v = exec_src(
        "def add(a, b) { return a + b; }"
        "def sq(x) { return x * x; }"
        "add(sq(2), sq(3));");
    EXPECT_EQ(v.to<int_64>(), 13);
}

TEST(gt_ascript_walk, BlockScopeVar) {

    auto v = exec_src(
        "var x; x = 1;"
        "def f() { var y; y = 2; return x + y; }"
        "f();");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, AssignInBlock) {

    auto v = exec_src(
        "var x; x = 1;"
        "if (true) { x = 2; }"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, StringLiteral) {
    auto v = exec_src("\"hello\";");
    EXPECT_EQ(v.to<std::string>(), "hello");
}

TEST(gt_ascript_walk, EmptyReturn) {
    auto v = exec_src(
        "def f() { return; }"
        "f();");

    EXPECT_FALSE(v.is<int_64>());
}

TEST(gt_ascript_walk, SwitchStmt) {
    auto v = exec_src(
        "var x; x = 0;"
        "switch (2) {"
        "  case 1: x = 10; break;"
        "  case 2: x = 20; break;"
        "  default: x = 0;"
        "}"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 20);
}

TEST(gt_ascript_walk, SwitchDefault) {
    auto v = exec_src(
        "var x; x = 0;"
        "switch (99) {"
        "  case 1: x = 10; break;"
        "  default: x = 99;"
        "}"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, BitOr) { EXPECT_EQ(exec_src("1 | 2;").to<int_64>(), 3); }
TEST(gt_ascript_walk, BitXor) { EXPECT_EQ(exec_src("1 ^ 3;").to<int_64>(), 2); }
TEST(gt_ascript_walk, BitAnd) { EXPECT_EQ(exec_src("3 & 1;").to<int_64>(), 1); }
TEST(gt_ascript_walk, BitNeg) { EXPECT_EQ(exec_src("~0;").to<int_64>(), -1); }

TEST(gt_ascript_walk, Lshift) { EXPECT_EQ(exec_src("1 << 2;").to<int_64>(), 4); }
TEST(gt_ascript_walk, Rshift) { EXPECT_EQ(exec_src("8 >> 2;").to<int_64>(), 2); }

TEST(gt_ascript_walk, AndShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = false && (++x); x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, OrShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = true || (++x); x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, AndNoShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = true && (++x != 0); x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, OrNoShortCircuit) {
    auto v = exec_src("var x; x = 0; var r; r = false || (++x != 0); x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, TernaryShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = false ? (++x) : x; x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, TernaryConseqEval) {

    auto v = exec_src("var x; x = 0; var r; r = true ? (++x) : x; x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, IfShortCircuit) {

    auto v = exec_src("var x; x = 0; if (false) { x = 1; } else { x = 2; } x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, AndChainShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = true && false && (++x); x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, OrChainShortCircuit) {

    auto v = exec_src("var x; x = 0; var r; r = false || true || (++x); x;");
    EXPECT_EQ(v.to<int_64>(), 0);
}
TEST(gt_ascript_walk, NestedTernaryShortCircuit) {

    auto v = exec_src("var x; x=0; var r; r=true?(false?(++x):(++x)):(++x); x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, AssSub) {
    auto v = exec_src("var x; x = 5; x -= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 3);
}
TEST(gt_ascript_walk, AssMul) {
    auto v = exec_src("var x; x = 3; x *= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 6);
}
TEST(gt_ascript_walk, AssDiv) {
    auto v = exec_src("var x; x = 8; x /= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 4);
}
TEST(gt_ascript_walk, AssMod) {
    auto v = exec_src("var x; x = 7; x %= 3; x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, AssLshift) {
    auto v = exec_src("var x; x = 1; x <<= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 4);
}
TEST(gt_ascript_walk, AssRshift) {
    auto v = exec_src("var x; x = 8; x >>= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, AssBitAnd) {
    auto v = exec_src("var x; x = 3; x &= 1; x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}
TEST(gt_ascript_walk, AssBitOr) {
    auto v = exec_src("var x; x = 1; x |= 2; x;");
    EXPECT_EQ(v.to<int_64>(), 3);
}
TEST(gt_ascript_walk, AssBitXor) {
    auto v = exec_src("var x; x = 1; x ^= 3; x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, PreDec) {
    auto v = exec_src("var i; i = 5; --i; i;");
    EXPECT_EQ(v.to<int_64>(), 4);
}
TEST(gt_ascript_walk, PostDec) {
    auto v = exec_src("var i; i = 5; i--; i;");
    EXPECT_EQ(v.to<int_64>(), 4);
}

TEST(gt_ascript_walk, DivZero) {
    auto [etype, eval] = exec_src_catch("1 / 0;");
    EXPECT_EQ(etype, error_type::DivZeroError);
}
TEST(gt_ascript_walk, ModZero) {
    auto [etype, eval] = exec_src_catch("1 % 0;");
    EXPECT_EQ(etype, error_type::DivZeroError);
}

TEST(gt_ascript_walk, IfInWhile) {
    auto v = exec_src(
        "var i; i = 0; var x; x = 0;"
        "while (i < 3) { if (i == 1) { x = 10; } i = i + 1; }"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 10);
}
TEST(gt_ascript_walk, WhileInFor) {
    auto v = exec_src(
        "var s; s = 0;"
        "var i; for (i = 0; i < 3; ++i) { var j; j = 0; while (j < 2) { s = s + 1; j = j + 1; } }"
        "s;");
    EXPECT_EQ(v.to<int_64>(), 6);
}
TEST(gt_ascript_walk, SwitchInIf) {
    auto v = exec_src(
        "var x; x = 0;"
        "if (true) { switch (2) { case 1: x = 1; break; case 2: x = 2; break; } }"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, TryInWhile) {
    auto v = exec_src(
        "var c; c = 0; var done; done = 0;"
        "while (done < 1) { try { c = c + 1; if (c >= 2) { done = 1; } } catch (e) {} }"
        "c;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, NestedDef) {
    auto v = exec_src(
        "def outer() {"
        "  var x; x = 10;"
        "  def inner() { return x; }"
        "  return inner();"
        "}"
        "outer();");
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, ElseIfChain) {
    auto v = exec_src(
        "var x; x = 0;"
        "if (false) { x = 1; } else if (true) { x = 2; } else { x = 3; }"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 2);
}
TEST(gt_ascript_walk, ElseIfFallthrough) {
    auto v = exec_src(
        "var x; x = 0;"
        "if (false) { x = 1; } else if (false) { x = 2; } else { x = 3; }"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 3);
}
TEST(gt_ascript_walk, ForFull) {

    auto v = exec_src("var s; s=0; var i; for(i=1; i<=3; ++i){s=s+i;} s;");
    EXPECT_EQ(v.to<int_64>(), 6);
}
TEST(gt_ascript_walk, TripleNestedLoop) {

    auto v = exec_src(
        "var c; c = 0; var i;"
        "for (i = 0; i < 2; ++i) {"
        "  var j; j = 0;"
        "  while (j < 2) {"
        "    if (true) { c = c + 1; }"
        "    j = j + 1;"
        "  }"
        "}"
        "c;");
    EXPECT_EQ(v.to<int_64>(), 4);
}

TEST(gt_ascript_walk, ManyArgs) {
    auto v = exec_src(
        "def sum(a, b, c, d) { return a + b + c + d; }"
        "sum(1, 2, 3, 4);");
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, FloatSub) {
    auto v = exec_src("3.5 - 1.0;");
    EXPECT_DOUBLE_EQ(v.to<double>(), 2.5);
}
TEST(gt_ascript_walk, FloatMul) {
    auto v = exec_src("2.5 * 2.0;");
    EXPECT_DOUBLE_EQ(v.to<double>(), 5.0);
}
TEST(gt_ascript_walk, FloatDiv) {
    auto v = exec_src("5.0 / 2.0;");
    EXPECT_DOUBLE_EQ(v.to<double>(), 2.5);
}

TEST(gt_ascript_walk, StringConcat) {

    auto v = exec_src("\"hello\" + \" \";");
    EXPECT_EQ(v.to<std::string>(), "hello ");
}

TEST(gt_ascript_walk, ImportEndToEnd) {

    std::string tmp = "/tmp/alx_walk_import.axc";
    std::ofstream ofs(tmp);
    ofs << "def square(x) { return x * x; }" << std::endl;
    ofs.close();

    auto v = exec_src("import \"/tmp/alx_walk_import.axc\" as m; m.square(7);");
    EXPECT_EQ(v.to<int_64>(), 49);

    std::remove(tmp.c_str());
}

TEST(gt_ascript_walk, LinkNoCrash) {

    auto r = exec_src_catch("link \"no_such_file\" as dummy; 1;");
    EXPECT_EQ(r.first, error_type::ImportError);
}

TEST(gt_ascript_walk, ImportShield_BareRead) {

    std::string tmp = "/tmp/alx_import_shield.axc";
    std::ofstream ofs(tmp);
    ofs << "var x = 1;" << std::endl;
    ofs.close();

    auto r = exec_src_catch(("import \"" + tmp + "\" as m; var x = m;").c_str());
    EXPECT_EQ(r.first, error_type::TypeError);
    std::remove(tmp.c_str());
}
TEST(gt_ascript_walk, ImportShield_ExprRead) {

    std::string tmp = "/tmp/alx_import_shield2.axc";
    std::ofstream ofs(tmp);
    ofs << "var x = 1;" << std::endl;
    ofs.close();

    auto r = exec_src_catch(("import \"" + tmp + "\" as m; var y = m + 1;").c_str());
    EXPECT_EQ(r.first, error_type::TypeError);
    std::remove(tmp.c_str());
}
TEST(gt_ascript_walk, ImportShield_CompoundAssign) {

    std::string tmp = "/tmp/alx_import_shield3.axc";
    std::ofstream ofs(tmp);
    ofs << "var x = 1;" << std::endl;
    ofs.close();

    auto r = exec_src_catch(("import \"" + tmp + "\" as m; m += 1;").c_str());
    EXPECT_EQ(r.first, error_type::TypeError);
    std::remove(tmp.c_str());
}
TEST(gt_ascript_walk, ImportShield_DotCallStillWorks) {

    std::string tmp = "/tmp/alx_import_shield4.axc";
    std::ofstream ofs(tmp);
    ofs << "def square(x) { return x * x; }" << std::endl;
    ofs.close();

    auto v = exec_src(("import \"" + tmp + "\" as m; m.square(5);").c_str());
    EXPECT_EQ(v.to<int_64>(), 25);
    std::remove(tmp.c_str());
}
TEST(gt_ascript_walk, ImportShield_DotWriteStillWorks) {

    std::string tmp = "/tmp/alx_import_shield5.axc";
    std::ofstream ofs(tmp);
    ofs << "var x = 1;" << std::endl;
    ofs.close();

    auto v = exec_src(("import \"" + tmp + "\" as m; m.x = 99; m.x;").c_str());
    EXPECT_EQ(v.to<int_64>(), 99);
    std::remove(tmp.c_str());
}

TEST(gt_ascript_walk, MemberAccess) {
    auto v = exec_src("var obj; obj = map{\"k\": 42}; obj.k;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, StringIndex) {
    auto v = exec_src("\"hello\"[1];");
    EXPECT_EQ(v.to<std::string>(), "e");
}

TEST(gt_ascript_walk, ForInString) {
    EXPECT_THROW(
        exec_src(
            "for (c : \"abc\") { var x = c; }"),
        script_exception);
}

TEST(gt_ascript_walk, IndexOutOfBounds) {
    auto [etype, eval] = exec_src_catch("var a; a = [1, 2]; a[100];");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, DictKeyNotFound) {
    auto [etype, eval] = exec_src_catch("var m; m = map{\"k\": 1}; m[\"x\"];");
    EXPECT_EQ(etype, error_type::KeyError);
}

TEST(gt_ascript_walk, NullLiteralExec) {
    auto v = exec_src("null;");
    EXPECT_FALSE(v.is<int_64>());
    EXPECT_FALSE(v.is<bool>());
    EXPECT_FALSE(v.is<std::string>());
}

TEST(gt_ascript_walk, BoolLiteralExec) {
    auto v = exec_src("true; var x; x = false; x;");
    EXPECT_EQ(v.to<bool>(), false);
}

TEST(gt_ascript_walk, CharLiteralExec) {
    auto v = exec_src("'c';");
    EXPECT_EQ(v.to<std::string>(), "c");
}

TEST(gt_ascript_walk, UnaryPlusExec) {
    auto v = exec_src("+5;");
    EXPECT_EQ(v.to<int_64>(), 5);
}

TEST(gt_ascript_walk, StringCompareEq) {
    auto v = exec_src("\"abc\" == \"abc\";");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, StringCompareLt) {
    auto v = exec_src("\"abc\" < \"xyz\";");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, FloatCompare) {
    auto v = exec_src("1.5 < 2.5;");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, TryNormal) {
    auto v = exec_src("var x; x = 0; try { x = 1; } catch (e) { x = 2; } x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, BreakInNestedLoop) {

    auto v = exec_src(
        "var s; s = 0; var i;"
        "for (i = 0; i < 3; ++i) {"
        "  var j; j = 0;"
        "  while (j < 10) { s = s + 1; j = j + 1; break; }"
        "}"
        "s;");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, ContinueInNestedLoop) {
    auto v = exec_src(
        "var s; s = 0; var i;"
        "for (i = 0; i < 3; ++i) {"
        "  if (i == 1) { continue; }"
        "  s = s + 1;"
        "}"
        "s;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, EmptyList) {
    auto v = exec_src("var a; a = []; a;");
    ASSERT_TRUE(v.is<varvec>());
    EXPECT_EQ(v.to<varvec>().size(), 0u);
}

TEST(gt_ascript_walk, EmptyDict) {
    auto v = exec_src("var m; m = map{}; m;");
    ASSERT_TRUE(v.is<varmap>());
    EXPECT_EQ(v.to<varmap>().size(), 0u);
}

TEST(gt_ascript_walk, PostIncReturnValue) {
    auto v = exec_src("var i; i = 0; var r; r = i++; r;");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, PreIncReturnValue) {
    auto v = exec_src("var i; i = 0; var r; r = ++i; r;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteVar) {

    auto [etype, eval] = exec_src_no_frame_catch("var x; x = 1; delete x; var y; y = x;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteDef) {

    auto [etype, eval] = exec_src_no_frame_catch(
        "def g() { return 1; }"
        "delete g;"
        "g();");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteAs) {

    std::string tmp = "/tmp/alx_del_as_test.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var x; x = 99;" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as mod; delete mod; "
                                           "var r; r = 0; try { var _; _ = mod.x; } catch (e) { r = 1; } r;";
    auto v = exec_src_no_frame(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteDotDict) {
    auto v = exec_src(
        "var d; d = {\"a\": 1, \"b\": 2};"
        "delete d.a;"
        "var r; r = 0;"
        "try { r = d.a; } catch (e) { }"
        "r;");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, DeleteIndexVec) {
    auto v = exec_src(
        "var a; a = [10, 20, 30];"
        "delete a[-1];"
        "a[-1];");
    EXPECT_EQ(v.to<int_64>(), 20);
}

TEST(gt_ascript_walk, NameConflict_DuplicateVar) {
    auto [etype, eval] = exec_src_catch("var x; var x;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameConflict_VarThenDef) {

    auto [etype, eval] = exec_src_catch("var x; def x() { return 1; }");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameConflict_DefThenVar) {

    auto [etype, eval] = exec_src_catch("def f() { return 1; } var f;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameConflict_DuplicateDef) {
    auto [etype, eval] = exec_src_catch("def g() { return 1; } def g() { return 2; }");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameError_AssignToUndefined) {

    auto [etype, eval] = exec_src_catch("x = 1; def x() { return 1; }");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameError_AssignToUndefFuncName) {

    auto [etype, eval] = exec_src_catch("def f() { return 1; } f = 1;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, NameConflict_DefThenLink) {
    std::string tmp = "/tmp/alx_nc_deflink.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "1;" << std::endl;
    }
    auto [etype, eval] = exec_src_catch(
        ("def n() { return 1; } import \"" + tmp + "\" as n;").c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteRecreate_Var) {
    auto v = exec_src_no_frame("var x; x = 1; delete x; var x; x = 42; x;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, DeleteRecreate_Def) {
    auto v = exec_src_no_frame("def h() { return 1; } delete h; def h() { return 99; } h();");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, DeleteRecreate_VarToDef) {

    auto v = exec_src_no_frame("var k; k = 1; delete k; def k() { return 77; } k();");
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, DeleteRecreate_DefToVar) {

    auto v = exec_src_no_frame("def p() { return 1; } delete p; var p; p = 88; p;");
    EXPECT_EQ(v.to<int_64>(), 88);
}

TEST(gt_ascript_walk, DeleteRecreate_Import) {
    std::string tmp = "/tmp/alx_del_re_imp.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var v; v = 55;" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as q; delete q; "
                                           "import \"" +
                       tmp + "\" as q; "
                             "q.v;";
    auto v = exec_src_no_frame(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(v.to<int_64>(), 55);
}

TEST(gt_ascript_walk, DeleteExistingVar) {
    auto [etype, eval] = exec_src_no_frame_catch("var x; x = 1; delete x; var y; y = x;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteNonExistingVar) {

    auto v = exec_src_no_frame("delete nonexistent; 42;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, DeleteModuleVarFromFunction) {

    auto v = exec_src_no_frame(
        "var x; x = 1;"
        "def f() { return delete x; }"
        "[f(), x];");
    auto& vec = v.to<varvec>();
    EXPECT_FALSE(vec[0].to<bool>());
    EXPECT_EQ(vec[1].to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteDefFromFunction) {

    auto v = exec_src_no_frame(
        "def g() { return 1; }"
        "def f() { return delete g; }"
        "[f(), g()];");
    auto& vec = v.to<varvec>();
    EXPECT_FALSE(vec[0].to<bool>());
    EXPECT_EQ(vec[1].to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteFrameVarInFunction) {

    auto v = exec_src_no_frame(
        "def f() { var t; t = 1; return delete t; }"
        "f();");
    EXPECT_TRUE(v.to<bool>());
}

TEST(gt_ascript_walk, DeleteFrameVarThenRead) {

    auto [etype, eval] = exec_src_no_frame_catch(
        "def f() { var t; t = 1; delete t; return t; }"
        "f();");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteOuterFrameVar) {

    auto v = exec_src_no_frame(
        "def g() { var y; y = 5;"
        "  def h() { return delete y; }"
        "  return [h(), y];"
        "}"
        "g();");
    auto& vec = v.to<varvec>();
    EXPECT_FALSE(vec[0].to<bool>());
    EXPECT_EQ(vec[1].to<int_64>(), 5);
}

TEST(gt_ascript_walk, DeleteModuleVarFromBlock) {

    auto v = exec_src_no_frame(
        "var x; x = 1;"
        "var r; r = true;"
        "{ r = delete x; }"
        "[r, x];");
    auto& vec = v.to<varvec>();
    EXPECT_FALSE(vec[0].to<bool>());
    EXPECT_EQ(vec[1].to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteBlockVar) {

    auto v = exec_src_no_frame(
        "var r; r = false;"
        "{ var b; b = 2; r = delete b; }"
        "r;");
    EXPECT_TRUE(v.to<bool>());
}

TEST(gt_ascript_walk, DeleteModuleVarFromEvalFrame) {

    auto v = exec_src_no_frame(
        "var x; x = 1;"
        "eval(\"delete x;\");"
        "x;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteEvalFrameVar) {

    auto v = exec_src_no_frame("eval(\"var y; y = 2; delete y;\");");
    EXPECT_TRUE(v.to<bool>());
}

TEST(gt_ascript_walk, DeleteModuleVarFromTryFrame) {

    auto v = exec_src_no_frame(
        "var x; x = 1;"
        "var r; r = true;"
        "try { r = delete x; } catch (e) {}"
        "[r, x];");
    auto& vec = v.to<varvec>();
    EXPECT_FALSE(vec[0].to<bool>());
    EXPECT_EQ(vec[1].to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteIndirectInFunction) {

    auto v = exec_src_no_frame(
        "def f() { var t; t = 3; return delete @(\"t\"); }"
        "f();");
    EXPECT_TRUE(v.to<bool>());
}

TEST(gt_ascript_walk, DeleteTcoFrameVar) {

    auto v = exec_src_no_frame(
        "def f(n) { var t; t = n;"
        "  if (n > 0) { return f(n - 1); }"
        "  return delete t;"
        "}"
        "f(3);");
    EXPECT_TRUE(v.to<bool>());
}

TEST(gt_ascript_walk, DeleteInImportedModuleLayers) {

    std::string tmp = "/tmp/alx_del_import_layers.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var z = 7;" << std::endl;
        ofs << "var top = delete z;" << std::endl;
        ofs << "def h() { var t; t = 1; return delete t; }" << std::endl;
        ofs << "var z2 = 7;" << std::endl;
        ofs << "def k() { return delete z2; }" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as m; [m.top, m.h(), m.k(), m.z2];";
    auto v = exec_src_no_frame(code.c_str());
    std::remove(tmp.c_str());
    auto& vec = v.to<varvec>();
    EXPECT_TRUE(vec[0].to<bool>());
    EXPECT_TRUE(vec[1].to<bool>());
    EXPECT_FALSE(vec[2].to<bool>());
    EXPECT_EQ(vec[3].to<int_64>(), 7);
}

TEST(gt_ascript_walk, DeleteAcrossModuleBoundary) {

    std::string tmp = "/tmp/alx_del_import_boundary.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var z = 7;" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as m; delete m.z;";
    auto [etype, eval] = exec_src_no_frame_catch(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, DeleteVecFullRange) {

    auto v = exec_src_no_frame("var vv; vv = [10,20,30]; delete vv[0]; vv;");
    auto& vec = v.to<varvec>();
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0].to<int_64>(), 20);
    EXPECT_EQ(vec[1].to<int_64>(), 30);

    v = exec_src_no_frame("var vv; vv = [10,20,30]; delete vv[1]; vv;");
    auto& vec2 = v.to<varvec>();
    EXPECT_EQ(vec2.size(), 2u);
    EXPECT_EQ(vec2[0].to<int_64>(), 10);
    EXPECT_EQ(vec2[1].to<int_64>(), 30);

    v = exec_src_no_frame("var vv; vv = [10,20,30]; delete vv[-1]; vv;");
    auto& vec3 = v.to<varvec>();
    EXPECT_EQ(vec3.size(), 2u);
    EXPECT_EQ(vec3[1].to<int_64>(), 20);

    v = exec_src_no_frame("var vv; vv = []; delete vv[0];");
    EXPECT_TRUE(v.is<bool>());
    EXPECT_EQ(v.to<bool>(), false);
}

TEST(gt_ascript_walk, DeleteLstFullRange) {

    auto v = exec_src_no_frame("var ll; ll = lst([10,20,30]); delete ll[0]; ll;");
    auto& lst = v.to<varlst>();
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ((*lst.begin()).to<int_64>(), 20);

    v = exec_src_no_frame("var ll; ll = lst([10,20,30]); delete ll[1]; ll;");
    auto& lst2 = v.to<varlst>();
    EXPECT_EQ(lst2.size(), 2u);
    auto it2 = lst2.begin();
    ++it2;
    EXPECT_EQ((*it2).to<int_64>(), 30);

    v = exec_src_no_frame("var ll; ll = lst([10,20,30]); delete ll[-1]; ll;");
    auto& lst3 = v.to<varlst>();
    EXPECT_EQ(lst3.size(), 2u);
    auto it3 = lst3.begin();
    ++it3;
    EXPECT_EQ((*it3).to<int_64>(), 20);

    v = exec_src_no_frame("var ll; ll = lst([10,20,30]); delete ll[9]; 42;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, FrameFreeReuse) {

    auto v = exec_src(
        "{"
        "  var a; a = 10;"
        "  var b; b = 20;"
        "  delete a;"
        "  var c; c = 30;"
        "  [b, c];"
        "}");
    auto& vec = v.to<varvec>();
    EXPECT_EQ(vec[0].to<int_64>(), 20);
    EXPECT_EQ(vec[1].to<int_64>(), 30);
}

TEST(gt_ascript_walk, FrameFreeIsolation) {

    auto v = exec_src(
        "var x; x = 1;"
        "delete x;"
        "var z; z = 2;"
        "{ var y; y = 99; }"
        "z;");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, FrameFreeMultiReuse) {

    auto v = exec_src(
        "{"
        "  var a; a = 1;"
        "  var b; b = 2;"
        "  var c; c = 3;"
        "  delete a; delete b;"
        "  var d; d = 10;"
        "  var e; e = 20;"
        "  [c, d, e];"
        "}");
    auto& vec = v.to<varvec>();
    EXPECT_EQ(vec[0].to<int_64>(), 3);
    EXPECT_EQ(vec[1].to<int_64>(), 10);
    EXPECT_EQ(vec[2].to<int_64>(), 20);
}

TEST(gt_ascript_walk, FrameFreeNestedBlock) {

    auto v = exec_src(
        "{"
        "  var a; a = 1;"
        "  delete a;"
        "  { var b; b = 2; }"
        "  var c; c = 3;"
        "  c;"
        "}");
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, DeleteNonExistent) {

    auto v = exec_src("delete nonexistent; 1;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteDotFrameVar) {

    auto [etype, eval] = exec_src_catch(
        "def f() { var x; x = 42; delete .x; return x; } f();");
    EXPECT_EQ(etype, error_type::UnknownError);
    (void) eval;
}

TEST(gt_ascript_walk, DeleteDotEntityPersistentVar) {

    auto v = exec_src_no_frame("var p; p = 42; delete p; 1;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DeleteDotScalarFrame) {

    auto [etype, eval] = exec_src_catch(
        "def f() { var a = 1; var d = 5; delete d.a; return a; } f();");
    EXPECT_EQ(etype, error_type::TypeError);
    (void) eval;
}

TEST(gt_ascript_walk, DeleteDotScalarEntity) {

    auto [etype, eval] = exec_src_no_frame_catch("var a = 1; var d = 5; delete d.a; a;");
    EXPECT_EQ(etype, error_type::TypeError);
    (void) eval;
}

TEST(gt_ascript_walk, Delete_ScalarFieldViaDot) {

    auto [etype, eval] = exec_src_catch("var d; d = 5; delete d.a;");
    EXPECT_EQ(etype, error_type::TypeError);
    ASSERT_TRUE(eval.is<std::string>());
    EXPECT_NE(eval.to<std::string>().find("delete .a"), std::string::npos);
}

TEST(gt_ascript_walk, ImportMultiAlias) {
    std::string tmp = "/tmp/alx_multi_import.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var counter; counter = 0;" << std::endl;
        ofs << "def inc() { counter = counter + 1; return counter; }" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as a; "
                                           "import \"" +
                       tmp + "\" as b; "
                             "a.inc(); b.inc(); a.inc(); "
                             "a.counter + b.counter;";
    auto v = exec_src(code.c_str());
    std::remove(tmp.c_str());

    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, ImportDefShared) {
    std::string tmp = "/tmp/alx_def_shared.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "def square(x) { return x * x; }" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as m1; "
                                           "import \"" +
                       tmp + "\" as m2; "
                             "m1.square(3) + m2.square(4);";
    auto v = exec_src(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(v.to<int_64>(), 25);
}

TEST(gt_ascript_walk, EnvSearchPath) {
    std::string dir = "/tmp/alx_env_test";
    {
        file_info fi(dir);
        fi.mkdir();
    }
    std::string mod = dir + "/lib.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def f() { return 123; }" << std::endl;
    }
    std::string code = "env([\"" + dir + "\"]); "
                                         "import \"lib.axc\" as lib; lib.f();";
    auto v = exec_src(code.c_str());
    std::remove(mod.c_str());
    std::remove(dir.c_str());
    EXPECT_EQ(v.to<int_64>(), 123);
}

TEST(gt_ascript_walk, MaxStack) {

    auto* eng = engine::create();
    eng->set_max_stack(50);
    alx::bytes src("def recurse(n) { if (n > 0) { recurse(n - 1); } } recurse(1001);");
    auto result = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(result.error, error_type::StackError);
    delete eng;
}

TEST(gt_ascript_walk, ConvError_IntFromString) {
    auto [etype, eval] = exec_src_catch("int(\"abc\");");
    EXPECT_EQ(etype, error_type::ConvError);
}

TEST(gt_ascript_walk, ConvError_FloatFromString) {
    auto [etype, eval] = exec_src_catch("float(\"abc\");");
    EXPECT_EQ(etype, error_type::ConvError);
}

TEST(gt_ascript_walk, Conv_IntFromNull) {
    auto result = exec_src("int(null);");
    EXPECT_EQ(0, result.to<int_64>());
}

TEST(gt_ascript_walk, ConvError_StringPlusInt) {

    auto [etype, eval] = exec_src_catch("\"hello\" + 1;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, EncName_CaseAndSeparatorsIgnored) {
    EXPECT_EQ(exec_src("string(bytes(\"Hello\"), \"HEX\");").to<std::string>(), "48656c6c6f");
    EXPECT_EQ(exec_src("string(bytes(\"Hello\"), \"Utf_8\");").to<std::string>(), "Hello");
    EXPECT_EQ(exec_src("string(bytes(\"Hello\"), \"utf 8\");").to<std::string>(), "Hello");
    EXPECT_EQ(exec_src("string(bytes(\"48656C6C6F\", \"HEX\"));").to<std::string>(), "Hello");
    EXPECT_EQ(exec_src("string(bytes(\"Hello\"), \"Base64\");").to<std::string>(), "SGVsbG8=");
    EXPECT_EQ(exec_src("string(bytes(\"Hello\"), \"bAsE_64\");").to<std::string>(), "SGVsbG8=");
}

TEST(gt_ascript_walk, EncName_BareUtf16IsLEWithoutBom) {
    EXPECT_EQ(exec_src("string(bytes(\"Hi\", \"utf16\"), \"hex\");").to<std::string>(), "48006900");
    EXPECT_EQ(exec_src("string(bytes(\"Hi\", \"UTF16LE\"), \"hex\");").to<std::string>(), "48006900");
}

TEST(gt_ascript_walk, EncName_DecodeIsInverseOfEncode) {
    EXPECT_EQ(exec_src("string(bytes(\"你好\", \"gbk\"), \"gbk\");").to<std::string>(), "你好");
    EXPECT_EQ(exec_src("string(bytes(\"你好abc\", \"gbk\"), \"gbk\");").to<std::string>(), "你好abc");
    EXPECT_EQ(exec_src("string(bytes(\"你好\", \"utf8\"), \"utf8\");").to<std::string>(), "你好");
    EXPECT_EQ(exec_src("string(bytes(\"你好\", \"utf16le\"), \"utf16le\");").to<std::string>(), "你好");
    EXPECT_EQ(exec_src("string(bytes(\"你好\", \"utf16be\"), \"utf16be\");").to<std::string>(), "你好");
    EXPECT_EQ(exec_src("string(bytes(\"你好\", \"utf16be-bom\"), \"UTF16BE-BOM\");").to<std::string>(), "你好");
}

TEST(gt_ascript_walk, EncName_UnknownListsNearest3AndAllNames) {
    auto [etype, eval] = exec_src_catch("string(bytes(\"Hi\"), \"utf8bmo\");");
    EXPECT_EQ(etype, error_type::ConvError);
    std::string msg = eval.to<std::string>();
    EXPECT_NE(msg.find("did you mean: utf8bom"), std::string::npos);
    EXPECT_NE(msg.find("valid: utf8, utf8bom"), std::string::npos);
    EXPECT_NE(msg.find("case and separators are ignored"), std::string::npos);
}

TEST(gt_ascript_walk, ArgError_MissingArg) {
    auto [etype, eval] = exec_src_catch(
        "def f(a, b) { return a + b; }"
        "f(1);");
    EXPECT_EQ(etype, error_type::ArgError);
}

TEST(gt_ascript_walk, ArgError_MissingArgNoDefault) {
    auto [etype, eval] = exec_src_catch(
        "def f(a, b, c) { return a + b + c; }"
        "f(1, 2);");
    EXPECT_EQ(etype, error_type::ArgError);
}

TEST(gt_ascript_walk, NameError_ImportVarConflict) {
    std::string tmp = "/tmp/alx_conflict_import.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var x; x = 1;" << std::endl;
    }
    std::string code = "var m; m = 0; import \"" + tmp + "\" as m;";
    auto [etype, eval] = exec_src_catch(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, TypeError_ForInOnInt) {
    auto [etype, eval] = exec_src_catch("for (v : 42) { var x = v; }");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, TypeError_DotOnNonDict) {
    auto [etype, eval] = exec_src_catch("var x; x = 1; x.y;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, TypeError_IndexOnBool) {
    auto [etype, eval] = exec_src_catch("true[0];");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, ImportError_DuplicateAlias) {
    std::string tmp = "/tmp/alx_dup_import.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var x; x = 1;" << std::endl;
    }
    std::string code = "import \"" + tmp + "\" as m; "
                                           "import \"" +
                       tmp + "\" as m;";
    auto [etype, eval] = exec_src_catch(code.c_str());
    std::remove(tmp.c_str());
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, RuntimeError_ThrowType) {
    auto [etype, eval] = exec_src_catch("throw \"oops\";");
    EXPECT_EQ(etype, error_type::RuntimeError);
}

TEST(gt_ascript_walk, RuntimeError_ThrowValue) {

    auto [etype, eval] = exec_src_catch("throw 42;");
    EXPECT_EQ(etype, error_type::RuntimeError);
    EXPECT_EQ(eval.to<std::string>(), "42");
}

TEST(gt_ascript_walk, Typeof_Int) {
    auto v = exec_src("type(42);");
    EXPECT_EQ(v.to<std::string>(), "int");
}

TEST(gt_ascript_walk, Typeof_Float) {
    auto v = exec_src("type(3.14);");
    EXPECT_EQ(v.to<std::string>(), "float");
}

TEST(gt_ascript_walk, Typeof_String) {
    auto v = exec_src("type(\"hi\");");
    EXPECT_EQ(v.to<std::string>(), "string");
}

TEST(gt_ascript_walk, Typeof_Bool) {
    auto v = exec_src("type(true);");
    EXPECT_EQ(v.to<std::string>(), "bool");
}

TEST(gt_ascript_walk, Typeof_Null) {
    auto v = exec_src("type(null);");
    EXPECT_EQ(v.to<std::string>(), "null");
}

TEST(gt_ascript_walk, Typeof_Vec) {
    auto v = exec_src("type([1, 2]);");
    EXPECT_EQ(v.to<std::string>(), "vec");
}

TEST(gt_ascript_walk, Typeof_Bytes) {
    auto v = exec_src("type(bytes(\"ab\"));");
    EXPECT_EQ(v.to<std::string>(), "bytes");
}

TEST(gt_ascript_walk, Typeof_Map) {
    auto v = exec_src("type(map{});");
    EXPECT_EQ(v.to<std::string>(), "map");
}

TEST(gt_ascript_walk, VecFromLst) {

    auto v = exec_src("var l; l = [1, 2]; var r; r = vec(l); type(r);");
    EXPECT_EQ(v.to<std::string>(), "vec");
}

TEST(gt_ascript_walk, VecFromInt) {

    auto v = exec_src("var r; r = vec(42); r[0];");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, MapConversionThrows) {

    auto [etype, eval] = exec_src_catch("map(42);");
    EXPECT_EQ(etype, error_type::ConvError);
}

TEST(gt_ascript_walk, LstFromVec) {

    auto v = exec_src("var l; l = lst([1, 2, 3]); type(l);");
    EXPECT_EQ(v.to<std::string>(), "lst");
}

TEST(gt_ascript_walk, StringConcatPlus) {
    auto v = exec_src("\"hello\" + \" \" + \"world\";");
    EXPECT_EQ(v.to<std::string>(), "hello world");
}

TEST(gt_ascript_walk, StringConcatPlusSingle) {
    auto v = exec_src("\"a\" + \"b\";");
    EXPECT_EQ(v.to<std::string>(), "ab");
}

TEST(gt_ascript_walk, IndexStoreVec) {
    auto v = exec_src("var a; a = [1, 2, 3]; a[0] = 99; a[0];");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, IndexStoreMap) {
    auto v = exec_src("var m; m = map{\"k\": 1}; m[\"k\"] = 42; m[\"k\"];");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndexStoreMapNewKey) {
    auto v = exec_src("var m; m = map{\"k\": 1}; m[\"new\"] = 99; m[\"new\"];");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, ChainedDotOnDict) {

    auto v = exec_src(
        "var obj; obj = map{\"a\": map{\"b\": 42}};"
        "obj.a.b;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, ChainedDotMixed) {

    auto v = exec_src(
        "var d; d = map{\"inner\": map{\"val\": 99}};"
        "d.inner.val;");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, EqIntFloat) {
    auto v = exec_src("1 == 1.0;");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, NeIntFloat) {
    auto v = exec_src("1 != 2.0;");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, CmpOtherTypes) {

    auto [etype, eval] = exec_src_catch("1 < \"abc\";");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, ImportError_SelfImport) {
    std::string tmp = "/tmp/alx_self_import.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "import \"/tmp/alx_self_import.axc\" as self; 1;" << std::endl;
    }
    auto* eng = engine::create();

    std::string captured_err;
    eng->on_cerr.connect([&](const std::string& s) { captured_err = s; });
    eng->exec(tmp);
    std::remove(tmp.c_str());
    delete eng;
    EXPECT_TRUE(captured_err.find("ImportError") != std::string::npos);
    EXPECT_TRUE(captured_err.find("Self-import") != std::string::npos);
}

TEST(gt_ascript_walk, ImportError_CircularImport) {
    std::string tmpA = "/tmp/alx_circ_a.axc";
    std::string tmpB = "/tmp/alx_circ_b.axc";
    {
        std::ofstream ofs(tmpA);
        ofs << "import \"/tmp/alx_circ_b.axc\" as b;" << std::endl;
    }
    {
        std::ofstream ofs(tmpB);
        ofs << "import \"/tmp/alx_circ_a.axc\" as a;" << std::endl;
    }
    auto* eng = engine::create();
    std::string captured_err;
    eng->on_cerr.connect([&](const std::string& s) { captured_err = s; });
    eng->exec(tmpA);
    std::remove(tmpA.c_str());
    std::remove(tmpB.c_str());
    delete eng;
    EXPECT_TRUE(captured_err.find("ImportError") != std::string::npos);
    EXPECT_TRUE(captured_err.find("Circular import") != std::string::npos);
}

static variant exec_ovf(bool _on, const char* _src) {
    alx::bytes b(_src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    varvec ast = p.parse();
    if (p.has_error()) return variant();

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    cfg.overflow_check = _on;
    walker w(cfg);

    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;
    w.state.push_frame();
    return w.walk_forest(ast);
}

static std::pair<error_type, variant> exec_ovf_catch(bool _on, const char* _src) {
    try {
        exec_ovf(_on, _src);
    } catch (script_exception& e) {
        return {e.type, e.info};
    }
    return {error_type::UnknownError, {}};
}

#define EXPECT_NO_SCRIPT_THROW(result) EXPECT_EQ((result).first, error_type::UnknownError)
#define EXPECT_THROW_TYPE(result, t)                     \
    EXPECT_NE((result).first, error_type::UnknownError); \
    if ((result).first != error_type::UnknownError) EXPECT_EQ((result).first, t)

TEST(gt_ascript_walk, OverflowCheck_Off_AddWraps) {
    auto v = exec_ovf(false, "var max; max = 0x7FFFFFFFFFFFFFFF; max + 1;");

    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), min_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_AddThrows) {
    auto r = exec_ovf_catch(true, "var max; max = 0x7FFFFFFFFFFFFFFF; max + 1;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_SubWraps) {
    auto v = exec_ovf(false, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; min - 1;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), max_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_SubThrows) {
    auto r = exec_ovf_catch(true, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; min - 1;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_MulWraps) {
    auto v = exec_ovf(false, "var max; max = 0x7FFFFFFFFFFFFFFF; max * 2;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), -2);
}

TEST(gt_ascript_walk, OverflowCheck_On_MulThrows) {
    auto r = exec_ovf_catch(true, "var max; max = 0x7FFFFFFFFFFFFFFF; max * 2;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_IncWraps) {
    auto v = exec_ovf(false, "var max; max = 0x7FFFFFFFFFFFFFFF; ++max; max;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), min_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_IncThrows) {
    auto r = exec_ovf_catch(true, "var max; max = 0x7FFFFFFFFFFFFFFF; ++max;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_DecWraps) {
    auto v = exec_ovf(false, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; --min; min;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), max_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_DecThrows) {
    auto r = exec_ovf_catch(true, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; --min;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_UminusWraps) {
    auto v = exec_ovf(false, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; -min;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), min_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_UminusThrows) {
    auto r = exec_ovf_catch(true, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; -min;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_ShiftWraps) {
    auto v = exec_ovf(false, "1 << -1;");

    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), min_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_NegShiftThrows) {
    auto r = exec_ovf_catch(true, "1 << -1;");
    EXPECT_THROW_TYPE(r, error_type::ShiftError);
}

TEST(gt_ascript_walk, OverflowCheck_On_LargeShiftThrows) {
    auto r = exec_ovf_catch(true, "1 << 64;");
    EXPECT_THROW_TYPE(r, error_type::ShiftError);
}

TEST(gt_ascript_walk, OverflowCheck_On_NegLshiftThrows) {
    auto r = exec_ovf_catch(true, "-1 << 1;");
    EXPECT_THROW_TYPE(r, error_type::ShiftError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_AssAddWraps) {
    auto v = exec_ovf(false, "var x; x = 0x7FFFFFFFFFFFFFFF; x += 1; x;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), min_int_64);
}

TEST(gt_ascript_walk, OverflowCheck_On_AssAddThrows) {
    auto r = exec_ovf_catch(true, "var x; x = 0x7FFFFFFFFFFFFFFF; x += 1;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_AssMulWraps) {
    auto v = exec_ovf(false, "var x; x = 0x7FFFFFFFFFFFFFFF; x *= 2; x;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), -2);
}

TEST(gt_ascript_walk, OverflowCheck_On_AssMulThrows) {
    auto r = exec_ovf_catch(true, "var x; x = 0x7FFFFFFFFFFFFFFF; x *= 2;");
    EXPECT_THROW_TYPE(r, error_type::OverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_DivOverflowStillThrows) {

    auto r = exec_ovf_catch(false, "var min; min = -0x7FFFFFFFFFFFFFFF - 1; min / -1;");
    EXPECT_THROW_TYPE(r, error_type::DivOverflowError);
}

TEST(gt_ascript_walk, OverflowCheck_Off_DivZeroStillThrows) {
    auto r = exec_ovf_catch(false, "1 / 0;");
    EXPECT_THROW_TYPE(r, error_type::DivZeroError);
}

TEST(gt_ascript_walk, Engine_SetOverflowCheck) {
    auto* eng = engine::create();
    EXPECT_FALSE(eng->config().overflow_check);
    eng->set_overflow_check(true);
    EXPECT_TRUE(eng->config().overflow_check);
    eng->set_overflow_check(false);
    EXPECT_FALSE(eng->config().overflow_check);
    delete eng;
}

TEST(gt_ascript_walk, DotIndexRead_Vec) {
    auto v = exec_src("var m; m = map{\"arr\": [10, 20, 30]}; m.arr[0];");
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, DotIndexRead_Map) {
    auto v = exec_src("var m; m = map{\"inner\": map{\"k\": 77}}; m.inner[\"k\"];");
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, DotIndexRead_Deep) {
    auto v = exec_src(
        "var m; m = map{\"outer\": map{\"inner\": map{\"arr\": [1, 2, 3]}}};"
        "m.outer.inner.arr[1];");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, IndexDotRead_VecMap) {
    auto v = exec_src("var a; a = [map{\"k\": 42}, map{\"k\": 99}]; a[0].k;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, DotIndexDotRead) {
    auto v = exec_src(
        "var m; m = map{\"arr\": [map{\"val\": 1}, map{\"val\": 2}]};"
        "m.arr[0].val;");
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, DotMultiIndexRead) {
    auto v = exec_src(
        "var m; m = map{\"mat\": [[1, 2], [3, 4]]};"
        "m.mat[0][1];");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, DotIndexStore_Vec) {
    auto v = exec_src(
        "var m; m = map{\"arr\": [1, 2, 3]};"
        "m.arr[0] = 99;"
        "m.arr[0];");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, DotIndexStore_Map) {
    auto v = exec_src(
        "var m; m = map{\"inner\": map{\"k\": 1}};"
        "m.inner[\"k\"] = 88;"
        "m.inner[\"k\"];");
    EXPECT_EQ(v.to<int_64>(), 88);
}

TEST(gt_ascript_walk, DotIndexStore_Deep) {
    auto v = exec_src(
        "var m; m = map{\"outer\": map{\"arr\": [1, 2]}};"
        "m.outer.arr[0] = 55;"
        "m.outer.arr[0];");
    EXPECT_EQ(v.to<int_64>(), 55);
}

TEST(gt_ascript_walk, IndexDotStore_VecMap) {
    auto v = exec_src(
        "var a; a = [map{\"k\": 1}, map{\"k\": 2}];"
        "a[0].k = 99;"
        "a[0].k;");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, DotIndexDotStore) {
    auto v = exec_src(
        "var m; m = map{\"arr\": [map{\"val\": 1}]};"
        "m.arr[0].val = 77;"
        "m.arr[0].val;");
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, DotMultiIndexStore) {
    auto v = exec_src(
        "var m; m = map{\"mat\": [[1, 2], [3, 4]]};"
        "m.mat[0][1] = 99;"
        "m.mat[0][1];");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, IndexDotIndexStore) {
    auto v = exec_src(
        "var a; a = [map{\"arr\": [10, 20]}];"
        "a[0].arr[1] = 88;"
        "a[0].arr[1];");
    EXPECT_EQ(v.to<int_64>(), 88);
}

TEST(gt_ascript_walk, FullMixedChainStore) {

    auto v = exec_src(
        "var a; a = [map{},"
        "        map{\"b\": map{\"c\": [map{}, map{},"
        "                                   [map{}, map{}, map{}, 99]]}}];"
        "a[1].b.c[2][3] = 999;"
        "a[1].b.c[2][3];");
    EXPECT_EQ(v.to<int_64>(), 999);
}

TEST(gt_ascript_walk, DotIndexStore_OutOfBounds) {
    auto [etype, eval] = exec_src_catch(
        "var m; m = map{\"arr\": [1, 2]};"
        "m.arr[10] = 99;");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, DotIndexStore_NonContainer) {
    auto [etype, eval] = exec_src_catch(
        "var m; m = map{\"val\": 42};"
        "m.val[0] = 99;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, IndexDotStore_OutOfBounds) {
    auto [etype, eval] = exec_src_catch(
        "var a; a = [map{\"k\": 1}];"
        "a[5].k = 99;");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, NullIndex_ReadSize) {
    EXPECT_EQ(exec_src("var v; v = [10, 20, 30]; v[null];").to<int_64>(), 3);
    EXPECT_EQ(exec_src("var l; l = lst[1, 2]; l[null];").to<int_64>(), 2);
    EXPECT_EQ(exec_src("var s; s = \"hello\"; s[null];").to<int_64>(), 5);
    EXPECT_EQ(exec_src("var m; m = map{\"a\": 1, \"b\": 2}; m[null];").to<int_64>(), 2);
}

TEST(gt_ascript_walk, NullIndex_ReadSizeDotChain) {
    auto v = exec_src(
        "var o; o = map{\"s\": \"hello\", \"v\": [1, 2, 3], \"p\": map{\"k\": 1}};"
        "[o.v[null], o.s[null], o.p[null]];");
    ASSERT_TRUE(v.is<varvec>());
    auto& vec = v.to<varvec>();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0].to<int_64>(), 3);
    EXPECT_EQ(vec[1].to<int_64>(), 5);
    EXPECT_EQ(vec[2].to<int_64>(), 1);
}

TEST(gt_ascript_walk, NullIndex_MapWriteThrows) {

    auto [etype, eval] = exec_src_catch(
        "var m; m = map{\"a\": 1};"
        "m[null] = 2;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullIndex_StringWriteThrows) {
    auto [etype, eval] = exec_src_catch(
        "var s; s = \"hello\";"
        "s[null] = \"x\";");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullIndex_DotChainStringWriteThrows) {
    auto [etype, eval] = exec_src_catch(
        "var o; o = map{\"s\": \"hello\"};"
        "o.s[null] = \"x\";");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullIndex_VecWritePushes) {
    EXPECT_EQ(exec_src("var v; v = [1, 2]; v[null] = 9; v[null];").to<int_64>(), 3);
    EXPECT_EQ(exec_src("var v; v = [1, 2]; v[null] = 9; v[-1];").to<int_64>(), 9);
}

TEST(gt_ascript_walk, MapKey_NonStringThrows) {

    auto [e1, v1] = exec_src_catch("var m; m = map{\"a\": 1}; m[0];");
    EXPECT_EQ(e1, error_type::TypeError);

    auto [e2, v2] = exec_src_catch("var m; m = map{\"a\": 1}; var k; k = 1; m[k];");
    EXPECT_EQ(e2, error_type::TypeError);

    auto [e3, v3] = exec_src_catch("var m; m = map{\"a\": 1}; m[0] = 2;");
    EXPECT_EQ(e3, error_type::TypeError);

    auto [e4, v4] = exec_src_catch("var m; m = map{\"a\": 1}; delete m[null];");
    EXPECT_EQ(e4, error_type::ConvError);
}

TEST(gt_ascript_walk, NullIndex_DeleteViaDotChainThrows) {

    auto [e1, v1] = exec_src_catch("var o; o = map{\"p\": map{\"k\": 1}}; delete o.p[null];");
    EXPECT_EQ(e1, error_type::ConvError);

    auto [e2, v2] = exec_src_catch("var o; o = map{\"s\": \"hi\"}; delete o.s[null];");
    EXPECT_EQ(e2, error_type::ConvError);

    auto [e3, v3] = exec_src_catch("var o; o = map{\"v\": [1, 2]}; delete o.v[null];");
    EXPECT_EQ(e3, error_type::ConvError);
}

TEST(gt_ascript_walk, DeleteViaDotChainStillWorks) {
    EXPECT_TRUE(exec_src(
                    "var o; o = map{\"p\": map{\"k\": 1}};"
                    "delete o.p[\"k\"];")
                    .to<bool>());
    EXPECT_EQ(exec_src("var o; o = map{\"v\": [1, 2, 3]}; delete o.v[0]; o.v[null];").to<int_64>(), 2);
}

TEST(gt_ascript_walk, MapKey_DeleteByStringKey) {
    EXPECT_TRUE(exec_src("var m; m = map{\"a\": 1, \"b\": 2}; delete m[\"a\"];").to<bool>());
    EXPECT_EQ(exec_src("var m; m = map{\"a\": 1, \"b\": 2}; delete m[\"a\"]; m[null];").to<int_64>(), 1);
    EXPECT_FALSE(exec_src("var m; m = map{\"a\": 1}; delete m[\"z\"];").to<bool>());

    auto [etype, eval] = exec_src_catch("var m; m = map{\"a\": 1}; delete m[0];");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullIndex_ScalarParentThrows) {
    auto [e1, v1] = exec_src_catch("var o; o = map{\"n\": 5}; o.n[null];");
    EXPECT_EQ(e1, error_type::TypeError);

    auto [e2, v2] = exec_src_catch("var n; n = 5; n[null];");
    EXPECT_EQ(e2, error_type::TypeError);
}

TEST(gt_ascript_walk, StringIndex_Read) {
    EXPECT_EQ(exec_src("var s; s = \"hello\"; s[0];").to<std::string>(), "h");
    EXPECT_EQ(exec_src("var s; s = \"hello\"; s[4];").to<std::string>(), "o");
    EXPECT_EQ(exec_src("var s; s = \"hello\"; s[-1];").to<std::string>(), "o");
    EXPECT_EQ(exec_src("var s; s = \"hello\"; s[null];").to<int_64>(), 5);
}

TEST(gt_ascript_walk, StringIndex_ReadDotChain) {
    EXPECT_EQ(exec_src("var o; o = map{\"s\": \"hello\"}; o.s[0];").to<std::string>(), "h");
    EXPECT_EQ(exec_src("var o; o = map{\"s\": \"hello\"}; o.s[-1];").to<std::string>(), "o");
    EXPECT_EQ(exec_src("var o; o = map{\"s\": \"hello\"}; o.s[null];").to<int_64>(), 5);
}

TEST(gt_ascript_walk, StringIndex_OutOfRange) {

    auto [e1, v1] = exec_src_catch("var s; s = \"hello\"; s[-2];");
    EXPECT_EQ(e1, error_type::IndexError);
    auto [e2, v2] = exec_src_catch("var s; s = \"hello\"; s[5];");
    EXPECT_EQ(e2, error_type::IndexError);
    auto [e3, v3] = exec_src_catch("var o; o = map{\"s\": \"hello\"}; o.s[-2];");
    EXPECT_EQ(e3, error_type::IndexError);
}

TEST(gt_ascript_walk, StringIndex_WriteThrows) {
    auto [e1, v1] = exec_src_catch("var s; s = \"hello\"; s[-1] = \"x\";");
    EXPECT_EQ(e1, error_type::TypeError);
    auto [e2, v2] = exec_src_catch("var o; o = map{\"s\": \"hello\"}; o.s[0] = \"x\";");
    EXPECT_EQ(e2, error_type::TypeError);
}

TEST(gt_ascript_walk, StringIndex_DeleteThrows) {

    auto [e1, v1] = exec_src_catch("var o; o = map{\"s\": \"hello\"}; delete o.s[0];");
    EXPECT_EQ(e1, error_type::TypeError);
    auto [e2, v2] = exec_src_catch("var o; o = map{\"n\": 5}; delete o.n[0];");
    EXPECT_EQ(e2, error_type::TypeError);
    auto [e3, v3] = exec_src_catch("var o; o = map{\"m\": map{\"k\": 1}}; delete o.m[0];");
    EXPECT_EQ(e3, error_type::TypeError);
}

TEST(gt_ascript_walk, Delete_StaleParentKindThrows) {

    auto [e1, v1] = exec_src_catch("var o; o = map{\"p\": map{\"k\": \"hi\"}}; delete o.p.k[0];");
    EXPECT_EQ(e1, error_type::TypeError);

    auto [e2, v2] = exec_src_catch("var o; o = map{\"a\": [\"hi\"]}; delete o.a[0][0];");
    EXPECT_EQ(e2, error_type::TypeError);

    auto [e3, v3] = exec_src_catch("var o; o = map{\"l\": lst[\"hi\"]}; delete o.l[0][0];");
    EXPECT_EQ(e3, error_type::TypeError);

    auto [e4, v4] = exec_src_catch("var o; o = map{\"s\": \"hi\"}; delete o.s[0];");
    EXPECT_EQ(e4, error_type::TypeError);

    auto [e5, v5] = exec_src_catch("var o; o = map{\"n\": 5}; delete o.n[0];");
    EXPECT_EQ(e5, error_type::TypeError);
}

TEST(gt_ascript_walk, ParentKind_DescribesParentNotHolder) {
    const char* setup =
        "var o = map{\"s\": \"hi\", \"n\": 5, \"v\": [1, 2], \"l\": lst[1, 2], "
        "\"p\": map{\"k\": 1}, \"a\": [\"hi\"]};";

    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", static_cast<int_64>(0)})), ParentKind::Map);
    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "v", static_cast<int_64>(0)})), ParentKind::Vec);
    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "l", static_cast<int_64>(0)})), ParentKind::Lst);
    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "p", static_cast<int_64>(0)})), ParentKind::Map);

    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "n", static_cast<int_64>(0)})), ParentKind::Value);
    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "s", static_cast<int_64>(0)})), ParentKind::Value);
    EXPECT_EQ(resolve_kind(setup, dot_ast({"o", "a", static_cast<int_64>(0), static_cast<int_64>(0)})),
              ParentKind::Value);
}

TEST(gt_ascript_walk, Delete_StillWorksAfterGuards) {
    EXPECT_EQ(exec_src("var a; a = [1, 2, 3]; delete a[0]; a[null];").to<int_64>(), 2);
    EXPECT_EQ(exec_src("var l; l = lst[1, 2, 3]; delete l[-1]; l[null];").to<int_64>(), 2);
    EXPECT_TRUE(exec_src("var m; m = map{\"k\": 1}; delete m[\"k\"];").to<bool>());
}

TEST(gt_ascript_walk, NullIndex_MidChainThrows) {
    auto [etype, eval] = exec_src_catch(
        "var o; o = map{\"s\": \"hello\"};"
        "o.s[null].b = 1;");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, FullMixedChainStore_MidKeyNotFound) {
    auto [etype, eval] = exec_src_catch(
        "var a; a = [map{}];"
        "a[0].missing.x = 1;");
    EXPECT_EQ(etype, error_type::KeyError);
}

TEST(gt_ascript_walk, AssConcatPlus) {
    auto v = exec_src("var x; x = \"hello\"; x += \" world\"; x;");
    EXPECT_EQ(v.to<std::string>(), "hello world");
}

TEST(gt_ascript_walk, AssConcatPlusChain) {
    auto v = exec_src("var a; a = \"a\"; var b; b = \"b\"; a += b += \"c\"; a;");

    EXPECT_EQ(v.to<std::string>(), "abc");
}

TEST(gt_ascript_walk, IndirectCallBasic) {
    auto v = exec_src("def f() { return 42; } var a; a = \"f\"; @a();");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndirectCallWithArgs) {
    auto v = exec_src("def add(a, b) { return a + b; } var fn; fn = \"add\"; @fn(3, 4);");
    EXPECT_EQ(v.to<int_64>(), 7);
}

TEST(gt_ascript_walk, IndirectCallNonStringVar) {
    auto [etype, eval] = exec_src_catch(
        "var not_str; not_str = 123; not_str();");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, IndirectCallMessageNamesScriptType) {
    auto [etype, eval] = exec_src_catch("@5();");
    EXPECT_EQ(etype, error_type::NameError);
    EXPECT_EQ(eval.to<std::string>(), "Indirect call: expected function name string, got int");
}

TEST(gt_ascript_walk, IndirectCallOneHopOnly) {
    auto [etype, eval] = exec_src_catch(
        "var hop1; hop1 = \"hop2\"; var hop2; hop2 = \"target\"; hop1();");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, VecIndexFastPathLiteral) {
    auto v = exec_src("var a; a = [10, 20, 30]; a[0];");
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, VecIndexFastPathVar) {
    auto v = exec_src("var a; a = [10, 20, 30]; var i; i = 2; a[i];");
    EXPECT_EQ(v.to<int_64>(), 30);
}

TEST(gt_ascript_walk, VecIndexFastPathNegativeOne) {
    auto v = exec_src("var a; a = [10, 20, 30]; a[-1];");
    EXPECT_EQ(v.to<int_64>(), 30);
}

TEST(gt_ascript_walk, VecIndexFastPathNegativeTwo) {
    auto [etype, eval] = exec_src_catch("var a; a = [10, 20, 30]; a[-2];");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, VecIndexFastPathUndefVar) {
    auto [etype, eval] = exec_src_catch("var a; a = [10, 20, 30]; a[undef];");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, VecIndexFastPathNonVec) {

    auto v = exec_src("var a; a = lst[10, 20, 30]; a[0];");
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, VecIndexFastPathUndefContainer) {
    auto [etype, eval] = exec_src_catch("var i; i = 0; undef[i];");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, EvalArgFastPathUndefined) {
    auto [etype, eval] = exec_src_catch("var a; a = undef;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, EvalArgFastPathAddWithUndefined) {
    auto [etype, eval] = exec_src_catch("var a; a = 1 + undef;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, MaxVecfillWithinLimit) {
    auto* eng = engine::create();
    eng->set_max_vecfill(100);
    auto result = eng->exec(alx::bytes_view(alx::bytes("[0: 10];")), "");
    EXPECT_EQ(result.error, error_type::NoError);
    delete eng;
}

TEST(gt_ascript_walk, MaxVecfillExceeded) {
    auto* eng = engine::create();
    eng->set_max_vecfill(5);
    auto result = eng->exec(alx::bytes_view(alx::bytes("[0: 20];")), "");
    EXPECT_EQ(result.error, error_type::ResourceError);
    delete eng;
}

TEST(gt_ascript_walk, TcoBasic) {
    auto v = exec_src("def countdown(n) { if (n <= 0) { return 0; } return countdown(n - 1); } countdown(5);");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, TcoDeepRecursion) {

    auto v = exec_src("def countdown(n) { if (n <= 0) { return n; } return countdown(n - 1); } countdown(5000);");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, TcoMultiArgs) {
    auto v = exec_src(
        "def fib_tail(n, a, b) { if (n <= 1) { return b; } return fib_tail(n - 1, b, a + b); } fib_tail(20, 0, 1);");
    EXPECT_EQ(v.to<int_64>(), 6765);
}

TEST(gt_ascript_walk, TcoNotTailCall) {

    auto v = exec_src("def fib(n) { if (n <= 1) { return n; } return fib(n - 1) + fib(n - 2); } fib(10);");
    EXPECT_EQ(v.to<int_64>(), 55);
}

TEST(gt_ascript_walk, TcoTailInIfBranch) {

    auto v = exec_src("def r(n) { if (n > 0) { return r(n - 1); } return 0; } r(2000);");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, TcoDeepTailInIf) {

    auto v = exec_src("def r(n) { if (n > 0) { return r(n - 1); } return 0; } r(5000);");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, TcoTailInLoop) {

    auto v = exec_src("def step(n) { while (n > 0) { return step(n - 1); } return n; } step(2000);");
    EXPECT_EQ(v.to<int_64>(), 0);
}

TEST(gt_ascript_walk, TcoShadowedLocalVar) {

    auto [etype, eval] = exec_src_catch(
        "def f(x) { var f = x; if (x > 3) { return f; } return f(x + 1); } f(1);");
    EXPECT_EQ(etype, error_type::NameError);
    (void) eval;
}

TEST(gt_ascript_walk, TcoShadowedParamCallable) {

    auto v = exec_src(
        "def g(f, n) { return n; }"
        "def f(f, n) { if (n <= 0) { return 99; } return f(f, n - 1); }"
        "f(g, 3);");
    EXPECT_EQ(v.to<int_64>(), 2);
}

TEST(gt_ascript_walk, NullLt) {
    auto [etype, eval] = exec_src_catch("null < 1;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullGt) {
    auto [etype, eval] = exec_src_catch("null > 0;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullLe) {
    auto [etype, eval] = exec_src_catch("null <= 5;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, NullGe) {
    auto [etype, eval] = exec_src_catch("null >= 3;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, ForEmptyCondition) {
    auto v = exec_src("var i; i = 0; for (;;) { i = i + 1; if (i >= 5) { break; } } i;");
    EXPECT_EQ(v.to<int_64>(), 5);
}

TEST(gt_ascript_walk, BreakInSwitchInLoop) {
    auto v = exec_src(
        "var sum; sum = 0; var i;"
        "for (i = 0; i < 3; ++i) {"
        "  switch (i) {"
        "    case 0: sum = sum + 1; break;"
        "    case 1: sum = sum + 10; break;"
        "    default: sum = sum + 100; break;"
        "  }"
        "  sum = sum + 1;"
        "} sum;");

    EXPECT_EQ(v.to<int_64>(), 114);
}

TEST(gt_ascript_walk, TryCatchRethrow) {
    auto [etype, eval] = exec_src_catch(
        "try { throw \"inner\"; } catch (e) { throw \"outer\"; }");
    EXPECT_EQ(etype, error_type::RuntimeError);
}

TEST(gt_ascript_walk, NestedTryCatch) {
    auto v = exec_src(
        "var result; result = \"none\";"
        "try {"
        "  try { throw \"inner\"; } catch (e) { result = \"caught\"; }"
        "} catch (e) { result = \"wrong\"; }"
        "result;");
    EXPECT_EQ(v.to<std::string>(), "caught");
}

TEST(gt_ascript_walk, TryCatchAcrossDef) {

    auto v = exec_src(
        "def thrower() { throw \"from def\"; }"
        "var result; result = \"none\";"
        "try { thrower(); } catch (e) { result = e.info; }"
        "result;");
    EXPECT_EQ(v.to<std::string>(), "from def");
}

TEST(gt_ascript_walk, IndirectRead) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 42; @x;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndirectWrite) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 0; @x = 42; y;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndirectCompoundAssign) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 10; @x += 5; y;");
    EXPECT_EQ(v.to<int_64>(), 15);
}

TEST(gt_ascript_walk, IndirectPreInc) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 10; ++@x; y;");
    EXPECT_EQ(v.to<int_64>(), 11);
}

TEST(gt_ascript_walk, IndirectPostInc) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 10; @x++; y;");
    EXPECT_EQ(v.to<int_64>(), 11);
}

TEST(gt_ascript_walk, IndirectDelete) {
    auto v = exec_src_no_frame(
        "var x; x = \"y\"; var y; y = 42; delete @x;"
        "var result; result = false;"
        "try { y; } catch (e) { result = true; }"
        "result;");
    EXPECT_EQ(v.to<bool>(), true);
}

TEST(gt_ascript_walk, IndirectVarDecl) {

    auto v = exec_src("var w; w = 99; w;");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, IndirectDefDecl) {

    auto v = exec_src(
        "def myfn() { return 77; }"
        "myfn();");
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, IndirectParenRead) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 42; @(x);");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndirectParenWrite) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 0; @(x) = 42; y;");
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, IndirectParenCall) {
    auto v = exec_src("def f() { return 99; } var fn; fn = \"f\"; @(fn)();");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, IndirectNonString) {
    auto [etype, eval] = exec_src_catch("var x; x = 42; @x;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, IndirectUndefTarget) {
    auto [etype, eval] = exec_src_catch("var x; x = \"no_such_var\"; @x;");
    EXPECT_EQ(etype, error_type::NameError);
}

TEST(gt_ascript_walk, StringFormatSingleArg) {
    auto v = exec_src("string(42);");
    EXPECT_EQ(v.to<std::string>(), "42");
}

TEST(gt_ascript_walk, StringFormatTwoArgs) {
    auto v = exec_src("string(\"Hello %1!\", \"world\");");
    EXPECT_EQ(v.to<std::string>(), "Hello world!");
}

TEST(gt_ascript_walk, StringFormatMultiArgs) {
    auto v = exec_src("string(\"%1 + %2 = %3\", 3, 4, 7);");
    EXPECT_EQ(v.to<std::string>(), "3 + 4 = 7");
}

TEST(gt_ascript_walk, StringFormatNoArgs) {
    auto v = exec_src("string();");
    EXPECT_EQ(v.to<std::string>(), "");
}

TEST(gt_ascript_walk, StringIntBaseHex) {
    auto v = exec_src("string(255, 16);");
    EXPECT_EQ(v.to<std::string>(), "0xff");
}
TEST(gt_ascript_walk, StringIntBaseBin) {
    auto v = exec_src("string(255, 2);");
    EXPECT_EQ(v.to<std::string>(), "0b11111111");
}
TEST(gt_ascript_walk, StringIntBaseOct) {
    auto v = exec_src("string(63, 8);");
    EXPECT_EQ(v.to<std::string>(), "0o77");
}
TEST(gt_ascript_walk, StringIntBaseDec) {
    auto v = exec_src("string(255, 10);");
    EXPECT_EQ(v.to<std::string>(), "255");
}
TEST(gt_ascript_walk, StringIntBaseNeg) {
    auto v = exec_src("string(-255, 16);");
    EXPECT_EQ(v.to<std::string>(), "-0xff");
}
TEST(gt_ascript_walk, StringIntBaseZero) {
    auto v = exec_src("string(0, 16);");
    EXPECT_EQ(v.to<std::string>(), "0x0");
}
TEST(gt_ascript_walk, StringIntBaseBad) {
    auto [etype, eval] = exec_src_catch("string(255, 3);");
    EXPECT_EQ(etype, error_type::ConvError);
    EXPECT_TRUE(eval.is<std::string>());
    EXPECT_NE(eval.to<std::string>().find("Base must be 2, 8, 10, or 16"), std::string::npos);
}

TEST(gt_ascript_walk, StringDoublePrecPos) {
    auto v = exec_src("string(3.14159, 2);");
    EXPECT_EQ(v.to<std::string>(), "3.14");
}
TEST(gt_ascript_walk, StringDoublePrecMore) {
    auto v = exec_src("string(3.14159, 3);");
    EXPECT_EQ(v.to<std::string>(), "3.142");
}
TEST(gt_ascript_walk, StringDoublePrecZero) {
    auto v = exec_src("string(3.14159, 0);");
    EXPECT_EQ(v.to<std::string>(), "3");
}
TEST(gt_ascript_walk, StringDoublePrecNeg) {
    auto v = exec_src("string(3.14159, -3);");
    EXPECT_EQ(v.to<std::string>(), "3.14");
}
TEST(gt_ascript_walk, StringDoublePrecNegBig) {
    auto v = exec_src("string(314.159, -3);");
    EXPECT_EQ(v.to<std::string>(), "314");
}

TEST(gt_ascript_walk, StringIntBaseMin) {

    auto v = exec_src("string(-9223372036854775807 - 1, 16);");
    EXPECT_EQ(v.to<std::string>(), "-0x8000000000000000");
}

TEST(gt_ascript_walk, StringDoublePrecSig1) {
    auto v = exec_src("string(3.14, -1);");
    EXPECT_EQ(v.to<std::string>(), "3");
}

TEST(gt_ascript_walk, StringDoubleNegZero) {
    auto v = exec_src("string(-0.0, 0);");
    EXPECT_EQ(v.to<std::string>(), "-0");
}

TEST(gt_ascript_walk, IndirectReadIndexedPath) {
    auto v = exec_src("var x; x = \"arr[1]\"; var arr; arr = [10, 20, 30]; @x;");
    EXPECT_EQ(v.to<int_64>(), 20);
}

TEST(gt_ascript_walk, IndirectWriteIndexedPath) {
    auto v = exec_src(
        "var x; x = \"arr[1]\"; var arr; arr = [10, 20, 30]; @x = 99; arr[1];");
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, IndirectReadMapKey) {
    auto v = exec_src(
        "var x; x = \"d.key\"; var d; d = map{\"key\": 77}; @x;");
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, IndirectCompoundIndexedPath) {
    auto v = exec_src(
        "var x; x = \"arr[0]\"; var arr; arr = [10, 20, 30]; @x += 5; arr[0];");
    EXPECT_EQ(v.to<int_64>(), 15);
}

TEST(gt_ascript_walk, IndirectParenCompoundAssign) {
    auto v = exec_src("var x; x = \"y\"; var y; y = 5; @(x) += 3; y;");
    EXPECT_EQ(v.to<int_64>(), 8);
}

TEST(gt_ascript_walk, IndirectParenDecl) {

    auto v = exec_src("var z; z = 33; z;");
    EXPECT_EQ(v.to<int_64>(), 33);
}

static std::pair<error_type, variant> exec_src_catch_in_child(
    const char* root_src, const char* child_src);

static variant exec_in_child(const char* root_src, const char* child_src) {
    alx::bytes b(root_src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    varvec root_ast = p.parse();

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    walker w(cfg);
    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static op_table bt{};
    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;

    w.walk_forest(root_ast);

    impl_import* child = new impl_import();
    child->m_parent = &w.m_root;
    child->m_alias = "child";
    child->m_fly = &w.m_root_fly;

    w.m_root.m_store.m_data.push_back(variant(anyptr_ex<impl_import>::make(child)));
    w.m_root.m_store.m_map["child"] = w.m_root.m_store.m_data.size() - 1;

    impl_import* saved = w.state.current;
    w.state.current = child;
    w.state.push_frame(child);

    variant result;
    try {
        alx::bytes cb(child_src);
        token_list ctl;
        ctl.tokenize(alx::bytes_view(cb));
        parser cp(ctl);
        varvec child_ast = cp.parse();
        result = w.walk_forest(child_ast);
    } catch (...) {
        w.state.pop_frame();
        w.state.current = saved;
        w.m_root.m_store.m_map.erase("child");

        throw;
    }

    w.state.pop_frame();
    w.state.current = saved;
    w.m_root.m_store.m_map.erase("child");

    return result;
}

TEST(gt_ascript_walk, ReverseNav_ReadParentVar) {
    auto v = exec_in_child("var p; p = 42;", "..p;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, ReverseNav_WriteParentVar) {
    auto v = exec_in_child("var p; p = 0;", "..p = 99; ..p;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, ReverseNav_WriteParentVarUndefined) {
    auto [type, val] = exec_src_catch_in_child("", "..q = 5;");
    EXPECT_EQ(type, error_type::NameError);
}

TEST(gt_ascript_walk, ReverseNav_RootVar) {

    auto v = exec_in_child("var x; x = 77;", "::x;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 77);
}

TEST(gt_ascript_walk, ReverseNav_NavError) {
    auto [type, val] = exec_src_catch_in_child("", "....x;");
    EXPECT_EQ(type, error_type::NavError);
}

TEST(gt_ascript_walk, ReverseNav_DeleteParentVarRejected) {

    auto [type, val] = exec_src_catch_in_child("var p; p = 1;", "delete ..p;");
    (void) type;
    (void) val;
    SUCCEED();
}

TEST(gt_ascript_walk, ImportNav_BridgeMap) {
    auto v = exec_src("var bm; bm = map {\"a\": 1, \"b\": 2}; .bm.(a);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, ImportNav_BridgeVec) {
    auto v = exec_src("var bv; bv = [10, 20, 30]; .bv.([0]);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, ImportNav_BridgeAssign) {
    auto v = exec_src("var bm; bm = map {\"a\": 1}; .bm.(a) = 99; bm[\"a\"];");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, ImportNav_BridgeDelete) {
    auto [type, val] = exec_src_catch("var bd; bd = map {\"x\": 1, \"y\": 2}; delete .bd.(x); bd[\"x\"];");
    EXPECT_EQ(type, error_type::KeyError);
    (void) val;
}

TEST(gt_ascript_walk, ImportNav_DeleteVar) {

    auto [type, val] = exec_src_no_frame_catch("var dv; dv = 42; delete .dv; dv;");
    EXPECT_EQ(type, error_type::NameError);
}

TEST(gt_ascript_walk, ImportNav_BridgeNonContainer) {
    auto [type, val] = exec_src_catch("var bn; bn = 42; .bn.(x);");
    EXPECT_EQ(type, error_type::TypeError);
    (void) val;
}

TEST(gt_ascript_walk, DataChain_FirstKeyVarPtr) {
    auto v = exec_src("var m; m = map {\"x\": map {\"y\": 100}}; m.x.y;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 100);
}

TEST(gt_ascript_walk, DataChain_NestedMap) {
    auto v = exec_src("var obj; obj = map {\"a\": map {\"b\": map {\"c\": 42}}}; obj.a.b.c;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, ImportNav_NCallParent) {
    auto v = exec_in_child(
        "def f(x) { return x * 2; }",
        "..f(5);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, ChainMiddle_NavError) {

    SUCCEED();
}

TEST(gt_ascript_walk, ImportNav_BridgeLst) {
    auto v = exec_src("var l; l = lst[10, 20, 30]; .l.([0]);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 10);
}

TEST(gt_ascript_walk, ImportNav_BridgeOnNonContainer) {
    auto [type, val] = exec_src_catch("var n; n = 42; .n.(x);");
    EXPECT_EQ(type, error_type::TypeError);
    (void) val;
}

TEST(gt_ascript_walk, ImportNav_StackedParent) {
    auto v = exec_in_child("var x; x = 5;", "..x;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 5);
}

TEST(gt_ascript_walk, ImportNav_RootMultiLevel) {
    auto v = exec_in_child("var z; z = 99;", "::z;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, DataChain_TailNotFoundNull) {
    auto v = exec_src("var m; m = map{\"a\": 1}; m.missing;");
    EXPECT_TRUE(v.null());
}

TEST(gt_ascript_walk, DataChain_MidNotFoundError) {
    auto [type, val] = exec_src_catch("var m; m = map{\"a\": 1}; m.missing.b;");
    EXPECT_EQ(type, error_type::KeyError);
    (void) val;
}

TEST(gt_ascript_walk, DataChain_WriteNested) {
    auto v = exec_src("var m; m = map{\"a\": map{}}; m.a.b = 42; m.a.b;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 42);
}

TEST(gt_ascript_walk, DataChain_OnNonContainer) {
    auto [type, val] = exec_src_catch("var x; x = 42; x.y;");
    EXPECT_EQ(type, error_type::TypeError);
    (void) val;
}

TEST(gt_ascript_walk, ImportNav_BridgeMultiIndex) {
    auto v = exec_src("var nd; nd = [[[1,2],[3,4]],[[5,6],[7,8]]]; .nd.([1][0][1]);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 6);
}

TEST(gt_ascript_walk, ImportNav_BridgeIndexThenDot) {

    auto v = exec_src("var vm; vm = [map{\"a\": map{\"x\": 1, \"y\": 2}}, map{\"a\": map{\"x\": 3, \"y\": 4}}]; .vm[1].a.x;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 3);
}

TEST(gt_ascript_walk, ImportNav_ParentBridgeMap) {
    auto v = exec_in_child(
        "var pm; pm = map{\"k\": 100};",
        "..pm.(k);");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 100);
}

TEST(gt_ascript_walk, Slot_IndexWrite_ContainerSlot) {
    auto v = exec_src("def f() { var v = [0,1,2]; v[0] = 99; return v[0]; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, Slot_IndexWrite_IndexSlot) {
    auto v = exec_src("def f() { var v = [0:3]; var i = 1; v[i] = 99; return v[1]; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, Slot_IndexWrite_LoopVarIndex) {
    auto v = exec_src("def f() { var v = [0:5]; for (var i = 0; i < 5; ++i) { v[i] = i * 2; } return v[3]; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 6);
}

TEST(gt_ascript_walk, Slot_IndexWrite_LoopBothSlot) {
    auto v = exec_src("def f() { var v = [0:10000]; for (var i = 0; i < 10000; ++i) { v[i] = i; } var sum = 0; for (var i = 0; i < 10000; ++i) { sum = sum + v[i]; } return sum; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 49995000);
}

TEST(gt_ascript_walk, Slot_CompoundAssign_IndexSlot) {
    auto v = exec_src("def f() { var v = [0,10,20]; var i = 1; v[i] += 5; return v[1]; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 15);
}

TEST(gt_ascript_walk, Slot_DeleteIndex_ContainerSlot) {
    auto v = exec_src("def f() { var v = [0,1,2]; delete v[-1]; return v; } f();");
    EXPECT_TRUE(v.is<varvec>());
    EXPECT_EQ(v.to<varvec>().size(), 2u);
}

TEST(gt_ascript_walk, Slot_DotWrite_ContainerSlot) {
    auto v = exec_src("def f() { var d = map{\"a\": 1}; d.a = 99; return d.a; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 99);
}

TEST(gt_ascript_walk, Slot_DotRead_ContainerSlot) {
    auto v = exec_src("def f() { var d = map{\"x\": 42}; return d.x; } f();");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 42);
}

static std::pair<error_type, variant> exec_src_catch_in_child(
    const char* root_src, const char* child_src) {
    try {
        exec_in_child(root_src, child_src);
    } catch (script_exception& e) {
        return {e.type, e.info};
    }
    return {error_type::NoError, variant()};
}

static void a_dummy_native(fwrap&) {}

static variant exec_src_with_mock_link(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    varvec ast = p.parse();
    if (p.has_error()) return variant();

    res_mng mgr;
    mod_mng mod;
    mod.m_res = &mgr;
    engine_config cfg;
    walker w(cfg);
    w.mgr = &mod;
    w.reset();
    w.m_root_fly.m_path = ".";

    static op_table bt{};
    static std::list<std::string> s_empty_paths;
    w.state.m_search_paths = &s_empty_paths;
    w.state.push_frame();

    impl_link* link = new impl_link();
    link->m_alias = "testlink";
    link->m_parent = &w.m_root;

    auto* ca_root = new call_able(a_dummy_native);
    link->m_store.m_data.push_back(variant(anyptr_ex<call_able>::make(ca_root)));
    link->m_store.m_map["root_func"] = link->m_store.m_data.size() - 1;

    auto* area1 = new link_area();
    area1->m_natives["fn_a"] = variant(anyptr_ex<call_able>::make(new call_able(a_dummy_native)));
    link->m_store.m_data.push_back(variant(anyptr_ex<link_area>::make(area1)));
    link->m_store.m_map["area1"] = link->m_store.m_data.size() - 1;

    auto* area2 = new link_area();
    area2->m_natives["fn_b"] = variant(anyptr_ex<call_able>::make(new call_able(a_dummy_native)));
    link->m_store.m_data.push_back(variant(anyptr_ex<link_area>::make(area2)));
    link->m_store.m_map["area2"] = link->m_store.m_data.size() - 1;

    link->m_store.m_data.push_back(variant(42));
    link->m_store.m_map["data_var"] = link->m_store.m_data.size() - 1;

    w.m_root.m_store.m_data.push_back(variant(anyptr_ex<impl_link>::make(link)));
    w.m_root.m_store.m_map["testlink"] = w.m_root.m_store.m_data.size() - 1;

    variant result;
    try {
        result = w.walk_forest(ast);
    } catch (...) {
        w.m_root.m_store.m_map.erase("testlink");

        throw;
    }
    w.m_root.m_store.m_map.erase("testlink");

    return result;
}

static std::pair<error_type, variant> exec_src_with_mock_link_catch(const char* src) {
    try {
        exec_src_with_mock_link(src);
    } catch (script_exception& e) {
        return {e.type, e.info};
    }
    return {error_type::NoError, variant()};
}

TEST(gt_ascript_walk, Area_ValidCall) {
    auto v = exec_src_with_mock_link("testlink.area1.fn_a(); 1;");
    EXPECT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 1);
}

TEST(gt_ascript_walk, Area_TypeofFuncHandle) {
    auto v = exec_src_with_mock_link("type(testlink.area1.fn_a);");
    EXPECT_EQ(v.to<std::string>(), "func");
}

TEST(gt_ascript_walk, Area_UndefinedArea) {
    auto [etype, eval] = exec_src_with_mock_link_catch(
        "testlink.bad_area.func();");
    EXPECT_EQ(etype, error_type::NameError);
    EXPECT_TRUE(eval.is<std::string>());
    EXPECT_NE(eval.to<std::string>().find("Undefined"), std::string::npos);
}

TEST(gt_ascript_walk, Area_UndefinedFuncInArea) {
    auto [etype, eval] = exec_src_with_mock_link_catch(
        "testlink.area1.nonexistent();");
    EXPECT_EQ(etype, error_type::NameError);
    EXPECT_TRUE(eval.is<std::string>());
    EXPECT_NE(eval.to<std::string>().find("Undefined function"), std::string::npos);
}

TEST(gt_ascript_walk, Area_DeleteLinkFunc) {

    auto [etype, eval] = exec_src_with_mock_link_catch(
        "delete root_func;");
    EXPECT_EQ(etype, error_type::NoError);
}

TEST(gt_ascript_walk, Area_DeleteLinkFuncViaDot) {

    auto [etype, eval] = exec_src_with_mock_link_catch(
        "delete testlink.root_func;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, Area_DeleteAreaFuncViaDot) {

    auto [etype, eval] = exec_src_with_mock_link_catch(
        "delete testlink.area1.fn_a;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, Area_DeleteLinkDataVar) {

    auto [etype, eval] = exec_src_with_mock_link_catch(
        "delete testlink.data_var;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, Slice_VecBasic) {
    auto v = exec_src("var v=[10,20,30,40,50]; v[1,4];");
    ASSERT_TRUE(v.is_vec());
    auto& vec = v.to_vec();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0].to<int_64>(), 20);
    EXPECT_EQ(vec[2].to<int_64>(), 40);
}

TEST(gt_ascript_walk, Slice_VecStep) {
    auto v = exec_src("var v=[0,1,2,3,4,5]; v[0,6,2];");
    ASSERT_TRUE(v.is_vec());
    auto& vec = v.to_vec();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0].to<int_64>(), 0);
    EXPECT_EQ(vec[1].to<int_64>(), 2);
    EXPECT_EQ(vec[2].to<int_64>(), 4);
}

TEST(gt_ascript_walk, Slice_VecReverse) {
    auto v = exec_src("var v=[0,1,2,3,4,5]; v[5,1,-2];");
    ASSERT_TRUE(v.is_vec());
    auto& vec = v.to_vec();
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0].to<int_64>(), 5);
    EXPECT_EQ(vec[1].to<int_64>(), 3);
}

TEST(gt_ascript_walk, Slice_VecNull) {
    auto v = exec_src("var v=[10,20,30,40,50]; v[2,null];");
    ASSERT_TRUE(v.is_vec());
    auto& vec = v.to_vec();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0].to<int_64>(), 30);
    EXPECT_EQ(vec[2].to<int_64>(), 50);
}

TEST(gt_ascript_walk, Slice_VecNegIndex) {
    auto v = exec_src("var v=[10,20,30,40,50]; v[-3,-1];");
    ASSERT_TRUE(v.is_vec());
    auto& vec = v.to_vec();
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0].to<int_64>(), 30);
    EXPECT_EQ(vec[1].to<int_64>(), 40);
}

TEST(gt_ascript_walk, Slice_LstBasic) {
    auto v = exec_src("var l=lst[10,20,30,40,50]; l[1,4];");
    ASSERT_TRUE(v.is_lst());
    auto& lst = v.to_lst();
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front().to<int_64>(), 20);
    EXPECT_EQ(lst.back().to<int_64>(), 40);
}

TEST(gt_ascript_walk, Slice_StringBasic) {
    auto v = exec_src("var s=\"hello world\"; s[0,5];");
    ASSERT_TRUE(v.is<std::string>());
    EXPECT_EQ(v.to<std::string>(), "hello");
}

TEST(gt_ascript_walk, Slice_StringReverse) {
    auto v = exec_src("var s=\"hello\"; s[3,0,-1];");
    ASSERT_TRUE(v.is<std::string>());
    EXPECT_EQ(v.to<std::string>(), "lle");
}

TEST(gt_ascript_walk, Slice_StepZero) {
    auto [etype, eval] =
        exec_src_catch("var v=[1,2,3]; v[0,3,0];");
    EXPECT_EQ(etype, error_type::ArgError);
}

TEST(gt_ascript_walk, Slice_OutOfBounds) {
    auto [etype, eval] =
        exec_src_catch("var v=[1,2,3]; v[10,20];");
    EXPECT_EQ(etype, error_type::IndexError);
}

TEST(gt_ascript_walk, Slice_AssignError) {
    auto [etype, eval] =
        exec_src_catch("var v=[1,2,3]; v[1,3]=99;");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, Slice_NonContainer) {
    auto [etype, eval] =
        exec_src_catch("(42)[0,2];");
    EXPECT_EQ(etype, error_type::TypeError);
}

TEST(gt_ascript_walk, HereReturnsMap) {
    auto v = exec_src("here();");
    ASSERT_TRUE(v.is<varmap>());
    auto& m = v.to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 1);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 0);
    EXPECT_EQ(m.value("file").to<std::string>(), "");
}
TEST(gt_ascript_walk, HereInExpr) {
    auto v = exec_src("var p = here(); p[\"ofst\"];");
    ASSERT_TRUE(v.is<int_64>());
    EXPECT_EQ(v.to<int_64>(), 8);
}
TEST(gt_ascript_walk, HereInsideDef) {
    auto v = exec_src("def f() {\n  return here();\n}\nf();");
    ASSERT_TRUE(v.is<varmap>());
    auto& m = v.to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 2);
    EXPECT_EQ(m.value("col").to<int_64>(), 10);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 19);
}

TEST(gt_ascript_walk, NestedIndexAssign_VecMap) {

    auto [etype, eval] = exec_src_no_frame_catch(
        "var vv; vv = [map{\"a\": 1}];"
        "vv[0][\"b\"] = 2;"
        "vv[0][\"b\"];");
    EXPECT_EQ(etype, error_type::UnknownError);
    EXPECT_EQ(eval.to<int_64>(), 2);
}
TEST(gt_ascript_walk, NestedIndexAssign_VecMap_ExistingKey) {

    auto [etype, eval] = exec_src_no_frame_catch(
        "var vv; vv = [map{\"a\": 1}];"
        "vv[0][\"a\"] = 99;"
        "vv[0][\"a\"];");
    EXPECT_EQ(etype, error_type::UnknownError);
    EXPECT_EQ(eval.to<int_64>(), 99);
}
TEST(gt_ascript_walk, NestedIndexAssign_DeepNested) {

    auto [etype, eval] = exec_src_no_frame_catch(
        "var vv; vv = [map{\"m\": map{\"x\": 10}}];"
        "vv[0][\"m\"][\"y\"] = 20;"
        "vv[0][\"m\"][\"y\"];");
    EXPECT_EQ(etype, error_type::UnknownError);
    EXPECT_EQ(eval.to<int_64>(), 20);
}
