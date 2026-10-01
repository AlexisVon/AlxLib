/** ****************************************************************
 * \file   gt_ascript_compile.cpp
 * \brief  Compile pipeline unit tests — resolve, embed, binary format
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include "ascript_compile.h"
#include "ascript_lex.h"
#include "ascript_parse.h"
#include "avarsolid.h"
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

static std::string tmp_path(const char* name) {
    return std::string("/tmp/") + name;
}
static bool write_tmp(const char* name, const char* content) {
    std::string p = tmp_path(name);
    std::ofstream ofs(p);
    if (!ofs) return false;
    ofs << content;
    return true;
}
static void rm_tmp(const char* name) {
    std::remove(tmp_path(name).c_str());
}

TEST(gt_ascript_compile, MakeCompileBinary_Plain) {
    varvec ast = parse_src("var x = 42;");
    compile_result r;
    bytes bin = make_compile_binary("test.axc", ast, "test", 1, r, false);
    EXPECT_FALSE(bin.empty());
    EXPECT_TRUE(varsolid::is_valid(alx::bytes_view(bin)));

    varmap outer;
    EXPECT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));
    EXPECT_TRUE(outer.contain("ast"));
    EXPECT_EQ(outer.value("etype").to<std::string>(), "test");
    EXPECT_EQ(outer.value("name").to<std::string>(), "test.axc");
    EXPECT_FALSE(outer.contain("data"));
}

TEST(gt_ascript_compile, MakeCompileBinary_Compressed) {
    varvec ast = parse_src("var y = 99;");
    compile_result r;
    bytes bin = make_compile_binary("", ast, "", 0, r, true);
    EXPECT_FALSE(bin.empty());
    EXPECT_TRUE(varsolid::is_valid(alx::bytes_view(bin)));

    varmap outer;
    EXPECT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));
    EXPECT_TRUE(outer.contain("ast"));
    EXPECT_TRUE(outer.contain("info"));
    EXPECT_FALSE(outer.contain("data"));
    const varmap& info = outer.value("info").to<varmap>();
    EXPECT_TRUE(info.contain("sha256"));
    EXPECT_TRUE(info.contain("size"));
    EXPECT_TRUE(info.contain("compressed"));
    EXPECT_TRUE(info.value("compressed").to<bool>());
}

TEST(gt_ascript_compile, MakeCompileBinary_WithDependencies) {
    varvec ast = parse_src("import \"bar.axc\"; link \"baz\";");
    compile_result r;
    r.imports.push_back("bar.axc");
    r.links.push_back("baz");

    bytes bin = make_compile_binary("prog.axc", ast, "prog", 2, r, false);
    varmap outer;
    ASSERT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));

    EXPECT_TRUE(outer.contain("imports"));
    EXPECT_TRUE(outer.contain("links"));
}

TEST(gt_ascript_compile, DecompressInner_Uncompressed) {

    varvec ast = parse_src("var z = 7;");
    compile_result r;
    r.modules["@test"] = variant(varmap());
    bytes bin = make_compile_binary("", ast, "", 0, r, false);

    varmap outer;
    ASSERT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));

    varvec out_ast;
    varmap out_mods;
    EXPECT_TRUE(decompress_inner(outer, out_ast, out_mods));
    EXPECT_FALSE(out_ast.empty());
}

TEST(gt_ascript_compile, DecompressInner_NewFormat) {

    varvec ast = parse_src("var w = 8;");
    compile_result r;
    bytes bin = make_compile_binary("", ast, "", 0, r, true);

    varmap outer;
    ASSERT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));

    varvec out_ast;
    varmap out_mods;
    EXPECT_TRUE(decompress_inner(outer, out_ast, out_mods));
    EXPECT_FALSE(out_ast.empty());
}

TEST(gt_ascript_compile, DecompressInner_Invalid) {
    varvec out_ast;
    varmap out_mods;
    varmap empty_map;
    EXPECT_FALSE(decompress_inner(empty_map, out_ast, out_mods));
}

TEST(gt_ascript_compile, GetDependencies_Valid) {
    varvec ast = parse_src("import \"a.axc\"; link \"b\";");
    compile_result r;
    r.imports.push_back("a.axc");
    r.links.push_back("b");
    bytes bin = make_compile_binary("", ast, "", 0, r, false);

    compile_result out;
    EXPECT_TRUE(get_dependencies(alx::bytes_view(bin), out));
    ASSERT_FALSE(out.imports.empty());
    EXPECT_EQ(out.imports.front(), "a.axc");
    ASSERT_FALSE(out.links.empty());
    EXPECT_EQ(out.links.front(), "b");
}

TEST(gt_ascript_compile, GetDependencies_NoDeps) {
    varvec ast = parse_src("var x = 1;");
    compile_result r;
    bytes bin = make_compile_binary("", ast, "", 0, r, false);

    compile_result out;
    EXPECT_TRUE(get_dependencies(alx::bytes_view(bin), out));
    EXPECT_TRUE(out.imports.empty());
    EXPECT_TRUE(out.links.empty());
}

TEST(gt_ascript_compile, GetDependencies_Invalid) {
    compile_result out;
    EXPECT_FALSE(get_dependencies(alx::bytes_view(), out));
}

TEST(gt_ascript_compile, RoundTrip_CompressedBinary) {

    varvec ast1 = parse_src("var result = 100;");
    compile_result r;
    bytes bin = make_compile_binary("", ast1, "test", 1, r, true);

    varmap outer;
    ASSERT_TRUE(varsolid::to_varmap(alx::bytes_view(bin), outer));
    varvec ast2;
    varmap mods;
    ASSERT_TRUE(decompress_inner(outer, ast2, mods));
    EXPECT_FALSE(ast2.empty());
}

TEST(gt_ascript_compile, CompileEmbed_SelfContained) {

    write_tmp("alexis_emb_main.axc", "import \"alexis_emb_mod.axc\" as m; m.foo();");
    write_tmp("alexis_emb_mod.axc", "def foo() { return 42; }");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_main.axc"), false, true);

    rm_tmp("alexis_emb_main.axc");
    rm_tmp("alexis_emb_mod.axc");
    ASSERT_FALSE(bin.empty());
    auto res = eng->exec(bytes_view(bin), "");
    ASSERT_EQ(res.error, error_type::NoError) << res.value.to<std::string>();
    EXPECT_TRUE(res.value.is<int_64>());
    EXPECT_EQ(res.value.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_NestedImports) {

    write_tmp("alexis_emb_main2.axc", "import \"alexis_emb_mid.axc\" as m; m.g();");
    write_tmp("alexis_emb_mid.axc", "import \"alexis_emb_leaf.axc\" as l; def g() { return l.h(); }");
    write_tmp("alexis_emb_leaf.axc", "def h() { return 7; }");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_main2.axc"), false, true);
    rm_tmp("alexis_emb_main2.axc");
    rm_tmp("alexis_emb_mid.axc");
    rm_tmp("alexis_emb_leaf.axc");
    ASSERT_FALSE(bin.empty());
    EXPECT_EQ(eng->exec(bytes_view(bin), "").value.to<int_64>(), 7);
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_ErrorNamesModule) {

    write_tmp("alexis_emb_e_main.axc", "import \"alexis_emb_e_leaf.axc\" as m;");
    write_tmp("alexis_emb_e_leaf.axc", "var x = ;");
    auto* eng = engine::create();
    std::string path;
    uint_64 row = 0;
    eng->on_cmpl.connect([&](const compile_error& e) {
        if (path.empty()) {
            path = e.loc.path;
            row = e.loc.row;
        }
    });
    bytes bin = eng->compile(tmp_path("alexis_emb_e_main.axc"), false, true);
    rm_tmp("alexis_emb_e_main.axc");
    rm_tmp("alexis_emb_e_leaf.axc");
    EXPECT_TRUE(bin.empty());
    EXPECT_NE(path.find("alexis_emb_e_leaf.axc"), std::string::npos);
    EXPECT_GT(row, 0u);
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_LinkForbidden) {
    write_tmp("alexis_emb_link.axc", "link \"somelib.so\" as b;");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_link.axc"), false, true);
    rm_tmp("alexis_emb_link.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_EnvForbidden) {
    write_tmp("alexis_emb_env.axc", "env([\".\"]); var x = 1;");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_env.axc"), false, true);
    rm_tmp("alexis_emb_env.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_UnresolvedImport) {
    write_tmp("alexis_emb_missing.axc", "import \"no_such_module.axc\" as m;");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_missing.axc"), false, true);
    rm_tmp("alexis_emb_missing.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_CircularImport) {
    write_tmp("alexis_emb_a.axc", "import \"alexis_emb_b.axc\" as b;");
    write_tmp("alexis_emb_b.axc", "import \"alexis_emb_a.axc\" as a;");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_a.axc"), false, true);
    rm_tmp("alexis_emb_a.axc");
    rm_tmp("alexis_emb_b.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_NoEmbedNoDeps) {

    auto* eng = engine::create();
    alx::bytes b("import \"anything.axc\" as m; var x = 1;");
    bytes bin = eng->compile(bytes_view(b), "", false, false);
    EXPECT_FALSE(bin.empty());
    delete eng;
}

static bool unwrap_bin(const bytes& _bin, varvec& _ast, varmap& _mods) {
    varmap outer;
    if (!varsolid::to_varmap(bytes_view(_bin), outer)) return false;
    return decompress_inner(outer, _ast, _mods);
}

static bool first_import_path(const varvec& _stmts, std::string& _out) {
    for (auto& s : _stmts) {
        if (!s.is_vec()) continue;
        const auto& n = s.to<varvec>();
        if (!n.empty() && n[0].is<OPTYPE>() &&
            static_cast<op_enum>(n[0].to<OPTYPE>()) == O_IMPORT && n.size() > 1) {
            _out = n[1].to<std::string>();
            return true;
        }
    }
    return false;
}

TEST(gt_ascript_compile, CompileEmbed_TreeStructure) {
    write_tmp("alexis_emb_t_main.axc", "import \"alexis_emb_t_mod.axc\" as m; m.foo();");
    write_tmp("alexis_emb_t_mod.axc", "def foo() { return 1; }");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_t_main.axc"), false, true);
    rm_tmp("alexis_emb_t_main.axc");
    rm_tmp("alexis_emb_t_mod.axc");
    ASSERT_FALSE(bin.empty());

    varvec ast;
    varmap mods;
    ASSERT_TRUE(unwrap_bin(bin, ast, mods));

    std::string import_path;
    ASSERT_TRUE(first_import_path(ast, import_path));
    EXPECT_EQ(import_path.size(), 17u);
    EXPECT_EQ(import_path[0], '@');

    ASSERT_EQ(mods.size(), 1u);
    ASSERT_TRUE(mods.contain(import_path));
    const varmap& entry = mods.value(import_path).to<varmap>();
    EXPECT_EQ(entry.value("resolved").to<std::string>(), tmp_path("alexis_emb_t_mod.axc"));
    EXPECT_EQ(entry.value("path").to<std::string>(), "alexis_emb_t_mod.axc");
    EXPECT_TRUE(entry.value("ast").is_vec());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_NestedTreeStructure) {
    write_tmp("alexis_emb_n_main.axc", "import \"alexis_emb_n_mid.axc\" as m; m.g();");
    write_tmp("alexis_emb_n_mid.axc", "import \"alexis_emb_n_leaf.axc\" as l; def g() { return l.h(); }");
    write_tmp("alexis_emb_n_leaf.axc", "def h() { return 7; }");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_emb_n_main.axc"), false, true);
    rm_tmp("alexis_emb_n_main.axc");
    rm_tmp("alexis_emb_n_mid.axc");
    rm_tmp("alexis_emb_n_leaf.axc");
    ASSERT_FALSE(bin.empty());

    varvec ast;
    varmap mods;
    ASSERT_TRUE(unwrap_bin(bin, ast, mods));
    ASSERT_EQ(mods.size(), 2u);

    std::string mid_key;
    ASSERT_TRUE(first_import_path(ast, mid_key));
    ASSERT_TRUE(mods.contain(mid_key));
    const varmap& mid_entry = mods.value(mid_key).to<varmap>();
    EXPECT_EQ(mid_entry.value("resolved").to<std::string>(), tmp_path("alexis_emb_n_mid.axc"));

    std::string leaf_key;
    ASSERT_TRUE(first_import_path(mid_entry.value("ast").to<varvec>(), leaf_key));
    EXPECT_EQ(leaf_key.size(), 17u);
    EXPECT_EQ(leaf_key[0], '@');
    ASSERT_TRUE(mods.contain(leaf_key));
    EXPECT_EQ(mods.value(leaf_key).to<varmap>().value("resolved").to<std::string>(),
              tmp_path("alexis_emb_n_leaf.axc"));
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_KeyStable) {

    write_tmp("alexis_emb_k_main.axc", "import \"alexis_emb_k.axc\" as m; m.foo();");
    write_tmp("alexis_emb_k.axc", "def foo() { return 1; }");
    auto* eng = engine::create();
    bytes bin1 = eng->compile(tmp_path("alexis_emb_k_main.axc"), false, true);

    write_tmp("alexis_emb_k.axc", "def foo() { return 2; }");
    bytes bin2 = eng->compile(tmp_path("alexis_emb_k_main.axc"), false, true);
    rm_tmp("alexis_emb_k_main.axc");
    rm_tmp("alexis_emb_k.axc");
    ASSERT_FALSE(bin1.empty());
    ASSERT_FALSE(bin2.empty());

    varvec ast1, ast2;
    varmap mods1, mods2;
    ASSERT_TRUE(unwrap_bin(bin1, ast1, mods1));
    ASSERT_TRUE(unwrap_bin(bin2, ast2, mods2));
    std::string k1, k2;
    ASSERT_TRUE(first_import_path(ast1, k1));
    ASSERT_TRUE(first_import_path(ast2, k2));

    EXPECT_NE(k1, k2);
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_ImportedAxpWithDepsForbidden) {

    write_tmp("alexis_emb_dep.axc", "import \"no_such_leaf.axc\" as l;");
    auto* eng = engine::create();

    bytes dep_bin = eng->compile(tmp_path("alexis_emb_dep.axc"), false, false);
    rm_tmp("alexis_emb_dep.axc");
    ASSERT_FALSE(dep_bin.empty());

    {
        std::ofstream ofs("/tmp/alexis_emb_dep.axp");
        ofs.write(reinterpret_cast<const char*>(dep_bin.data()), static_cast<std::streamsize>(dep_bin.size()));
    }
    write_tmp("alexis_emb_dep_main.axc", "import \"/tmp/alexis_emb_dep.axp\" as m;");
    bytes bin = eng->compile(tmp_path("alexis_emb_dep_main.axc"), false, true);
    std::remove("/tmp/alexis_emb_dep.axp");
    rm_tmp("alexis_emb_dep_main.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_ImportedAxpVersionTooNew) {

    write_tmp("alexis_emb_v.axc", "def foo() { return 1; }");
    auto* eng_new = engine::create();
    eng_new->set_vtype(5);
    bytes dep_bin = eng_new->compile(tmp_path("alexis_emb_v.axc"), false, false);
    rm_tmp("alexis_emb_v.axc");
    ASSERT_FALSE(dep_bin.empty());
    {
        std::ofstream ofs("/tmp/alexis_emb_v.axp");
        ofs.write(reinterpret_cast<const char*>(dep_bin.data()), static_cast<std::streamsize>(dep_bin.size()));
    }
    delete eng_new;

    auto* eng = engine::create();
    eng->set_vtype(3);
    write_tmp("alexis_emb_v_main.axc", "import \"/tmp/alexis_emb_v.axp\" as m;");
    bytes bin = eng->compile(tmp_path("alexis_emb_v_main.axc"), false, true);
    std::remove("/tmp/alexis_emb_v.axp");
    rm_tmp("alexis_emb_v_main.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_compile, CompileEmbed_ImportedAxpEtypeMismatch) {

    write_tmp("alexis_emb_e.axc", "def foo() { return 1; }");
    auto* eng_other = engine::create();
    eng_other->set_etype("other");
    bytes dep_bin = eng_other->compile(tmp_path("alexis_emb_e.axc"), false, false);
    rm_tmp("alexis_emb_e.axc");
    ASSERT_FALSE(dep_bin.empty());
    {
        std::ofstream ofs("/tmp/alexis_emb_e.axp");
        ofs.write(reinterpret_cast<const char*>(dep_bin.data()), static_cast<std::streamsize>(dep_bin.size()));
    }
    delete eng_other;

    auto* eng = engine::create();
    eng->set_etype("mine");
    write_tmp("alexis_emb_e_main.axc", "import \"/tmp/alexis_emb_e.axp\" as m;");
    bytes bin = eng->compile(tmp_path("alexis_emb_e_main.axc"), false, true);
    std::remove("/tmp/alexis_emb_e.axp");
    rm_tmp("alexis_emb_e_main.axc");
    EXPECT_TRUE(bin.empty());
    delete eng;
}

static alx::bytes pf(const char* _src) {
    return alx::script::prtfmt(alx::bytes_view(alx::bytes(_src)));
}

static bool has(const alx::bytes& _out, const char* _sub) {
    return _out.find(alx::bytes(_sub)) != alx::max_uint_64;
}

TEST(gt_ascript_compile, PrtFmt_Structure) {

    EXPECT_TRUE(has(pf("def f(x,y){var z=x+1;if(z>0){return y*2;}else{return -z;}}"),
                     "def f(x, y) {\n    var z = x + 1;\n    if (z > 0) {"));

    EXPECT_FALSE(pf("def f( {if( x").empty());

    EXPECT_TRUE(has(pf("var x=1;// note\n/* block\ncomment */\nvar y=2;"), "// note"));
    EXPECT_TRUE(has(pf("var x=1;// note\n/* block\ncomment */\nvar y=2;"), "/* block"));

    EXPECT_TRUE(has(pf("for(var i=0;i<10;i++){trap(i);}"), "for (var i = 0; i < 10; i++) {"));

    EXPECT_TRUE(has(pf("var x=1;\n\nvar y=2;"), "var x = 1;\n\nvar y = 2;"));
    EXPECT_TRUE(has(pf("var x=1;\nvar y=2;"), "var x = 1;\nvar y = 2;"));

    EXPECT_TRUE(has(pf("if(x){}else{}"), "} else {"));
    EXPECT_TRUE(has(pf("try{}catch(e){}"), "} catch (e) {"));

    EXPECT_TRUE(has(pf("var m=map{\"a\":1};"), "map{\"a\":1}"));
    EXPECT_TRUE(has(pf("var v=vec{1,2};"), "vec{1,2}"));

    EXPECT_TRUE(has(pf("int a=42;"), "int a = 42;"));
    EXPECT_TRUE(has(pf("float b=3.14;"), "float b = 3.14;"));
}

