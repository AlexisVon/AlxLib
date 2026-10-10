/*****************************************************************/ /**
 * \file   gt_ascript_parse.cpp
 * \brief  Parser unit tests — full grammar coverage
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_lex.h"
#include "ascript_parse.h"
#include <cstdio>
#include <fstream>
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

static varvec parse_src_ext(const char* src, const std::unordered_map<std::string, native_func>& ext) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl, nullptr, "", {}, 0, &ext);
    return p.parse();
}

static bool parse_ext_has_error(const char* src, const std::unordered_map<std::string, native_func>& ext) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl, nullptr, "", {}, 0, &ext);
    p.parse();
    return p.has_error();
}

static const variant* nav(const varvec& root, const std::vector<size_t>& path) {
    const variant* cur = nullptr;
    const varvec* arr = &root;
    for (size_t idx : path) {
        if (idx >= arr->size()) return nullptr;
        cur = &(*arr)[idx];
        if (cur->is_vec()) arr = &cur->to<varvec>();
    }
    return cur;
}

static bool head_is(const varvec& v, op_enum expected) {
    return v.size() > 0 && v[0].is<OPTYPE>() && static_cast<op_enum>(v[0].to<OPTYPE>()) == expected;
}

TEST(gt_ascript_parse, IntLiteral) {
    auto ast = parse_src("123;");
    ASSERT_GE(ast.size(), 2u);
    ASSERT_TRUE(ast[1].is_vec());
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 123);
}

TEST(gt_ascript_parse, FloatLiteral) {
    auto ast = parse_src("3.14;");
    EXPECT_DOUBLE_EQ(ast[1].to<varvec>()[0].to<double>(), 3.14);
}

TEST(gt_ascript_parse, CharLiteral) {
    auto ast = parse_src("'c';");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 99);
}

TEST(gt_ascript_parse, CharLiteralCodePoints) {
    EXPECT_EQ(parse_src("'\\x41';")[1].to<varvec>()[0].to<int_64>(), 65);
    EXPECT_EQ(parse_src("'é';")[1].to<varvec>()[0].to<int_64>(), 233);
    EXPECT_EQ(parse_src("'你';")[1].to<varvec>()[0].to<int_64>(), 20320);
    EXPECT_EQ(parse_src("'𝄞';")[1].to<varvec>()[0].to<int_64>(), 119070);
}

TEST(gt_ascript_parse, StringLiteral) {
    auto ast = parse_src("\"hello\";");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<std::string>(), "hello");
}

TEST(gt_ascript_parse, BoolTrue) {
    auto ast = parse_src("true;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<bool>(), true);
}

TEST(gt_ascript_parse, BoolFalse) {
    auto ast = parse_src("false;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<bool>(), false);
}

TEST(gt_ascript_parse, VarDecl) {
    auto ast = parse_src("var x;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "x");
    EXPECT_FALSE(v[2].is_vec());
    EXPECT_EQ(v.size(), 3u);
}

TEST(gt_ascript_parse, PrecedenceAddMul) {
    auto ast = parse_src("1 + 2 * 3;");

    auto& add = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(add, O_ADD));
    EXPECT_EQ(add[1].to<varvec>()[0].to<int_64>(), 1);
    EXPECT_TRUE(head_is(add[2].to<varvec>(), O_MUL));
    EXPECT_EQ(add[2].to<varvec>()[1].to<varvec>()[0].to<int_64>(), 2);
    EXPECT_EQ(add[2].to<varvec>()[2].to<varvec>()[0].to<int_64>(), 3);
}

TEST(gt_ascript_parse, PrecedenceMulAdd) {
    auto ast = parse_src("1 * 2 + 3;");

    auto& add = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(add, O_ADD));
    EXPECT_TRUE(head_is(add[1].to<varvec>(), O_MUL));
    EXPECT_EQ(add[2].to<varvec>()[0].to<int_64>(), 3);
}

TEST(gt_ascript_parse, IfStmt) {
    auto ast = parse_src("if (x) {}");
    auto& ifn = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(ifn, O_IF));
    EXPECT_TRUE(head_is(ifn[2].to<varvec>(), O_BLOCK));
    EXPECT_TRUE(head_is(ifn[3].to<varvec>(), O_BLOCK));
}

TEST(gt_ascript_parse, WhileStmt) {
    auto ast = parse_src("while (x) {}");
    auto& w = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(w, O_WHILE));
}

TEST(gt_ascript_parse, ForStmt) {
    auto ast = parse_src("for (;;) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));
}
TEST(gt_ascript_parse, ForFull) {

    auto ast = parse_src("for (i = 0; i < 10; ++i) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));
}
TEST(gt_ascript_parse, ForVarInit) {
    auto ast = parse_src("for (var i = 0; i < 10; ++i) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));
    EXPECT_TRUE(head_is(f[1].to<varvec>(), O_VAR));
}
TEST(gt_ascript_parse, ForVarIn) {
    auto ast = parse_src("for (var x : arr) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOREACH));
    auto& target = f[1].to<varvec>();
    EXPECT_TRUE(head_is(target, O_VAR));
}
TEST(gt_ascript_parse, ElseIfChain) {
    auto ast = parse_src("if (a) {} else if (b) {} else {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_IF));
}
TEST(gt_ascript_parse, WhileTrue) {
    auto ast = parse_src("while (true) {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_WHILE));
}

TEST(gt_ascript_parse, DefStmt) {
    auto ast = parse_src("def f() {}");
    auto& d = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(d, O_DEF));
    EXPECT_EQ(d[1].to<std::string>(), "f");
    EXPECT_TRUE(head_is(d[4].to<varvec>(), O_BLOCK));
}

TEST(gt_ascript_parse, DefWithParams) {
    auto ast = parse_src("def f(a, b = 1) {}");
    auto& d = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(d, O_DEF));
    EXPECT_EQ(d[1].to<std::string>(), "f");
    EXPECT_EQ(d[2].to<varvec>()[0].to<std::string>(), "a");
    EXPECT_EQ(d[2].to<varvec>()[1].to<std::string>(), "b");

    EXPECT_FALSE(d[3].to<varmap>().contain("a"));
    EXPECT_TRUE(d[3].to<varmap>().contain("b"));
}

TEST(gt_ascript_parse, SwitchStmt) {
    auto ast = parse_src("switch (x) { case 1: break; default: break; }");
    auto& sw = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(sw, O_SWITCH));
}

TEST(gt_ascript_parse, TryStmt) {
    auto ast = parse_src("try {} catch (e) {}");
    auto& t = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(t, O_TRY));
    auto& c = t[2].to<varvec>();
    EXPECT_TRUE(head_is(c, O_CATCH));
    EXPECT_EQ(c[1].to<std::string>(), "e");
}

TEST(gt_ascript_parse, ReturnVoid) {
    auto ast = parse_src("return;");
    auto& r = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(r, O_RETURN));
    EXPECT_EQ(r.size(), 1u);
}

TEST(gt_ascript_parse, ReturnExpr) {
    auto ast = parse_src("return 1;");
    auto& r = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(r, O_RETURN));
    EXPECT_EQ(r[1].to<varvec>()[0].to<int_64>(), 1);
}

TEST(gt_ascript_parse, BreakStmt) {
    auto ast = parse_src("break;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_BREAK));
}

TEST(gt_ascript_parse, ContinueStmt) {
    auto ast = parse_src("continue;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_CONTINUE));
}

TEST(gt_ascript_parse, ThrowStmt) {
    auto ast = parse_src("throw x;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_THROW));
}

TEST(gt_ascript_parse, ImportStmt) {

    auto ast = parse_src("import \"/tmp/alx_import_test.axc\" as test;");
    EXPECT_TRUE(head_is(ast, O_PROGRAM));

    bool found_import = false;
    for (size_t i = 0; i < ast.size(); i++) {
        if (ast[i].is_vec() && head_is(ast[i].to<varvec>(), O_IMPORT)) {
            auto& imp = ast[i].to<varvec>();
            EXPECT_EQ(imp[1].to<std::string>(), "/tmp/alx_import_test.axc");
            EXPECT_EQ(imp[2].to<std::string>(), "test");
            found_import = true;
        }
    }
    EXPECT_TRUE(found_import) << "Should find @import node with path";

    auto ast2 = parse_src("import \"/tmp/alx_import_test.axc\" as a; import \"/tmp/alx_import_test.axc\" as b;");
    int imp_count = 0;
    for (size_t i = 0; i < ast2.size(); i++)
        if (ast2[i].is_vec() && head_is(ast2[i].to<varvec>(), O_IMPORT)) imp_count++;
    EXPECT_EQ(imp_count, 2) << "Both imports kept as-is at parse time";
}

TEST(gt_ascript_parse, ImportSearchPath) {

    alx::bytes b("import \"alx_search_mod.axc\" as mod;");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl, {}, "", {"/tmp"});
    auto ast = p.parse();

    bool found = false;
    for (size_t i = 0; i < ast.size(); i++)
        if (ast[i].is_vec() && head_is(ast[i].to<varvec>(), O_IMPORT)) found = true;
    EXPECT_TRUE(found) << "Should find @import node (resolve happens at compile time)";
}

TEST(gt_ascript_parse, ListLit) {
    auto ast = parse_src("[1, 2, 3];");
    auto& lst = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(lst, O_VEC));
    EXPECT_EQ(lst.size(), 4u);
    auto& elems = lst[1].to<varvec>();
    EXPECT_EQ(elems[0].to<varvec>()[0].to<int_64>(), 1);
    EXPECT_EQ(elems[1].to<varvec>()[0].to<int_64>(), 2);
    EXPECT_EQ(elems[2].to<varvec>()[0].to<int_64>(), 3);
}

TEST(gt_ascript_parse, ListFill) {
    auto ast = parse_src("[1: 3];");
    auto& lf = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(lf, O_VEC));
    EXPECT_EQ(lf.size(), 4u);
    EXPECT_TRUE(lf[1].to<varvec>().empty());
    EXPECT_EQ(lf[2].to<varvec>()[0].to<int_64>(), 1);
    EXPECT_EQ(lf[3].to<varvec>()[0].to<int_64>(), 3);
}

TEST(gt_ascript_parse, DictLit) {
    auto ast = parse_src("map{\"key\": 1};");
    auto& d = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(d, O_MAP));

    EXPECT_EQ(d[1].to<varvec>()[0].to<std::string>(), "key");
    EXPECT_EQ(d[2].to<varvec>()[0].to<int_64>(), 1);
}

TEST(gt_ascript_parse, Ternary) {
    auto ast = parse_src("a ? b : c;");
    auto& t = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(t, O_TERNARY));
}

TEST(gt_ascript_parse, Comma) {
    auto ast = parse_src("a, b;");
    auto& c = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(c, O_COMMA));
}

TEST(gt_ascript_parse, MemberAccess) {
    auto ast = parse_src("a.b;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));
    EXPECT_EQ(dot[2].to<std::string>(), "b");
}

TEST(gt_ascript_parse, IndexAccess) {
    auto ast = parse_src("a[0];");
    auto& idx = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(idx, O_INDEX));
}

TEST(gt_ascript_parse, CallExpr) {
    auto ast = parse_src("f(1, 2);");
    auto& call = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(call, O_CALL));
    EXPECT_EQ(call[2].to<varvec>()[0].to<int_64>(), 1);
    EXPECT_EQ(call[3].to<varvec>()[0].to<int_64>(), 2);
}

TEST(gt_ascript_parse, UnpackVec) {
    auto ast = parse_src("f(v[]);");
    auto& call = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(call, O_CALL));

    auto& arg = call[2].to<varvec>();
    EXPECT_TRUE(head_is(arg, O_UNPACK));
    EXPECT_TRUE(head_is(arg[1].to<varvec>(), O_LOAD));
    EXPECT_EQ(arg[1].to<varvec>()[1].to<std::string>(), "v");
}

TEST(gt_ascript_parse, UnpackMapLiteral) {
    auto ast = parse_src("f(map{\"k\": 1}[]);");
    auto& call = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(call, O_CALL));
    auto& arg = call[2].to<varvec>();
    EXPECT_TRUE(head_is(arg, O_UNPACK));
    EXPECT_TRUE(head_is(arg[1].to<varvec>(), O_MAP));
}

TEST(gt_ascript_parse, UnpackMixedPosAndSpread) {
    auto ast = parse_src("f(10, v[]);");
    auto& call = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(call, O_CALL));
    EXPECT_EQ(call[2].to<varvec>()[0].to<int_64>(), 10);
    EXPECT_TRUE(head_is(call[3].to<varvec>(), O_UNPACK));
}

TEST(gt_ascript_parse, Assign) {
    auto ast = parse_src("x = 1;");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_STORE));
    EXPECT_EQ(a[2].to<varvec>()[0].to<int_64>(), 1);
}

TEST(gt_ascript_parse, AssignAdd) {
    auto ast = parse_src("x += 1;");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_ASS_ADD));
    EXPECT_TRUE(head_is(a[1].to<varvec>(), O_LOAD));
    EXPECT_EQ(a[1].to<varvec>()[1].to<std::string>(), "x");
}

TEST(gt_ascript_parse, FullSample) {
    auto ast = parse_src(
        "var x; x = 1;\n"
        "if (x > 0) { x = x + 1; }\n");
    EXPECT_TRUE(head_is(ast, O_PROGRAM));
    EXPECT_TRUE(ast.size() >= 3u);
}

TEST(gt_ascript_parse, ErrorRecovery) {
    auto ast = parse_src("var x; var y; y = 1;");
    EXPECT_TRUE(head_is(ast, O_PROGRAM));

    int decl_count = 0;
    for (size_t i = 0; i < ast.size(); i++) {
        if (ast[i].is_vec()) {
            auto& v = ast[i].to<varvec>();
            if (head_is(v, O_VAR) || head_is(v, O_STORE)) decl_count++;
        }
    }
    EXPECT_GE(decl_count, 1) << "Should recover and parse at least one var decl";
}

TEST(gt_ascript_parse, ForInWithVar) {
    auto ast = parse_src("for (x : arr) {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOREACH));
}

TEST(gt_ascript_parse, ForInExpr) {
    auto ast = parse_src("for (a : b) {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOREACH));
}

TEST(gt_ascript_parse, LinkStmt) {
    auto ast = parse_src("link \"lib\" as mylib;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_LINK));
}
TEST(gt_ascript_parse, PreDec) {
    auto ast = parse_src("--a;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_PRE_DEC));
}
TEST(gt_ascript_parse, PostDec) {
    auto ast = parse_src("a--;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_POST_DEC));
}
TEST(gt_ascript_parse, BitNot) {
    auto ast = parse_src("~a;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_BIT_NEG));
}
TEST(gt_ascript_parse, UnaryMinus) {
    auto ast = parse_src("-a;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_UMINUS));
}
TEST(gt_ascript_parse, BitXorExpr) {
    auto ast = parse_src("a ^ b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_BIT_XOR));
}
TEST(gt_ascript_parse, LshiftExpr) {
    auto ast = parse_src("a << b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_LSHIFT));
}
TEST(gt_ascript_parse, RshiftExpr) {
    auto ast = parse_src("a >> b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_RSHIFT));
}
TEST(gt_ascript_parse, GeExpr) {
    auto ast = parse_src("a >= b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_GE));
}
TEST(gt_ascript_parse, HexLiteral) {
    auto ast = parse_src("0xFF;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 255);
}
TEST(gt_ascript_parse, EmptyDict) {
    auto ast = parse_src("map{};");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_MAP));
}
TEST(gt_ascript_parse, AssignLshift) {
    auto ast = parse_src("x <<= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_LSHIFT));
}
TEST(gt_ascript_parse, NullLiteral) {
    auto ast = parse_src("null;");
    EXPECT_FALSE(ast[1].to<varvec>()[0].is<int_64>());
}
TEST(gt_ascript_parse, NestedIf) {
    auto ast = parse_src("if (a) { if (b) {} }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_IF));
}
TEST(gt_ascript_parse, ListFillEmpty) {
    auto ast = parse_src("[: 5];");
    auto& lf = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(lf, O_VEC));
    EXPECT_EQ(lf.size(), 4u);
    EXPECT_TRUE(lf[1].to<varvec>().empty());
}
TEST(gt_ascript_parse, SwitchWithCases) {
    auto ast = parse_src("switch (x) { case 1: case 2: break; }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_SWITCH));
}

TEST(gt_ascript_parse, BitOrExpr) {
    auto ast = parse_src("a | b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_BIT_OR));
}
TEST(gt_ascript_parse, BitAndExpr) {
    auto ast = parse_src("a & b;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_BIT_AND));
}
TEST(gt_ascript_parse, UnaryPlus) {
    auto ast = parse_src("+a;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_UPLUS));
}

TEST(gt_ascript_parse, AssSub) {
    auto ast = parse_src("x -= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_SUB));
}
TEST(gt_ascript_parse, AssMul) {
    auto ast = parse_src("x *= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_MUL));
}
TEST(gt_ascript_parse, AssDiv) {
    auto ast = parse_src("x /= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_DIV));
}
TEST(gt_ascript_parse, AssMod) {
    auto ast = parse_src("x %= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_MOD));
}
TEST(gt_ascript_parse, AssRshift) {
    auto ast = parse_src("x >>= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_RSHIFT));
}
TEST(gt_ascript_parse, AssBitAnd) {
    auto ast = parse_src("x &= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_BIT_AND));
}
TEST(gt_ascript_parse, AssBitOr) {
    auto ast = parse_src("x |= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_BIT_OR));
}
TEST(gt_ascript_parse, AssBitXor) {
    auto ast = parse_src("x ^= 1;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_BIT_XOR));
}

TEST(gt_ascript_parse, AssPlainSameShape) {
    auto ast = parse_src("x = x + 1;");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_STORE));
    EXPECT_TRUE(head_is(a[2].to<varvec>(), O_ADD));
}

TEST(gt_ascript_parse, AssPlainReversedOperand) {
    auto ast = parse_src("x = y + x;");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_STORE));
}

TEST(gt_ascript_parse, AssIndexTarget) {
    auto ast = parse_src("x[0] += 1;");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_ASS_ADD));
    EXPECT_TRUE(head_is(a[1].to<varvec>(), O_INDEX));
}

TEST(gt_ascript_parse, AssImpureRhs) {
    auto ast = parse_src("x += foo();");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ASS_ADD));
}

TEST(gt_ascript_parse, AssPureBinaryRhs) {
    auto ast = parse_src("x += y + z;");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_ASS_ADD));
    EXPECT_TRUE(head_is(a[2].to<varvec>(), O_ADD));
}

TEST(gt_ascript_parse, IfInWhile) {
    auto ast = parse_src("while (x) { if (y) { break; } }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_WHILE));
}
TEST(gt_ascript_parse, WhileInFor) {
    auto ast = parse_src("for (;;) { while (x) { break; } }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOR));
}
TEST(gt_ascript_parse, SwitchInIf) {
    auto ast = parse_src("if (x) { switch (y) { case 1: break; } }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_IF));
}
TEST(gt_ascript_parse, TryInWhile) {
    auto ast = parse_src("while (x) { try {} catch (e) {} }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_WHILE));
}
TEST(gt_ascript_parse, ForInIf) {
    auto ast = parse_src("if (x) { for (;;) {} }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_IF));
}
TEST(gt_ascript_parse, DefInDef) {
    auto ast = parse_src("def outer() { def inner() {} }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_DEF));
}
TEST(gt_ascript_parse, OctalLiteral) {
    auto ast = parse_src("0o77;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 63);
}
TEST(gt_ascript_parse, BinaryLiteral) {
    auto ast = parse_src("0b1010;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 10);
}

TEST(gt_ascript_parse, ParenExpr) {
    auto ast = parse_src("(1 + 2);");

    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_ADD));
}

TEST(gt_ascript_parse, EmptyStmt) {
    auto ast = parse_src("if (true) { ; }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_IF));
}

TEST(gt_ascript_parse, CallNoArgs) {
    auto ast = parse_src("f();");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_CALL));
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_CALL));
    EXPECT_EQ(inner.size(), 2u);
}

TEST(gt_ascript_parse, ListSingleElement) {
    auto ast = parse_src("[42];");
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_VEC));
    EXPECT_EQ(inner.size(), 4u);
}

TEST(gt_ascript_parse, TrailingCommaInList) {
    alx::bytes b("[1, 2,];");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    auto ast = p.parse();
    EXPECT_TRUE(p.has_error());
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_VEC));
}

TEST(gt_ascript_parse, DictMultipleKeys) {
    auto ast = parse_src("map{\"a\": 1, \"b\": 2, \"c\": 3};");
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_MAP));
    EXPECT_EQ(inner.size(), 7u);
}

TEST(gt_ascript_parse, TrailingCommaInDict) {
    auto ast = parse_src("map{\"a\": 1,};");

    (void) ast;
}

TEST(gt_ascript_parse, SwitchOnlyDefault) {
    auto ast = parse_src("switch (x) { default: break; }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_SWITCH));
    auto& sw = ast[1].to<varvec>();
    EXPECT_EQ(sw.size(), 3u);
}

TEST(gt_ascript_parse, ForExprInit) {

    auto ast = parse_src("for (f(); x < 10; ++x) {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOR));
}

TEST(gt_ascript_parse, ForNoInit) {
    auto ast = parse_src("for (; i < 10; ++i) {}");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOR));
}

TEST(gt_ascript_parse, ChainedMemberAccess) {
    auto ast = parse_src("a.b.c;");
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_DOT));
    EXPECT_EQ(inner.size(), 4u);
    EXPECT_TRUE(inner[1].is<std::string>());
    EXPECT_EQ(inner[1].to<std::string>(), "a");
    EXPECT_EQ(inner[2].to<std::string>(), "b");
    EXPECT_EQ(inner[3].to<std::string>(), "c");
}

TEST(gt_ascript_parse, ChainedIndex) {
    auto ast = parse_src("a[0][1];");
    auto& inner = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_INDEX));
}

TEST(gt_ascript_parse, DefNonDefaultAfterDefault) {
    alx::bytes b("def f(a=1, b) {}");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    auto ast = p.parse();

    EXPECT_FALSE(p.has_error());
}

TEST(gt_ascript_parse, ImportFileNotFound) {

    alx::bytes b("import \"nonexistent_file.axc\" as dummy;");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    auto ast = p.parse();
    EXPECT_FALSE(p.has_error()) << "File-not-found not detected at parse time";

    bool found = false;
    for (size_t i = 0; i < ast.size(); i++)
        if (ast[i].is_vec() && head_is(ast[i].to<varvec>(), O_IMPORT)) found = true;
    EXPECT_TRUE(found) << "Should keep @import node for runtime fallback";
}

TEST(gt_ascript_parse, CharEscapedNewline) {
    auto ast = parse_src("'\\n';");
    auto& inner = ast[1].to<varvec>();
    EXPECT_EQ(inner[0].to<int_64>(), 10);
}

TEST(gt_ascript_parse, StringEscAfterComment) {

    auto ast = parse_src("/* c */ \"a\\nb\";");
    auto& inner = ast[1].to<varvec>();
    EXPECT_EQ(inner[0].to<std::string>(), "a\nb");
}

TEST(gt_ascript_parse, UppercaseHexLiteral) {
    auto ast = parse_src("0XFF;");
    EXPECT_EQ(ast[1].to<varvec>()[0].to<int_64>(), 255);
}

TEST(gt_ascript_parse, BlockInsideFor) {

    auto ast = parse_src("for (i = 0; i < 5; ++i) { { if (i == 2) { break; } } }");

    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOR));
}
TEST(gt_ascript_parse, BlockInsideForContinue) {

    auto ast = parse_src("for (i = 0; i < 5; ++i) { { continue; } }");
    EXPECT_TRUE(head_is(ast[1].to<varvec>(), O_FOR));
}

TEST(gt_ascript_parse, VarDeclMulti) {
    auto ast = parse_src("var a, b, c;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "a");
    EXPECT_FALSE(v[2].is_vec());
    EXPECT_EQ(v[3].to<std::string>(), "b");
    EXPECT_FALSE(v[4].is_vec());
    EXPECT_EQ(v[5].to<std::string>(), "c");
    EXPECT_FALSE(v[6].is_vec());
    EXPECT_EQ(v.size(), 7u);
}

TEST(gt_ascript_parse, VarDeclWithInit) {
    auto ast = parse_src("var x = 1;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "x");
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_EQ(v.size(), 3u);
}

TEST(gt_ascript_parse, VarDeclMixed) {
    auto ast = parse_src("var a = 1, b, c = 3;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "a");
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_EQ(v[3].to<std::string>(), "b");
    EXPECT_FALSE(v[4].is_vec());
    EXPECT_EQ(v[5].to<std::string>(), "c");
    ASSERT_TRUE(v[6].is_vec());
    EXPECT_EQ(v.size(), 7u);
}

TEST(gt_ascript_parse, AssConcat) {
    auto ast = parse_src("x += \"suffix\";");
    auto& a = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(a, O_ASS_ADD));
}

TEST(gt_ascript_parse, AssChain) {
    auto ast = parse_src("a += b += 2;");

    auto& outer = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(outer, O_ASS_ADD));
    EXPECT_TRUE(head_is(outer[1].to<varvec>(), O_LOAD));
    EXPECT_EQ(outer[1].to<varvec>()[1].to<std::string>(), "a");

    auto& inner = outer[2].to<varvec>();
    EXPECT_TRUE(head_is(inner, O_ASS_ADD));
    EXPECT_TRUE(head_is(inner[1].to<varvec>(), O_LOAD));
    EXPECT_EQ(inner[1].to<varvec>()[1].to<std::string>(), "b");
}

TEST(gt_ascript_parse, ReverseNav_ParentVar) {
    auto ast = parse_src("..x;");

    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_PARENT);
    EXPECT_TRUE(dot[2].is<std::string>());
    EXPECT_EQ(dot[2].to<std::string>(), "x");
}