TEST(gt_ascript_compile, PrtFmt_Expr) {

    EXPECT_TRUE(has(pf("a+b;"), "a + b"));
    EXPECT_TRUE(has(pf("a-b;"), "a - b"));
    EXPECT_TRUE(has(pf("a*b;"), "a * b"));
    EXPECT_TRUE(has(pf("a/b;"), "a / b"));
    EXPECT_TRUE(has(pf("a%b;"), "a % b"));

    EXPECT_TRUE(has(pf("a==b;"), "a == b"));
    EXPECT_TRUE(has(pf("a!=b;"), "a != b"));
    EXPECT_TRUE(has(pf("a<b;"), "a < b"));
    EXPECT_TRUE(has(pf("a>b;"), "a > b"));
    EXPECT_TRUE(has(pf("a<=b;"), "a <= b"));
    EXPECT_TRUE(has(pf("a>=b;"), "a >= b"));

    EXPECT_TRUE(has(pf("a&&b;"), "a && b"));
    EXPECT_TRUE(has(pf("a||b;"), "a || b"));

    EXPECT_TRUE(has(pf("a&b;"), "a & b"));
    EXPECT_TRUE(has(pf("a|b;"), "a | b"));
    EXPECT_TRUE(has(pf("a^b;"), "a ^ b"));
    EXPECT_TRUE(has(pf("a<<b;"), "a << b"));
    EXPECT_TRUE(has(pf("a>>b;"), "a >> b"));

    EXPECT_TRUE(has(pf("a=b;"), "a = b"));
    EXPECT_TRUE(has(pf("a+=b;"), "a += b"));
    EXPECT_TRUE(has(pf("a-=b;"), "a -= b"));
    EXPECT_TRUE(has(pf("a*=b;"), "a *= b"));
    EXPECT_TRUE(has(pf("a/=b;"), "a /= b"));

    EXPECT_TRUE(has(pf("a=b?c:d;"), "b ? c : d"));

    EXPECT_TRUE(has(pf("a=-x;"), "= -x"));
    EXPECT_TRUE(has(pf("a=!x;"), "= !x"));
    EXPECT_TRUE(has(pf("a=~x;"), "= ~x"));
    EXPECT_TRUE(has(pf("a=++x;"), "= ++x"));
    EXPECT_TRUE(has(pf("a=--x;"), "= --x"));

    EXPECT_TRUE(has(pf("x++;"), "x++"));
    EXPECT_TRUE(has(pf("x--;"), "x--"));

    EXPECT_TRUE(has(pf("f(x);"), "f(x)"));
    EXPECT_TRUE(has(pf("f(x,y);"), "f(x, y)"));

    EXPECT_TRUE(has(pf("a[i];"), "a[i]"));

    EXPECT_TRUE(has(pf("a=(b+c);"), "(b + c)"));
}