TEST(gt_ascript_parse, ReverseNav_ParentCall) {
    auto ast = parse_src("..f(1);");
    auto& ncall = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(ncall, O_NCALL));
    auto& keys = ncall[1].to<varvec>();
    EXPECT_EQ(keys.size(), 2u);
    EXPECT_TRUE(keys[0].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(keys[0].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(keys[1].to<std::string>(), "f");
}

TEST(gt_ascript_parse, ReverseNav_RootVar) {
    auto ast = parse_src("::x;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_ROOT);
    EXPECT_TRUE(dot[2].is<std::string>());
}

TEST(gt_ascript_parse, ReverseNav_DoubleParent) {
    auto ast = parse_src("....x;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_PARENT);
    EXPECT_TRUE(dot[2].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[2].to<OPTYPE>()), O_PARENT);
    EXPECT_TRUE(dot[3].is<std::string>());
    EXPECT_EQ(dot[3].to<std::string>(), "x");
}

TEST(gt_ascript_parse, ReverseNav_CWD) {
    auto ast = parse_src(".a.b;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_CURRENT);
    EXPECT_TRUE(dot[2].is<std::string>());
    EXPECT_EQ(dot[2].to<std::string>(), "a");
    EXPECT_EQ(dot[3].to<std::string>(), "b");
}

TEST(gt_ascript_parse, ReverseNav_NestedNS) {
    auto ast = parse_src("..a.b.f(1, 2);");
    auto& ncall = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(ncall, O_NCALL));
    auto& keys = ncall[1].to<varvec>();
    EXPECT_EQ(keys.size(), 4u);
    EXPECT_TRUE(keys[0].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(keys[0].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(keys[1].to<std::string>(), "a");
    EXPECT_EQ(keys[2].to<std::string>(), "b");
    EXPECT_EQ(keys[3].to<std::string>(), "f");
}

TEST(gt_ascript_parse, ReverseNav_RootCall) {
    auto ast = parse_src("::a.b.f(1);");
    auto& ncall = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(ncall, O_NCALL));
    auto& keys = ncall[1].to<varvec>();
    EXPECT_TRUE(keys[0].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(keys[0].to<OPTYPE>()), O_ROOT);
    EXPECT_EQ(keys[1].to<std::string>(), "a");
    EXPECT_EQ(keys[2].to<std::string>(), "b");
    EXPECT_EQ(keys[3].to<std::string>(), "f");
}

TEST(gt_ascript_parse, ReverseNav_BareDotError) {
    auto ast = parse_src(".;");

    EXPECT_TRUE(ast.size() >= 1);
}

TEST(gt_ascript_parse, ReverseNav_MixedPrefixError) {
    auto ast = parse_src("..::x;");

    EXPECT_TRUE(ast.size() >= 1);
}

TEST(gt_ascript_parse, Bridge_Keys) {
    auto ast = parse_src(".a.(k1.k2);");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 5u);
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_CURRENT);
    EXPECT_EQ(dot[2].to<std::string>(), "a");
    EXPECT_EQ(dot[3].to<std::string>(), "k1");
    EXPECT_EQ(dot[4].to<std::string>(), "k2");
}

TEST(gt_ascript_parse, Bridge_Index) {
    auto ast = parse_src("..a.b.([0][1]);");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 6u);
    EXPECT_TRUE(dot[1].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[1].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(dot[2].to<std::string>(), "a");
    EXPECT_EQ(dot[3].to<std::string>(), "b");
    EXPECT_TRUE(dot[4].is<int_64>());
    EXPECT_EQ(dot[4].to<int_64>(), 0);
    EXPECT_TRUE(dot[5].is<int_64>());
    EXPECT_EQ(dot[5].to<int_64>(), 1);
}

TEST(gt_ascript_parse, Bridge_Mixed) {
    auto ast = parse_src(".obj.(k1[0]k2);");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 6u);
    EXPECT_EQ(dot[3].to<std::string>(), "k1");
    EXPECT_TRUE(dot[4].is<int_64>());
    EXPECT_EQ(dot[4].to<int_64>(), 0);
    EXPECT_EQ(dot[5].to<std::string>(), "k2");
}

TEST(gt_ascript_parse, ChainMiddle_Dot) {
    auto ast = parse_src("a..b;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 4u);
    EXPECT_EQ(dot[1].to<std::string>(), "a");
    EXPECT_TRUE(dot[2].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[2].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(dot[3].to<std::string>(), "b");
}

TEST(gt_ascript_parse, ChainMiddle_MultiParent) {
    auto ast = parse_src("a....b;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 5u);
    EXPECT_EQ(dot[1].to<std::string>(), "a");
    EXPECT_TRUE(dot[2].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[2].to<OPTYPE>()), O_PARENT);
    EXPECT_TRUE(dot[3].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[3].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(dot[4].to<std::string>(), "b");
}

TEST(gt_ascript_parse, ChainMiddle_MultiSegment) {
    auto ast = parse_src("a..b..c;");
    auto& dot = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(dot, O_DOT));

    EXPECT_EQ(dot.size(), 6u);
    EXPECT_EQ(dot[1].to<std::string>(), "a");
    EXPECT_TRUE(dot[2].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[2].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(dot[3].to<std::string>(), "b");
    EXPECT_TRUE(dot[4].is<OPTYPE>());
    EXPECT_EQ(static_cast<op_enum>(dot[4].to<OPTYPE>()), O_PARENT);
    EXPECT_EQ(dot[5].to<std::string>(), "c");
}

static bool parse_has_error(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    p.parse();
    return p.has_error();
}
TEST(gt_ascript_parse, DotChainIndex_LiteralsOnlyWithSliceException) {
    // a variable index inside a dot chain is a parse error; the comma form is the exception
    EXPECT_TRUE(parse_has_error("var o; o.s[i];"));
    EXPECT_TRUE(parse_has_error("var o; o.s[i + 1];"));
    EXPECT_FALSE(parse_has_error("var o; o.s[0];"));
    EXPECT_FALSE(parse_has_error("var o; o.s[-1];"));
    EXPECT_FALSE(parse_has_error("var o; o.v[1, 3];")) << "a slice through a chain, bounds may be expressions";
    EXPECT_FALSE(parse_has_error("var o; o.v[i, j];"));
    EXPECT_TRUE(parse_has_error("var o; o.v[1, 3].x;")) << "a member of a slice result is not navigable";
}


TEST(gt_ascript_parse, NavReject_BareIndex) {
    EXPECT_TRUE(parse_has_error("..[0];"));
}

TEST(gt_ascript_parse, NavReject_IndexInChain) {

    EXPECT_FALSE(parse_has_error("..x[0];"));
}

TEST(gt_ascript_parse, NavReject_RootBareIndex) {
    EXPECT_TRUE(parse_has_error("::[0];"));
}

TEST(gt_ascript_parse, NavReject_CurrentBareIndex) {
    EXPECT_TRUE(parse_has_error(".[0];"));
}

TEST(gt_ascript_parse, NavReject_DoubleParentBareIndex) {
    EXPECT_TRUE(parse_has_error("....[0];"));
}

TEST(gt_ascript_parse, SliceParse_2Arg) {
    auto ast = parse_src("a[1,3];");
    auto& slice = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(slice, O_SLICE));
    EXPECT_TRUE(slice[2].is<int_64>());
    EXPECT_EQ(slice[2].to<int_64>(), 1);
    EXPECT_TRUE(slice[3].is<int_64>());
    EXPECT_EQ(slice[3].to<int_64>(), 3);
    EXPECT_TRUE(slice[4].is<int_64>());
    EXPECT_EQ(slice[4].to<int_64>(), 1);
}

TEST(gt_ascript_parse, SliceParse_3Arg) {
    auto ast = parse_src("a[1,5,2];");
    auto& slice = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(slice, O_SLICE));
    EXPECT_EQ(slice[4].to<int_64>(), 2);
}

TEST(gt_ascript_parse, SliceParse_SingleIndex) {
    auto ast = parse_src("a[0];");
    auto& idx = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(idx, O_INDEX));
}