TEST(gt_ascript_compile, PrtFmt_Ext) {

    EXPECT_TRUE(has(pf("$foo();"), "$foo()"));
    EXPECT_TRUE(has(pf("$foo(1,2);"), "$foo(1, 2)"));

    EXPECT_TRUE(has(pf("var x=$PI;"), "$PI"));

    EXPECT_TRUE(has(pf("a.b;"), "a.b"));
    EXPECT_TRUE(has(pf("a.b.c;"), "a.b.c"));

    EXPECT_TRUE(has(pf("..x;"), ".. x"));

    EXPECT_TRUE(has(pf("::x;"), "::x"));

    EXPECT_TRUE(has(pf("@name;"), "@name"));
    EXPECT_TRUE(has(pf("@(expr);"), "@(expr)"));

    EXPECT_TRUE(has(pf("delete x;"), "delete x"));
}

TEST(gt_ascript_compile, PrtFmt_Ctrl) {
    EXPECT_TRUE(has(pf("if(x){}"), "if (x) {"));
    EXPECT_TRUE(has(pf("if(x){}else{}"), "if (x) {"));
    EXPECT_TRUE(has(pf("while(x){}"), "while (x) {"));
    EXPECT_TRUE(has(pf("for(;;){}"), "for (; ; ) {"));
    EXPECT_TRUE(has(pf("for(var i=0;i<10;i++){}"), "for (var i = 0; i < 10; i++) {"));
    EXPECT_TRUE(has(pf("switch(x){case 1:break;}"), "switch (x) {"));
    EXPECT_TRUE(has(pf("break;"), "break;"));
    EXPECT_TRUE(has(pf("continue;"), "continue;"));
    EXPECT_TRUE(has(pf("return x;"), "return x"));
    EXPECT_TRUE(has(pf("throw x;"), "throw x"));
    EXPECT_TRUE(has(pf("try{}catch(e){}"), "catch (e) {"));
}