TEST(gt_ascript_parse, Excall_Parse) {
    std::unordered_map<std::string, native_func> ext = {{"foo", nullptr}};
    auto ast = parse_src_ext("$foo(1, 2);", ext);
    ASSERT_GE(ast.size(), 2u);
    auto& node = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(node, O_EXCALL));
    EXPECT_EQ(node[1].to<std::string>(), "foo");

    ASSERT_GE(node.size(), 4u);
}

TEST(gt_ascript_parse, Excall_NoArgs) {
    std::unordered_map<std::string, native_func> ext = {{"foo", nullptr}};
    auto ast = parse_src_ext("$foo();", ext);
    ASSERT_GE(ast.size(), 2u);
    auto& node = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(node, O_EXCALL));
    EXPECT_EQ(node[1].to<std::string>(), "foo");
    EXPECT_EQ(node.size(), 2u);
}

TEST(gt_ascript_parse, Excall_Undefined) {

    std::unordered_map<std::string, native_func> ext;
    EXPECT_TRUE(parse_ext_has_error("$foo(1);", ext));
}

TEST(gt_ascript_parse, Excall_NullTable) {

    auto ast = parse_src("$foo(1);");
    ASSERT_GE(ast.size(), 2u);
    auto& node = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(node, O_EXCALL));
    EXPECT_EQ(node[1].to<std::string>(), "foo");
    EXPECT_EQ(node.size(), 3u);
}