TEST(gt_ascript_compile, PrtFmt_Decl) {
    EXPECT_TRUE(has(pf("var x=1;"), "var x = 1"));
    EXPECT_TRUE(has(pf("var x;"), "var x"));
    EXPECT_TRUE(has(pf("def f(){return 1;}"), "def f() {"));
    EXPECT_TRUE(has(pf("int a=42;"), "int a = 42"));
    EXPECT_TRUE(has(pf("float b=3.14;"), "float b = 3.14"));
    EXPECT_TRUE(has(pf("string s=\"hi\";"), "string s = \"hi\""));
}

TEST(gt_ascript_compile, PrtFmt_ExprExt) {

    EXPECT_TRUE(has(pf("a+b;$foo();"), "a + b;\n$foo()"));

    EXPECT_TRUE(has(pf("a+b.c;"), "a + b.c"));

    EXPECT_TRUE(has(pf("a+b::c;"), "a + b::c"));

    EXPECT_TRUE(has(pf("a=b+@x;"), "a = b + @x"));

    EXPECT_TRUE(has(pf("a=$foo();"), "= $foo()"));

    EXPECT_TRUE(has(pf("a=b.c;"), "a = b.c"));

    EXPECT_TRUE(has(pf("a[0].b;"), "a[0].b"));

    EXPECT_TRUE(has(pf("f()[0];"), "f()[0]"));

    EXPECT_TRUE(has(pf("f().$x;"), "f().$x"));

    EXPECT_TRUE(has(pf("a.b.(c.d);"), "a.b.(c.d)"));
}