TEST(gt_ascript_parse, Excall_InExpr) {
    std::unordered_map<std::string, native_func> ext = {{"foo", nullptr}};
    auto ast = parse_src_ext("1 + $foo(x);", ext);
    ASSERT_GE(ast.size(), 2u);
    auto& add = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(add, O_ADD));

    auto& excall = add[2].to<varvec>();
    EXPECT_TRUE(head_is(excall, O_EXCALL));
}

TEST(gt_ascript_parse, HereConstant) {
    auto ast = parse_src("here();");
    ASSERT_GE(ast.size(), 2u);
    auto& stmt = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(stmt, O_HERE));
    ASSERT_GE(stmt.size(), 2u);
    auto& m = stmt[1].to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 1);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 0);
    EXPECT_EQ(m.value("file").to<std::string>(), "");
}
TEST(gt_ascript_parse, HereFileAbsolute) {
    alx::bytes b("here();");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl, nullptr, "/tmp/xyz/test.axc");
    auto ast = p.parse();
    ASSERT_GE(ast.size(), 2u);
    auto& m = ast[1].to<varvec>()[1].to<varmap>();
    EXPECT_EQ(m.value("file").to<std::string>(), "/tmp/xyz/test.axc");
}
TEST(gt_ascript_parse, HereBareRejected) {
    alx::bytes b("here;");
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    parser p(tl);
    p.parse();
    EXPECT_TRUE(p.has_error());
}
TEST(gt_ascript_parse, HereLine2) {
    auto ast = parse_src("\nhere();");
    ASSERT_GE(ast.size(), 2u);
    auto& m = ast[1].to<varvec>()[1].to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 2);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 1);
}