TEST(gt_ascript_compile, PrtFmt_ExprCtrl) {

    EXPECT_TRUE(has(pf("a+b;return x;"), "a + b;\nreturn x"));

    EXPECT_TRUE(has(pf("a=b;else{}"), "a = b;\nelse"));

    EXPECT_TRUE(has(pf("f(x);if(y){}"), "f(x);\nif (y)"));

    EXPECT_TRUE(has(pf("a=1;while(x){}"), "a = 1;\nwhile (x)"));

    EXPECT_TRUE(has(pf("a=1;for(;;){}"), "a = 1;\nfor (; ; )"));

    EXPECT_TRUE(has(pf("a=1;break;"), "a = 1;\nbreak"));

    EXPECT_TRUE(has(pf("a=1;throw x;"), "a = 1;\nthrow x"));
}

TEST(gt_ascript_compile, PrtFmt_ExtChain) {

    EXPECT_TRUE(has(pf("$foo().x;"), "$foo().x"));

    EXPECT_TRUE(has(pf("a.$b;"), "a.$b"));

    EXPECT_TRUE(has(pf("a.b.c;"), "a.b.c"));

    EXPECT_TRUE(has(pf("::a.b;"), "::a.b"));

    EXPECT_TRUE(has(pf("::$a;"), "::$a"));

    EXPECT_TRUE(has(pf("a.::b;"), "a. ::b"));

    EXPECT_TRUE(has(pf("@x.y;"), "@x.y"));

    EXPECT_TRUE(has(pf("f().$x.y;"), "f().$x.y"));

    EXPECT_TRUE(has(pf("a.b.(c.d);"), "a.b.(c.d)"));

    EXPECT_TRUE(has(pf("::a.b.(c.d);"), "::a.b.(c.d)"));

    EXPECT_TRUE(has(pf("..a.(f)();"), ".. a.(f)()"));

    EXPECT_TRUE(has(pf(".a.(b);"), ".a.(b)"));
}

TEST(gt_ascript_compile, PrtFmt_ExtCtrl) {

    EXPECT_TRUE(has(pf("$foo();return x;"), "$foo();\nreturn x"));

    EXPECT_TRUE(has(pf("a.b;else{}"), "a.b;\nelse"));

    EXPECT_TRUE(has(pf("::x;throw y;"), "::x;\nthrow y"));

    EXPECT_TRUE(has(pf("@x;break;"), "@x;\nbreak"));

    EXPECT_TRUE(has(pf("delete x;if(y){}"), "delete x;\nif (y)"));
}

TEST(gt_ascript_compile, PrtFmt_CtrlExt) {

    EXPECT_TRUE(has(pf("return $foo();"), "return $foo"));

    EXPECT_TRUE(has(pf("return .x;"), "return .x"));

    EXPECT_TRUE(has(pf("return ::x;"), "return ::x"));

    EXPECT_TRUE(has(pf("return @x;"), "return @x"));

    EXPECT_TRUE(has(pf("if(x){}else$fff();"), "else $fff"));

    EXPECT_TRUE(has(pf("if(x){}else.foo();"), "else .foo"));

    EXPECT_TRUE(has(pf("if(x){}else::foo();"), "else ::foo"));

    EXPECT_TRUE(has(pf("if(x){}else @x;"), "else @x"));

    EXPECT_TRUE(has(pf("throw $err;"), "throw $err"));

    EXPECT_TRUE(has(pf("delete $bar;"), "delete $bar"));

    EXPECT_TRUE(has(pf("delete .y;"), "delete .y"));

    EXPECT_TRUE(has(pf("if($x){}"), "if ($x)"));

    EXPECT_TRUE(has(pf("while($x){}"), "while ($x)"));

    EXPECT_TRUE(has(pf("for($x;;){}"), "for ($x;"));
}