TEST(gt_ascript_parse, HerePositionAfterComment) {

    auto ast = parse_src("/* c */\nhere();");
    ASSERT_GE(ast.size(), 2u);
    auto& m = ast[1].to<varvec>()[1].to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 2);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 8);
}

TEST(gt_ascript_parse, TypedVar_Int) {
    auto ast = parse_src("int a;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "a");

    ASSERT_TRUE(v[2].is_vec());
    auto& def = v[2].to<varvec>();
    EXPECT_EQ(def[0].to<int_64>(), 0);
    EXPECT_EQ(v.size(), 3u);
}

TEST(gt_ascript_parse, TypedVar_Float) {
    auto ast = parse_src("float b;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "b");
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_DOUBLE_EQ(v[2].to<varvec>()[0].to<double>(), 0.0);
}

TEST(gt_ascript_parse, TypedVar_String) {
    auto ast = parse_src("string c;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "c");
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_EQ(v[2].to<varvec>()[0].to<std::string>(), "");
}

TEST(gt_ascript_parse, TypedVar_Bool) {
    auto ast = parse_src("bool d;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_EQ(v[2].to<varvec>()[0].to<bool>(), false);
}

TEST(gt_ascript_parse, TypedVar_Bytes) {
    auto ast = parse_src("bytes e;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_TRUE(v[2].to<varvec>()[0].to<bytes>().empty());
}

TEST(gt_ascript_parse, TypedVar_Vec) {
    auto ast = parse_src("vec f;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_TRUE(v[2].to<varvec>()[0].to<varvec>().empty());
}

TEST(gt_ascript_parse, TypedVar_Map) {
    auto ast = parse_src("map g;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_TRUE(v[2].to<varvec>()[0].to<varmap>().empty());
}

TEST(gt_ascript_parse, TypedVar_Lst) {
    auto ast = parse_src("lst h;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    EXPECT_TRUE(v[2].to<varvec>()[0].to<varlst>().empty());
}

TEST(gt_ascript_parse, TypedVar_IntWithInit) {
    auto ast = parse_src("int a = 42;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    EXPECT_EQ(v[1].to<std::string>(), "a");
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_INT));
    EXPECT_EQ(init[1].to<varvec>()[0].to<int_64>(), 42);
    EXPECT_EQ(v.size(), 3u);
}

TEST(gt_ascript_parse, TypedVar_FloatWithInit) {
    auto ast = parse_src("float b = 3.14;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_FLOAT));
    EXPECT_DOUBLE_EQ(init[1].to<varvec>()[0].to<double>(), 3.14);
}

TEST(gt_ascript_parse, TypedVar_StringWithInit) {
    auto ast = parse_src("string c = \"hello\";");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_STRING));
    EXPECT_EQ(init[1].to<varvec>()[0].to<std::string>(), "hello");
}

TEST(gt_ascript_parse, TypedVar_BoolWithInit) {
    auto ast = parse_src("bool d = true;");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_BOOL));
    EXPECT_EQ(init[1].to<varvec>()[0].to<bool>(), true);
}

TEST(gt_ascript_parse, TypedVar_VecWithInit) {
    auto ast = parse_src("vec v = [1,2];");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_VEC));
}

TEST(gt_ascript_parse, TypedVar_MapWithInit) {
    auto ast = parse_src("map m = map{};");
    auto& v = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(v, O_VAR));
    ASSERT_TRUE(v[2].is_vec());
    auto& init = v[2].to<varvec>();
    EXPECT_TRUE(head_is(init, O_MAP));
}

TEST(gt_ascript_parse, ForTypedVar_CStyle) {
    auto ast = parse_src("for (int i = 0; i < 10; ++i) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));

    auto& init = f[1].to<varvec>();
    EXPECT_TRUE(head_is(init, O_VAR));
    EXPECT_EQ(init[1].to<std::string>(), "i");
    ASSERT_TRUE(init[2].is_vec());
    EXPECT_TRUE(head_is(init[2].to<varvec>(), O_INT));
}

TEST(gt_ascript_parse, ForTypedVar_CStyleNoInit) {

    auto ast = parse_src("for (int i; i < 10; ++i) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));
    auto& init = f[1].to<varvec>();
    EXPECT_TRUE(head_is(init, O_VAR));
    EXPECT_EQ(init[1].to<std::string>(), "i");

    ASSERT_TRUE(init[2].is_vec());
    EXPECT_EQ(init[2].to<varvec>()[0].to<int_64>(), 0);
}

TEST(gt_ascript_parse, ForTypedVar_ForEach) {
    auto ast = parse_src("for (int x : arr) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOREACH));

    auto& target = f[1].to<varvec>();
    EXPECT_TRUE(head_is(target, O_VAR));
    EXPECT_EQ(target[1].to<std::string>(), "x");
    ASSERT_TRUE(target[2].is_vec());
    EXPECT_EQ(target[2].to<varvec>()[0].to<int_64>(), 0);

    auto& iter = f[2].to<varvec>();
    EXPECT_TRUE(head_is(iter, O_LOAD));
    EXPECT_EQ(iter[1].to<std::string>(), "arr");
}

TEST(gt_ascript_parse, ForTypedVar_MultiDecl) {

    auto ast = parse_src("for (int i = 0, j = 1; i < 10; ++i) {}");
    auto& f = ast[1].to<varvec>();
    EXPECT_TRUE(head_is(f, O_FOR));
    auto& init = f[1].to<varvec>();
    EXPECT_TRUE(head_is(init, O_VAR));
    EXPECT_EQ(init[1].to<std::string>(), "i");
    EXPECT_EQ(init[3].to<std::string>(), "j");

    ASSERT_TRUE(init[2].is_vec());
    ASSERT_TRUE(init[4].is_vec());
}

struct parse_out {
    varvec ast;
    std::vector<compile_error> errors;
};

static parse_out parse_full(const char* src) {
    alx::bytes b(src);
    token_list tl;
    tl.tokenize(alx::bytes_view(b));
    alx::signal<const compile_error&> sig;
    parse_out out;
    sig.connect([&](const compile_error& e) { out.errors.push_back(e); });
    parser p(tl, &sig);
    out.ast = p.parse();
    return out;
}

TEST(gt_ascript_parse, AssignTarget_NotCheckedAtParse) {

    // not an lvalue, but the parse leaves the judgement to the run-time resolver
    const char* deferred[] = {"1 = 2;", "\"abc\" = 1;", "[1,2] = 3;", "1 += 1;", "(1) = 2;"};
    for (const char* src : deferred) {
        auto out = parse_full(src);
        EXPECT_EQ(out.errors.size(), 0u) << src;
    }

    EXPECT_EQ(parse_full("var v = [1,2,3]; v[1,3] = 99;").errors.size(), 0u);

    EXPECT_EQ(parse_full("var x = 1; x = 2;").errors.size(), 0u);
    EXPECT_EQ(parse_full("var v = [1]; v[0] = 2;").errors.size(), 0u);
    EXPECT_EQ(parse_full("var a = 0; a += 1; a++;").errors.size(), 0u);
    EXPECT_EQ(parse_full("var v = [1,2]; v[null] += 1;").errors.size(), 0u);
}

TEST(gt_ascript_parse, MissingOperand_ReportedAtOperator) {

    auto out = parse_full("s-=");
    ASSERT_EQ(out.errors.size(), 2u);
    EXPECT_NE(out.errors[0].msg.find("Expected expression after '-='"), std::string::npos);
    EXPECT_TRUE(head_is(out.ast, O_PROGRAM));
    EXPECT_TRUE(head_is(out.ast[1].to<varvec>(), O_LOAD));
}

TEST(gt_ascript_parse, CharLiteralErrors) {

    auto empty = parse_full("'';");
    ASSERT_EQ(empty.errors.size(), 1u);
    EXPECT_NE(empty.errors[0].msg.find("empty character literal"), std::string::npos);

    for (const char* src : {"'ab';", "'\\x41\\x42';"}) {
        auto multi = parse_full(src);
        ASSERT_EQ(multi.errors.size(), 1u) << src;
        EXPECT_NE(multi.errors[0].msg.find("exactly one code point"), std::string::npos) << src;
    }

    const char bad[] = {'\'', static_cast<char>(0xff), '\'', ';', 0};
    auto invalid = parse_full(bad);
    ASSERT_EQ(invalid.errors.size(), 1u);
    EXPECT_NE(invalid.errors[0].msg.find("invalid UTF-8"), std::string::npos);
}

TEST(gt_ascript_parse, MissingOperand_NoCrashShapes) {

    const char* cases[] = {
        "var x = 1; x += ;",
        "var x = 1; x -= ;",
        "var x = 1; x |= ;",
        "var x = 1; x -= / 2;",
        "var x = 1; var y = 1; x += y + ;",
        "var x = 1; x += (;",
        "f(x += , 1);",
    };
    for (const char* src : cases) {
        auto out = parse_full(src);
        EXPECT_GE(out.errors.size(), 1u) << src;
        EXPECT_TRUE(head_is(out.ast, O_PROGRAM)) << src;
    }
}

TEST(gt_ascript_parse, MissingOperand_SingleErrorNoCascade) {

    EXPECT_EQ(parse_full("var x = 1; x += ;").errors.size(), 1u);
    EXPECT_EQ(parse_full("f(x += , 1);").errors.size(), 1u);
    EXPECT_EQ(parse_full("while (1) { x += ; }").errors.size(), 1u);
}

TEST(gt_ascript_parse, ImportLink_MissingPath_NoCrash) {

    const char* cases[] = {"import", "link", "import // tail", "import ;", "import \"x\"", "link \"x\""};
    for (const char* src : cases) {
        auto out = parse_full(src);
        EXPECT_GE(out.errors.size(), 1u) << src;
        EXPECT_TRUE(head_is(out.ast, O_PROGRAM)) << src;
    }
    EXPECT_EQ(parse_full("import").errors.size(), 4u);
}

TEST(gt_ascript_parse, MissingOperand_CallArgRecovery) {

    auto out = parse_full("f(x += , 1);");
    ASSERT_EQ(out.errors.size(), 1u);
    auto& call = out.ast[1].to<varvec>();
    ASSERT_TRUE(head_is(call, O_CALL));
    EXPECT_TRUE(head_is(call[2].to<varvec>(), O_LOAD));
    EXPECT_EQ(call[3].to<varvec>()[0].to<int_64>(), 1);
}