TEST(gt_ascript_compile, PrtFmt_Bridge) {

    EXPECT_TRUE(has(pf("::a.b.(c);"), "::a.b.(c)"));

    EXPECT_TRUE(has(pf("..a.(f)();"), ".. a.(f)()"));

    EXPECT_TRUE(has(pf(".a.(b);"), ".a.(b)"));
}

static const char* k_hint_json =
    "{\"breaks\": [{\"row\": 3, \"col\": 5}], \"note\": \"调试 hint\"}";

TEST(gt_ascript_compile, Unpack_Plain) {
    auto* eng = engine::create();
    bytes bin = eng->compile(bytes_view(bytes("var x = 42;")), "", false);
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    EXPECT_TRUE(out.value("ast").is<std::string>());
    EXPECT_NE(out.value("ast").to<std::string>().find("VAR"),
              std::string::npos);
    EXPECT_TRUE(out.value("info").is<varmap>());
    EXPECT_FALSE(out.contain("hint"));
    EXPECT_FALSE(out.contain("modules"));
    delete eng;
}

TEST(gt_ascript_compile, Unpack_Compressed) {
    auto* eng = engine::create();
    bytes bin = eng->compile(bytes_view(bytes("var y = 99;")), "", true);
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    EXPECT_NE(out.value("ast").to<std::string>().find("VAR"),
              std::string::npos);
    const varmap& info = out.value("info").to<varmap>();
    EXPECT_TRUE(info.value("info").to<varmap>().value("compressed").to<bool>());
    delete eng;
}

TEST(gt_ascript_compile, Unpack_NotBinary) {
    varmap out;
    alx::bytes garbage("this is not an axp");
    EXPECT_FALSE(engine::unpack(bytes_view(garbage), out));
}

TEST(gt_ascript_compile, Compile_Hint_Roundtrip) {
    auto* eng = engine::create();
    bytes bin = eng->compile(bytes_view(bytes("var x = 42;")), "", false, false, bytes_view(bytes(k_hint_json)));
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    EXPECT_TRUE(out.contain("hint"));
    EXPECT_EQ(out.value("hint").to<std::string>(), k_hint_json);
    delete eng;
}

TEST(gt_ascript_compile, Compile_Hint_Compressed) {
    auto* eng = engine::create();
    bytes bin = eng->compile(bytes_view(bytes("var x = 42;")), "", true, false, bytes_view(bytes(k_hint_json)));
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    EXPECT_TRUE(out.contain("hint"));
    EXPECT_EQ(out.value("hint").to<std::string>(), k_hint_json);
    delete eng;
}

TEST(gt_ascript_compile, Compile_PathHint_Roundtrip) {
    write_tmp("alexis_hint.json", k_hint_json);
    write_tmp("alexis_hint_main.axc", "var x = 1;");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_hint_main.axc"), false, false,
                             tmp_path("alexis_hint.json"));
    rm_tmp("alexis_hint.json");
    rm_tmp("alexis_hint_main.axc");
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    EXPECT_EQ(out.value("hint").to<std::string>(), k_hint_json);
    delete eng;
}

TEST(gt_ascript_compile, Unpack_Embed_Modules) {
    write_tmp("alexis_upk_main.axc", "import \"alexis_upk_mod.axc\" as m; m.foo();");
    write_tmp("alexis_upk_mod.axc", "def foo() { return 1; }");
    auto* eng = engine::create();
    bytes bin = eng->compile(tmp_path("alexis_upk_main.axc"), false, true);
    rm_tmp("alexis_upk_main.axc");
    rm_tmp("alexis_upk_mod.axc");
    ASSERT_FALSE(bin.empty());
    varmap out;
    ASSERT_TRUE(engine::unpack(bytes_view(bin), out));
    ASSERT_TRUE(out.contain("modules"));
    const varmap& mods = out.value("modules").to<varmap>();
    ASSERT_FALSE(mods.empty());
    bool has_at_key = false;
    for (auto& kv : mods) {
        if (kv.first.rfind("@", 0) == 0) has_at_key = true;
        EXPECT_TRUE(kv.second->is<std::string>());
        EXPECT_FALSE(kv.second->to<std::string>().empty());
    }
    EXPECT_TRUE(has_at_key);
    delete eng;
}
