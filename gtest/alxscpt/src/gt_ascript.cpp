/** ****************************************************************
 * \file   gt_ascript.cpp
 * \brief  Script engine integration tests — public API end-to-end
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include "avarsolid.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <filesystem>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
using namespace alx;
using namespace alx::script;

TEST(gt_ascript_engine, OnCSys_ReentrantImport) {

    std::string mod = "tmp-alxscpt-gtest/alx_rsys_mod.axc";
    std::string inner = "tmp-alxscpt-gtest/alx_rsys_inner.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def m() { return 1; }" << std::endl;
    }
    {
        std::ofstream ofs(inner);
        ofs << "def i() { return 2; }" << std::endl;
    }
    auto* eng = engine::create();
    std::atomic<bool> reentered{false};
    std::atomic<bool> in_callback{false};
    eng->get_csys().connect([&](uint_64, const std::string& s) {
        if (s.find("ast load:") != std::string::npos && !in_callback.exchange(true)) {

            auto r = eng->exec(alx::bytes_view(alx::bytes(
                                   "import \"tmp-alxscpt-gtest/alx_rsys_inner.axc\" as x; x.i();")),
                               "");
            if (r.error == error_type::NoError && r.value.to<int_64>() == 2)
                reentered = true;
            in_callback = false;
        }
    });

    auto fut = std::async(std::launch::async, [&] {
        return eng->exec(alx::bytes_view(alx::bytes(
                             "import \"tmp-alxscpt-gtest/alx_rsys_mod.axc\" as m; m.m();")),
                         "");
    });
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "deadlock: reentrant import inside on_csys";
    auto r = fut.get();
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    EXPECT_TRUE(reentered.load());
    eng->get_csys().clear();
    delete eng;
    std::remove(mod.c_str());
    std::remove(inner.c_str());
}

TEST(gt_ascript_engine, Create) {
    engine* eng = engine::create();
    ASSERT_NE(eng, nullptr);

    EXPECT_FALSE(eng->exec(alx::bytes_view(alx::bytes("1;")), "").value.is<varvec>());
    delete eng;
}

TEST(gt_ascript_engine, HotReload_ReimportPicksUpDiskChanges) {

    std::string mod = "tmp-alxscpt-gtest/alx_reload_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def v() { return 1; }" << std::endl;
    }
    auto* eng = engine::create();

    auto run = [&]() {
        return eng->exec(alx::bytes_view(alx::bytes(
                             "import \"tmp-alxscpt-gtest/alx_reload_mod.axc\" as m; m.v();")),
                         "");
    };
    EXPECT_EQ(run().value.to<int_64>(), 1);
    eng->reset();
    EXPECT_EQ(run().value.to<int_64>(), 1);
    eng->reset();

    {
        std::ofstream ofs(mod);
        ofs << "def v() { return 2; }" << std::endl;
    }
    EXPECT_EQ(run().value.to<int_64>(), 2) << "hot reload: disk changes must be picked up";
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, Reset) {

    engine* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes("var x; x = 42;")), "");
    eng->reset();

    variant v = eng->exec(alx::bytes_view(alx::bytes("99;")), "").value;
    EXPECT_EQ(v.to<int_64>(), 99);
    delete eng;
}

TEST(gt_ascript_engine, EngineTypeDefault) {
    engine* eng = engine::create();
    EXPECT_EQ(eng->config().etype, "");
    EXPECT_EQ(eng->config().vtype, 0u);
    delete eng;
}

TEST(gt_ascript_engine, EngineTypeSetGet) {
    engine* eng = engine::create();
    eng->set_etype("myapp");
    eng->set_vtype(42);
    EXPECT_EQ(eng->config().etype, "myapp");
    EXPECT_EQ(eng->config().vtype, 42u);
    delete eng;
}

TEST(gt_ascript_engine, VersionMatch) {
    engine* eng = engine::create();
    eng->set_etype("test");
    eng->set_vtype(10);
    alx::bytes src("42;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_FALSE(bin.empty());
    variant v = eng->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(v.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, Version_EtypeMismatch) {

    engine* eng = engine::create();
    eng->set_etype("test");
    eng->set_vtype(10);
    alx::bytes src("42;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);

    engine* eng2 = engine::create();
    eng2->set_etype("other");
    eng2->set_vtype(10);
    auto res = eng2->exec(alx::bytes_view(bin), "");
    EXPECT_EQ(res.error, error_type::VersionError);
    delete eng;
    delete eng2;
}

TEST(gt_ascript_engine, VersionUniversalAcceptsAny) {
    engine* eng = engine::create();
    eng->set_etype("test");
    eng->set_vtype(10);
    alx::bytes src("99;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    delete eng;

    engine* eng2 = engine::create();
    variant v = eng2->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(v.to<int_64>(), 99);
    delete eng2;
}

TEST(gt_ascript_engine, Version_SameEngineRoundTrip) {

    engine* eng = engine::create();
    eng->set_etype("test");
    eng->set_vtype(10);

    alx::bytes src("77;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    variant v = eng->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(v.to<int_64>(), 77);
    delete eng;
}

TEST(gt_ascript_engine, ScriptDefFunction) {
    engine* eng = engine::create();
    alx::bytes src("def double_it(x) { return x * 2; } double_it(21);");
    variant v = eng->exec(alx::bytes_view(src), "").value;
    EXPECT_EQ(v.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, ExecSimple) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes("42;")), "").value;
    EXPECT_EQ(result.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, ExecVarDecl) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes(
                                   "var x; x = 10; var y; y = 20; x + y;")),
                               "")
                         .value;
    EXPECT_EQ(result.to<int_64>(), 30);
    delete eng;
}

TEST(gt_ascript_engine, ExecString) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes("\"hello world\";")), "").value;
    EXPECT_TRUE(result.is<std::string>());
    EXPECT_EQ(result.to<std::string>(), "hello world");
    delete eng;
}

TEST(gt_ascript_engine, ExecIfElse) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes(
                                   "var x; x = 5; if (x > 3) { 100; } else { 200; }")),
                               "")
                         .value;
    EXPECT_EQ(result.to<int_64>(), 100);
    delete eng;
}

TEST(gt_ascript_engine, ExecWhile) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes(
                                   "var s; s = 0; var i; i = 0; while (i < 5) { s = s + i; i = i + 1; } s;")),
                               "")
                         .value;
    EXPECT_EQ(result.to<int_64>(), 10);
    delete eng;
}

TEST(gt_ascript_engine, ExecFuncDefCall) {
    engine* eng = engine::create();
    variant result = eng->exec(alx::bytes_view(alx::bytes(
                                   "def add(a, b) { return a + b; } add(3, 7);")),
                               "")
                         .value;
    EXPECT_EQ(result.to<int_64>(), 10);
    delete eng;
}

TEST(gt_ascript_engine, ExecCompiledBinary) {
    engine* eng = engine::create();
    alx::bytes bin = eng->compile(alx::bytes_view(alx::bytes("99;")), "", false);
    ASSERT_FALSE(bin.empty());
    variant result = eng->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(result.to<int_64>(), 99);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_IntDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("int a; a;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<int_64>());
    EXPECT_EQ(r.value.to<int_64>(), 0);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_IntWithInit) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("int a = 42; a;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_IntAssign) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("int a; a = 10; a + 5;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 15);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_FloatDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("float b; b;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<double>());
    EXPECT_DOUBLE_EQ(r.value.to<double>(), 0.0);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_FloatWithInit) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("float b = 3.14; b;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_DOUBLE_EQ(r.value.to<double>(), 3.14);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_StringDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("string c; c;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<std::string>());
    EXPECT_EQ(r.value.to<std::string>(), "");
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_StringWithInit) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("string c = \"hello\"; c;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<std::string>(), "hello");
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_BoolDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("bool d; d;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<bool>());
    EXPECT_EQ(r.value.to<bool>(), false);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_BoolWithInit) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("bool d = true; d;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<bool>(), true);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_VecDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("vec v; v;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<varvec>());
    EXPECT_TRUE(r.value.to<varvec>().empty());
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_MapDefault) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("map m; m;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(r.value.is<varmap>());
    EXPECT_TRUE(r.value.to<varmap>().empty());
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_ForEach) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "var s; s = 0; for (int x : [1,2,3]) { s = s + x; } s;")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 6);
    delete eng;
}

TEST(gt_ascript_engine, ExecTypedVar_ForCStyle) {
    engine* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "var s; s = 0; for (int i = 0; i < 5; ++i) { s = s + i; } s;")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 10);
    delete eng;
}

TEST(gt_ascript_engine, ExecCompiledBinary_Compressed) {
    engine* eng = engine::create();
    alx::bytes bin = eng->compile(alx::bytes_view(alx::bytes("42;")), "", true);
    ASSERT_FALSE(bin.empty());
    variant result = eng->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(result.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, Compile) {
    engine* eng = engine::create();
    alx::bytes src("var x; x = 1; x;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_FALSE(bin.empty());

    variant v = eng->exec(alx::bytes_view(bin), "").value;
    EXPECT_EQ(v.to<int_64>(), 1);
    delete eng;
}

TEST(gt_ascript_engine, Compile_Compressed) {
    engine* eng = engine::create();
    alx::bytes bin = eng->compile(alx::bytes_view(alx::bytes("var x; x=1;")), "", true);
    EXPECT_FALSE(bin.empty());
    EXPECT_TRUE(varsolid::is_valid(alx::bytes_view(bin)));
    delete eng;
}

TEST(gt_ascript_engine, OnCompile_LexError) {
    engine* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });

    eng->exec(alx::bytes_view(alx::bytes("\\var x; x = 1;")), "").value;
    ASSERT_GE(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "invalid character: '\\'");
    EXPECT_EQ(errors[0].loc.row, 1u);
    delete eng;
}

TEST(gt_ascript_engine, OnCompile_MultiErrors) {
    engine* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });

    eng->exec(alx::bytes_view(alx::bytes("var x = ; var y = ;")), "").value;
    ASSERT_GE(errors.size(), 2u);
    delete eng;
}

TEST(gt_ascript, Here_BasicMap) {
    auto* eng = engine::create();
    alx::bytes src("here();");
    variant v = eng->exec(alx::bytes_view(src), "").value;
    ASSERT_TRUE(v.is<varmap>());
    auto& m = v.to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 1);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    EXPECT_EQ(m.value("ofst").to<int_64>(), 0);
    EXPECT_EQ(m.value("file").to<std::string>(), "");
    delete eng;
}

TEST(gt_ascript, Here_IndexChain) {
    auto* eng = engine::create();
    alx::bytes src("here()[\"col\"];");
    variant v = eng->exec(alx::bytes_view(src), "").value;
    EXPECT_EQ(v.to<int_64>(), 1);
    delete eng;
}

TEST(gt_ascript, Here_AxpRoundTrip) {
    auto* eng = engine::create();
    alx::bytes src("here();");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_FALSE(bin.empty());
    variant v = eng->exec(alx::bytes_view(bin), "").value;
    ASSERT_TRUE(v.is<varmap>());
    auto& m = v.to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 1);
    EXPECT_EQ(m.value("col").to<int_64>(), 1);
    delete eng;
}

TEST(gt_ascript, Here_FileWrapper) {
    std::string path = "tmp-alxscpt-gtest/alexis_test_here.axc";
    {
        std::ofstream ofs(path);
        ofs << "here();";
    }
    auto* eng = engine::create();
    variant v = eng->exec(path).value;
    std::remove(path.c_str());
    ASSERT_TRUE(v.is<varmap>());
    auto& m = v.to<varmap>();
    EXPECT_EQ(m.value("row").to<int_64>(), 1);
    EXPECT_EQ(m.value("file").to<std::string>(), fs::absolute(path).string());
    delete eng;
}

TEST(gt_ascript, Here_ImportFile) {
    std::string mod_path = "tmp-alxscpt-gtest/alx_here_mod.axc";
    std::string main_path = "tmp-alxscpt-gtest/alx_here_main.axc";
    {
        std::ofstream ofs(mod_path);
        ofs << "def getfile() { return here()[\"file\"]; }";
    }
    {
        std::ofstream ofs(main_path);
        ofs << "import \"tmp-alxscpt-gtest/alx_here_mod.axc\" as m;\nm.getfile();";
    }
    auto* eng = engine::create();
    variant v = eng->exec(main_path).value;
    std::remove(mod_path.c_str());
    std::remove(main_path.c_str());
    EXPECT_EQ(v.to<std::string>(), fs::absolute(mod_path).string());
    delete eng;
}

TEST(gt_ascript_engine, Here_BareError_NoCascade) {

    engine* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });
    auto res = eng->exec(alx::bytes_view(alx::bytes("here;")), "");
    EXPECT_EQ(res.error, error_type::ParseError);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].loc.row, 1u);
    delete eng;
}

TEST(gt_ascript_engine, Here_BareError_AtEOF) {

    engine* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });
    auto res = eng->exec(alx::bytes_view(alx::bytes("here")), "");
    EXPECT_EQ(res.error, error_type::ParseError);
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_NE(errors[0].msg.find("here"), std::string::npos);
    delete eng;
}

TEST(gt_ascript, ExecFileMissing_Error) {

    auto* eng = engine::create();
    auto res = eng->exec("tmp-alxscpt-gtest/alexis_no_such_file.axc");
    EXPECT_EQ(res.error, error_type::RuntimeError);
    delete eng;
}

TEST(gt_ascript_engine, PathsSetGet) {
    engine* eng = engine::create();
    EXPECT_TRUE(eng->config().search_paths.empty());
    eng->set_search_paths({"/a", "/b"});
    EXPECT_EQ(eng->config().search_paths.size(), 2u);
    eng->set_search_paths({});
    EXPECT_TRUE(eng->config().search_paths.empty());
    delete eng;
}

TEST(gt_ascript, ExecTopLevel) {
    auto* eng = engine::create();
    alx::bytes src("42;");
    variant v = eng->exec(alx::bytes_view(src), "").value;
    EXPECT_EQ(v.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript, ExecFilePath) {
    std::string path = "tmp-alxscpt-gtest/alexis_test_script.axc";
    {
        std::ofstream ofs(path);
        ofs << "88;";
    }
    auto* eng = engine::create();
    variant result = eng->exec(path).value;
    std::remove(path.c_str());
    EXPECT_EQ(result.to<int_64>(), 88);
    delete eng;
}

TEST(gt_ascript, CompileBytesView) {
    auto* eng = engine::create();
    alx::bytes bin = eng->compile(alx::bytes_view(alx::bytes("55;")), "", false);
    EXPECT_FALSE(bin.empty());
    EXPECT_TRUE(varsolid::is_valid(alx::bytes_view(bin)));
    delete eng;
}

TEST(gt_ascript, CompileBytesView_Compressed) {
    auto* eng = engine::create();
    alx::bytes bin = eng->compile(alx::bytes_view(alx::bytes("55;")), "", true);
    EXPECT_FALSE(bin.empty());
    delete eng;
}

TEST(gt_ascript, CompileFilePath) {
    std::string path = "tmp-alxscpt-gtest/alexis_test_compile.axc";
    {
        std::ofstream ofs(path);
        ofs << "var z; z = 77; z;";
    }
    auto* eng = engine::create();
    alx::bytes bin = eng->compile(path, false);
    std::remove(path.c_str());
    EXPECT_FALSE(bin.empty());
    delete eng;
}

TEST(gt_ascript_engine, EmbedEndToEnd) {

    std::string mod_path = "tmp-alxscpt-gtest/alx_embed_mod.axc";
    {
        std::ofstream ofs(mod_path);
        ofs << "def double_it(x) { return x * 2; }" << std::endl;
    }

    std::string main_path = "tmp-alxscpt-gtest/alx_embed_main.axc";
    {
        std::ofstream ofs(main_path);
        ofs << "import \"alx_embed_mod.axc\" as mod; mod.double_it(21);";
    }

    auto* eng = engine::create();

    alx::bytes bin = eng->compile(main_path, false, true);

    std::remove(mod_path.c_str());
    std::remove(main_path.c_str());

    ASSERT_FALSE(bin.empty());

    variant result = eng->exec(alx::bytes_view(bin), "").value;
    delete eng;

    EXPECT_EQ(result.to<int_64>(), 42);
}

TEST(gt_ascript_engine, StackError_Direct) {
    auto* eng = engine::create();
    eng->set_max_stack(10);

    auto result = eng->exec(alx::bytes_view(alx::bytes(
                                "def recurse(n) { if (n > 0) { recurse(n - 1); } }"
                                "recurse(200);")),
                            "");
    delete eng;
    EXPECT_EQ(result.error, error_type::StackError);
}

TEST(gt_ascript_engine, LinkError_InvalidSo) {

    std::string so_path = "tmp-alxscpt-gtest/alx_fake_link.so";
    {
        std::ofstream ofs(so_path);
        ofs << "not a valid shared object" << std::endl;
    }

    auto* eng = engine::create();

    auto res = eng->exec(
        alx::bytes_view(alx::bytes("link \"" + so_path + "\" as fake; 1;")), "");
    std::remove(so_path.c_str());
    delete eng;
    EXPECT_EQ(res.error, error_type::ImportError) << "link failure propagates, no crash";
}

TEST(gt_ascript_engine, ParseDepthDefault) {
    auto* eng = engine::create();
    EXPECT_EQ(eng->config().parse_depth, 1024u);
    delete eng;
}

TEST(gt_ascript_engine, ParseDepthSetGet) {
    auto* eng = engine::create();
    eng->set_parse_depth(64);
    EXPECT_EQ(eng->config().parse_depth, 64u);
    delete eng;
}

TEST(gt_ascript_engine, MaxVecfillDefault) {
    auto* eng = engine::create();
    EXPECT_EQ(eng->config().max_vecfill, 0u);
    delete eng;
}

TEST(gt_ascript_engine, MaxVecfillSetGet) {
    auto* eng = engine::create();
    eng->set_max_vecfill(1000);
    EXPECT_EQ(eng->config().max_vecfill, 1000u);
    delete eng;
}

TEST(gt_ascript_engine, IndirectCallIntegration) {
    auto* eng = engine::create();
    auto result = eng->exec(alx::bytes_view(alx::bytes(
                                "def f() { return 42; } var a; a = \"f\"; @a();")),
                            "");
    EXPECT_EQ(result.value.to<int_64>(), 42);
    EXPECT_EQ(result.error, error_type::NoError);
    delete eng;
}

TEST(gt_ascript_engine, FlyweightClosure) {
    std::string tmp = "tmp-alxscpt-gtest/alx_flyweight_test.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "def inc(x) { return x + 1; }" << std::endl;
    }

    std::vector<std::string> events;

    {
        auto* eng = engine::create();
        eng->get_csys().connect([&](uint_64, const std::string& s) { events.push_back(s); });

        auto r1 = eng->exec(alx::bytes_view(alx::bytes(
                                "import \"tmp-alxscpt-gtest/alx_flyweight_test.axc\" as m; m.inc(1);")),
                            "");
        EXPECT_EQ(r1.error, error_type::NoError);
        EXPECT_EQ(r1.value.to<int_64>(), 2);

        auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                                "import \"tmp-alxscpt-gtest/alx_flyweight_test.axc\" as n; n.inc(10);")),
                            "");
        EXPECT_EQ(r2.error, error_type::NoError);
        EXPECT_EQ(r2.value.to<int_64>(), 11);

        eng->exec(alx::bytes_view(alx::bytes(
                      "env([\"/tmp\"]); 1;")),
                  "");

        int load_cnt = 0, unload_cnt = 0, env_cnt = 0;
        for (auto& e : events) {
            if (e.find("ast load:") != std::string::npos) load_cnt++;
            else if (e.find("ast unload:") != std::string::npos) unload_cnt++;
            else if (e.find("env:") != std::string::npos) env_cnt++;
        }
        EXPECT_EQ(load_cnt, 1) << "first import parses once (resource level)";
        EXPECT_EQ(unload_cnt, 0) << "no unload while the engine still holds the resource";
        EXPECT_EQ(env_cnt, 1) << "env() should emit csys event";

        delete eng;
    }

    int load2 = 0, unload2 = 0;
    for (auto& e : events) {
        if (e.find("ast load:") != std::string::npos) load2++;
        else if (e.find("ast unload:") != std::string::npos) unload2++;
    }
    EXPECT_EQ(load2, 1) << "total loads";
    EXPECT_EQ(unload2, 1) << "every load must have matching unload";

    {
        std::ofstream ofs(tmp);
        ofs << "def inc(x) { return x + 100; }" << std::endl;
    }
    {
        auto* eng2 = engine::create();
        std::vector<std::string> ev2;
        eng2->get_csys().connect([&](uint_64, const std::string& s) { ev2.push_back(s); });

        auto r = eng2->exec(alx::bytes_view(alx::bytes(
                                "import \"tmp-alxscpt-gtest/alx_flyweight_test.axc\" as m; m.inc(5);")),
                            "");
        EXPECT_EQ(r.error, error_type::NoError);
        EXPECT_EQ(r.value.to<int_64>(), 105) << "hot reload: fresh parse runs new code";

        int l = 0, u = 0;
        for (auto& e : ev2) {
            if (e.find("ast load:") != std::string::npos) l++;
            else if (e.find("ast unload:") != std::string::npos) u++;
        }
        EXPECT_EQ(l, 1) << "new engine re-parses (resource was evicted)";
        EXPECT_EQ(u, 0) << "no unload while engine alive";

        delete eng2;
        for (auto& e : ev2) {
            if (e.find("ast unload:") != std::string::npos) u++;
        }
        EXPECT_EQ(u, 1) << "engine destroyed → all unloaded";
    }

    std::remove(tmp.c_str());
}

static void ext_hello(fwrap& fw) {
    fw.freturn(variant(std::string("hello")));
}

static void ext_add(fwrap& fw) {
    int_64 a = fw[0].to<int_64>(0);
    int_64 b = fw[1].to<int_64>(0);
    fw.freturn(variant(a + b));
}

static void ext_sq(fwrap& fw) {
    int_64 x = fw[0].to<int_64>(0);
    fw.freturn(variant(x * x));
}

TEST(gt_ascript_engine, Ext_SetLookup) {
    engine* eng = engine::create();
    EXPECT_FALSE(eng->fid_extend("hello"));
    eng->set_extend("hello", ext_hello);
    EXPECT_TRUE(eng->fid_extend("hello"));
    EXPECT_NE(eng->get_extend("hello"), nullptr);
    delete eng;
}

TEST(gt_ascript_engine, Ext_DelLookup) {
    engine* eng = engine::create();
    eng->set_extend("hello", ext_hello);
    EXPECT_TRUE(eng->fid_extend("hello"));
    eng->del_extend("hello");
    EXPECT_FALSE(eng->fid_extend("hello"));
    EXPECT_EQ(eng->get_extend("hello"), nullptr);
    delete eng;
}

TEST(gt_ascript_engine, Ext_Override) {
    engine* eng = engine::create();
    eng->set_extend("fn", ext_hello);
    auto h1 = eng->get_extend("fn");
    eng->set_extend("fn", ext_sq);
    auto h2 = eng->get_extend("fn");
    EXPECT_NE(h1, h2);
    delete eng;
}

TEST(gt_ascript_engine, Ext_CompileCheck) {

    engine* eng = engine::create();
    alx::bytes src("$foo(1);");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_TRUE(bin.empty());
    delete eng;
}

TEST(gt_ascript_engine, Ext_ReservedNameRejected) {

    engine* eng = engine::create();
    EXPECT_FALSE(eng->set_extend("int", ext_hello));
    EXPECT_FALSE(eng->set_extend("var", ext_hello));
    EXPECT_FALSE(eng->set_extend("here", ext_hello));
    EXPECT_FALSE(eng->fid_extend("int"));

    EXPECT_TRUE(eng->set_extend("foo", ext_hello));
    EXPECT_TRUE(eng->fid_extend("foo"));
    delete eng;
}

TEST(gt_ascript_engine, Ext_ExecE2E) {
    engine* eng = engine::create();
    eng->set_extend("hello", ext_hello);
    alx::bytes src("$hello();");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "hello");
    delete eng;
}

struct type_probe_host {};

static void ext_host_handle(fwrap& fw) {
    fw.freturn(variant(anyptr_ex<type_probe_host>::make(new type_probe_host())));
}

static void ext_unmodeled_type(fwrap& fw) {
    fw.freturn(variant(1.5f));
}

TEST(gt_ascript_engine, Ext_TypeOfHostHandle) {
    engine* eng = engine::create();
    eng->set_extend("host_handle", ext_host_handle);
    alx::bytes src("var h; h = $host_handle(); type(h) + \"!\";");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "unknown!");
    delete eng;
}

TEST(gt_ascript_engine, Ext_TypeOfUnmodeledValue) {
    engine* eng = engine::create();
    eng->set_extend("odd_value", ext_unmodeled_type);
    alx::bytes src("var v; v = $odd_value(); type(v);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "unknown");
    delete eng;
}

static const char* type_ex_namer(const variant& _v, void* _ud) {
    int* asked = static_cast<int*>(_ud);
    if (asked) (*asked)++;
    if (_v.is<float>()) return "float32";
    if (_v.is<anyptr>()) return "probe";
    return "";
}

static const char* type_ex_empty(const variant&, void* _ud) {
    int* asked = static_cast<int*>(_ud);
    if (asked) (*asked)++;
    return "";
}

TEST(gt_ascript_engine, Ext_TypeExNamesUnmodeled) {
    engine* eng = engine::create();
    eng->set_extend("odd_value", ext_unmodeled_type);
    int asked = 0;
    eng->set_type_ex(type_ex_namer, &asked);
    alx::bytes src("var v; v = $odd_value(); type(v);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "float32");
    EXPECT_EQ(asked, 1);
    delete eng;
}

TEST(gt_ascript_engine, Ext_TypeExNamesHostObject) {
    engine* eng = engine::create();
    eng->set_extend("host_handle", ext_host_handle);
    int asked = 0;
    eng->set_type_ex(type_ex_namer, &asked);
    alx::bytes src("var h; h = $host_handle(); type(h);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "probe");
    EXPECT_EQ(asked, 1);
    delete eng;
}

TEST(gt_ascript_engine, Ext_TypeExEmptyAnswerKeepsUnknown) {
    engine* eng = engine::create();
    eng->set_extend("odd_value", ext_unmodeled_type);
    eng->set_extend("host_handle", ext_host_handle);
    int asked = 0;
    eng->set_type_ex(type_ex_empty, &asked);
    alx::bytes src("var v; v = $odd_value(); var h; h = $host_handle(); type(v) + type(h);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "unknownunknown");
    EXPECT_EQ(asked, 2);
    delete eng;
}

TEST(gt_ascript_engine, Ext_TypeExRemoved) {
    engine* eng = engine::create();
    eng->set_extend("odd_value", ext_unmodeled_type);
    int asked = 0;
    eng->set_type_ex(type_ex_namer, &asked);
    eng->set_type_ex(nullptr);
    alx::bytes src("var v; v = $odd_value(); type(v);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "unknown");
    EXPECT_EQ(asked, 0);
    delete eng;
}

TEST(gt_ascript_engine, Ext_TypeExNotAskedForModeled) {
    engine* eng = engine::create();
    int asked = 0;
    eng->set_type_ex(type_ex_namer, &asked);
    alx::bytes src("type(42) + type(null) + type(\"s\") + type([1]);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "intnullstringvec");
    EXPECT_EQ(asked, 0);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ExecArgs) {
    engine* eng = engine::create();
    eng->set_extend("add", ext_add);
    eng->set_extend("sq", ext_sq);
    alx::bytes src("$add(3, 4);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 7);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ExecInExpr) {
    engine* eng = engine::create();
    eng->set_extend("sq", ext_sq);
    alx::bytes src("10 + $sq(5);");
    auto res = eng->exec(alx::bytes_view(src), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 35);
    delete eng;
}

TEST(gt_ascript_engine, Ext_RuntimeNotFound) {

    engine* eng = engine::create();
    eng->set_extend("hello", ext_hello);
    alx::bytes src("$hello();");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_FALSE(bin.empty());

    eng->del_extend("hello");
    auto res = eng->exec(alx::bytes_view(bin), "");
    EXPECT_EQ(res.error, error_type::NameError);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ReflectCallFastPath) {
    engine* eng = engine::create();
    eng->set_extend("sq", ext_sq);

    auto res = eng->exec(alx::bytes_view(alx::bytes("@(\"$sq\")(4);")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 16);

    res = eng->exec(alx::bytes_view(alx::bytes("var n = \"$sq\"; @(n)(5);")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 25);

    res = eng->exec(alx::bytes_view(alx::bytes("@(\"$sq(4)\");")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 16);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ReflectCallMissing) {

    engine* eng = engine::create();
    auto res = eng->exec(alx::bytes_view(alx::bytes("@(\"$no_such_ext\")(1);")), "");
    EXPECT_EQ(res.error, error_type::NameError);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ReflectReadAsValue) {

    engine* eng = engine::create();
    eng->set_extend("sq", ext_sq);
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    auto res = eng->exec(alx::bytes_view(alx::bytes("@(\"$sq\");")), "");
    EXPECT_EQ(res.error, error_type::TypeError);
    EXPECT_NE(err.find("cannot be read as value"), std::string::npos);
    delete eng;
}

TEST(gt_ascript_engine, AssignTarget_DynamicPath) {

    engine* eng = engine::create();
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    auto bad = eng->exec(alx::bytes_view(alx::bytes("@(\"1+2\") = 5;")), "");
    EXPECT_EQ(bad.error, error_type::TypeError);
    EXPECT_NE(err.find("assignment target"), std::string::npos);

    EXPECT_EQ(eng->exec(alx::bytes_view(alx::bytes("@(\"1\") = 5;")), "").error, error_type::TypeError);

    auto undef = eng->exec(alx::bytes_view(alx::bytes("var k = \"nope\"; @(k) = 1;")), "");
    EXPECT_EQ(undef.error, error_type::NameError);

    auto ok = eng->exec(alx::bytes_view(alx::bytes("var v = [7]; var k = \"v[0]\"; @(k) = 8;")), "");
    EXPECT_EQ(ok.error, error_type::NoError);
    delete eng;
}

TEST(gt_ascript_engine, Trace_UncaughtWithStack) {

    engine* eng = engine::create();
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    alx::bytes src("def f() { throw(\"boom\"); } def g() { f(); } def h() { g(); } h();");
    eng->exec(alx::bytes_view(src), "");
    EXPECT_NE(err.find("Uncaught"), std::string::npos);
    EXPECT_NE(err.find("f"), std::string::npos);
    EXPECT_NE(err.find("g"), std::string::npos);
    EXPECT_NE(err.find("h"), std::string::npos);
    EXPECT_NE(err.find("[::]"), std::string::npos);
    delete eng;
}

TEST(gt_ascript_engine, Trace_RecursiveFold) {

    engine* eng = engine::create();
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    alx::bytes src("def f(n) { f(n + 1); } f(0);");
    eng->exec(alx::bytes_view(src), "");
    EXPECT_NE(err.find("StackError"), std::string::npos);
    EXPECT_NE(err.find("f"), std::string::npos);

    EXPECT_NE(err.find("x)"), std::string::npos);
    delete eng;
}

TEST(gt_ascript_engine, Def_CompileFold) {
    engine* eng = engine::create();
    EXPECT_TRUE(eng->set_define("cfg_max", variant(int_64(42))));
    EXPECT_TRUE(eng->set_define("cfg_name", variant(std::string("alexis"))));
    EXPECT_TRUE(eng->set_define("cfg_list", variant(varvec{variant(int_64(1)), variant(int_64(2))})));
    varmap m;
    m["k"] = variant(int_64(7));
    EXPECT_TRUE(eng->set_define("cfg_map", variant(m)));

    auto res = eng->exec(alx::bytes_view(alx::bytes("$cfg_max + 1;")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 43);

    res = eng->exec(alx::bytes_view(alx::bytes("$cfg_list[0] + $cfg_list[1];")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 3);

    res = eng->exec(alx::bytes_view(alx::bytes("$cfg_name + \"!\";")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "alexis!");

    res = eng->exec(alx::bytes_view(alx::bytes("$cfg_map[\"k\"];")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 7);
    delete eng;
}

TEST(gt_ascript_engine, Def_NotFoundCompileError) {
    engine* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });

    alx::bytes src("$no_such_define;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_TRUE(bin.empty());
    ASSERT_GE(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "undefined static definition: $no_such_define");

    eng->set_extend("print", ext_hello);
    errors.clear();
    bytes bin2 = eng->compile(alx::bytes_view(alx::bytes("$print;")), "", false);
    EXPECT_TRUE(bin2.empty());
    ASSERT_GE(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "extension function cannot be read as value: $print");
    delete eng;
}

TEST(gt_ascript_engine, Def_SharedNamespaceMutualExclusion) {
    engine* eng = engine::create();
    eng->set_extend("x", ext_hello);
    EXPECT_FALSE(eng->set_define("x", variant(int_64(1))));
    EXPECT_TRUE(eng->set_define("y", variant(int_64(1))));
    EXPECT_FALSE(eng->set_extend("y", ext_hello));

    EXPECT_FALSE(eng->set_define("int", variant(int_64(1))));
    EXPECT_FALSE(eng->set_define("var", variant(int_64(1))));
    EXPECT_FALSE(eng->fid_define("x"));
    EXPECT_TRUE(eng->fid_define("y"));
    EXPECT_TRUE(eng->fid_extend("x"));
    delete eng;
}

TEST(gt_ascript_engine, Def_CompiledSelfContained) {

    engine* eng = engine::create();
    EXPECT_TRUE(eng->set_define("cfg_max", variant(int_64(42))));
    alx::bytes src("$cfg_max * 2;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);
    EXPECT_FALSE(bin.empty());
    eng->del_define("cfg_max");
    EXPECT_FALSE(eng->fid_define("cfg_max"));
    auto res = eng->exec(alx::bytes_view(bin), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 84);
    delete eng;
}

TEST(gt_ascript_engine, Def_ReflectRead) {

    engine* eng = engine::create();
    eng->set_define("cfg_max", variant(int_64(42)));
    auto res = eng->exec(alx::bytes_view(alx::bytes("@(\"$cfg_max\");")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 42);

    res = eng->exec(alx::bytes_view(alx::bytes("var n = \"$cfg_max\"; @(n);")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 42);

    auto res2 = eng->exec(alx::bytes_view(alx::bytes("@(\"$no_such_def\");")), "");
    EXPECT_EQ(res2.error, error_type::NameError);
    delete eng;
}

TEST(gt_ascript_engine, Ext_ReflectCallBadName) {

    engine* eng = engine::create();
    auto res = eng->exec(alx::bytes_view(alx::bytes("@(\"$sq(4)\")();")), "");
    EXPECT_EQ(res.error, error_type::NameError);
    delete eng;
}

TEST(gt_ascript_engine, Version_AxpTooNew) {

    engine* eng = engine::create();
    eng->set_etype("test");
    eng->set_vtype(5);
    alx::bytes src("42;");
    bytes bin = eng->compile(alx::bytes_view(src), "", false);

    engine* eng2 = engine::create();
    eng2->set_etype("test");
    eng2->set_vtype(3);
    auto res = eng2->exec(alx::bytes_view(bin), "");
    EXPECT_EQ(res.error, error_type::VersionError);
    delete eng;
    delete eng2;
}

namespace {

    struct start_gate {
        std::mutex mtx;
        std::condition_variable cv;
        int ready = 0;
        bool go = false;
        int total;

        explicit start_gate(int _total) : total(_total) {}
        void wait() {
            std::unique_lock<std::mutex> lk(mtx);
            if (++ready == total) {
                go = true;
                cv.notify_all();
            }
            cv.wait(lk, [&] { return go; });
        }
    };

}

TEST(gt_ascript_engine, MultiThreadSharedPool) {

    std::string tmp = "tmp-alxscpt-gtest/alx_mt_pool.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "var calls; calls = 0;" << std::endl
            << "def inc(x) { calls = calls + 1; return x + 1; }" << std::endl
            << "def get_calls() { return calls; }" << std::endl;
    }

    constexpr int kThreads = 8;
    constexpr int kIters = 40;
    std::atomic<int> failures{0};
    std::atomic<int> loads{0};
    std::atomic<int> unloads{0};

    auto* obs = engine::create();
    obs->get_csys().connect([&](uint_64, const std::string& s) {
        if (s.find("ast load:") != std::string::npos) ++loads;
        else if (s.find("ast unload:") != std::string::npos) ++unloads;
    });

    std::vector<std::thread> threads;
    start_gate gate(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            gate.wait();
            auto* eng = engine::create();
            for (int i = 0; i < kIters; ++i) {

                std::string src = "import \"tmp-alxscpt-gtest/alx_mt_pool.axc\" as m" +
                                  std::to_string(i) + "; m" + std::to_string(i) + ".inc(1);";
                auto r = eng->exec(alx::bytes_view(alx::bytes(src)), "");
                if (r.error != error_type::NoError || r.value.to<int_64>() != 2) ++failures;
            }
            delete eng;
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(failures.load(), 0) << "all concurrent imports must succeed";

    EXPECT_GE(loads.load(), 1) << "module parsed at least once";
    EXPECT_EQ(unloads.load(), loads.load()) << "every load has a matching unload";
    obs->get_csys().clear();
    delete obs;
    std::remove(tmp.c_str());
}

TEST(gt_ascript_engine, MultiThreadCircularImport) {

    std::string a = "tmp-alxscpt-gtest/alx_mt_a.axc";
    std::string b = "tmp-alxscpt-gtest/alx_mt_b.axc";
    {
        std::ofstream ofs(a);
        ofs << "import \"tmp-alxscpt-gtest/alx_mt_b.axc\" as b; def fa() { return 1; }" << std::endl;
        std::ofstream ofs2(b);
        ofs2 << "import \"tmp-alxscpt-gtest/alx_mt_a.axc\" as a; def fb() { return 2; }" << std::endl;
    }

    constexpr int kThreads = 6;
    std::atomic<int> import_errors{0};
    std::atomic<int> other_failures{0};
    std::vector<std::thread> threads;
    start_gate gate(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            gate.wait();
            auto* eng = engine::create();
            for (int i = 0; i < 20; ++i) {
                auto r = eng->exec(alx::bytes_view(alx::bytes(
                                       "import \"tmp-alxscpt-gtest/alx_mt_a.axc\" as a; 1;")),
                                   "");
                if (r.error == error_type::ImportError) ++import_errors;
                else ++other_failures;
            }
            delete eng;
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(other_failures.load(), 0) << "all failures must be ImportError";
    EXPECT_EQ(import_errors.load(), kThreads * 20) << "every import reports the cycle";
    std::remove(a.c_str());
    std::remove(b.c_str());
}

TEST(gt_ascript_engine, MultiThreadCircularImportBidi) {

    std::string a = "tmp-alxscpt-gtest/alx_mt_bidi_a.axc";
    std::string b = "tmp-alxscpt-gtest/alx_mt_bidi_b.axc";
    {
        std::ofstream ofs(a);
        ofs << "import \"tmp-alxscpt-gtest/alx_mt_bidi_b.axc\" as b; def fa() { return 1; }" << std::endl;
        std::ofstream ofs2(b);
        ofs2 << "import \"tmp-alxscpt-gtest/alx_mt_bidi_a.axc\" as a; def fb() { return 2; }" << std::endl;
    }

    constexpr int kThreads = 2;
    std::atomic<int> import_errors{0};
    std::atomic<int> other_failures{0};
    std::vector<std::future<void>> futs;
    start_gate gate(kThreads);
    const char* mods[kThreads] = {"tmp-alxscpt-gtest/alx_mt_bidi_a.axc", "tmp-alxscpt-gtest/alx_mt_bidi_b.axc"};

    for (int t = 0; t < kThreads; ++t) {
        futs.emplace_back(std::async(std::launch::async, [&, t] {
            gate.wait();
            auto* eng = engine::create();
            for (int i = 0; i < 20; ++i) {
                auto r = eng->exec(alx::bytes_view(alx::bytes(
                                       std::string("import \"") + mods[t] + "\" as m; 1;")),
                                   "");
                if (r.error == error_type::ImportError) ++import_errors;
                else ++other_failures;
            }
            delete eng;
        }));
    }
    for (auto& f : futs) {
        EXPECT_EQ(f.wait_for(std::chrono::seconds(5)), std::future_status::ready)
            << "bidirectional ring must not deadlock";
    }
    EXPECT_EQ(other_failures.load(), 0) << "all failures must be ImportError";
    EXPECT_EQ(import_errors.load(), kThreads * 20) << "every import reports the cycle";
    std::remove(a.c_str());
    std::remove(b.c_str());
}

TEST(gt_ascript_engine, MultiThreadCircularImport3) {

    std::string a = "tmp-alxscpt-gtest/alx_mt3_a.axc";
    std::string b = "tmp-alxscpt-gtest/alx_mt3_b.axc";
    std::string c = "tmp-alxscpt-gtest/alx_mt3_c.axc";
    {
        std::ofstream ofs(a);
        ofs << "import \"tmp-alxscpt-gtest/alx_mt3_b.axc\" as b; def fa() { return 1; }" << std::endl;
        std::ofstream ofs2(b);
        ofs2 << "import \"tmp-alxscpt-gtest/alx_mt3_c.axc\" as c; def fb() { return 2; }" << std::endl;
        std::ofstream ofs3(c);
        ofs3 << "import \"tmp-alxscpt-gtest/alx_mt3_a.axc\" as a; def fc() { return 3; }" << std::endl;
    }

    constexpr int kThreads = 3;
    std::atomic<int> import_errors{0};
    std::atomic<int> other_failures{0};
    std::vector<std::future<void>> futs;
    start_gate gate(kThreads);
    const char* mods[kThreads] = {"tmp-alxscpt-gtest/alx_mt3_a.axc", "tmp-alxscpt-gtest/alx_mt3_b.axc", "tmp-alxscpt-gtest/alx_mt3_c.axc"};

    for (int t = 0; t < kThreads; ++t) {
        futs.emplace_back(std::async(std::launch::async, [&, t] {
            gate.wait();
            auto* eng = engine::create();
            for (int i = 0; i < 20; ++i) {
                auto r = eng->exec(alx::bytes_view(alx::bytes(
                                       std::string("import \"") + mods[t] + "\" as m; 1;")),
                                   "");
                if (r.error == error_type::ImportError) ++import_errors;
                else ++other_failures;
            }
            delete eng;
        }));
    }
    for (auto& f : futs) {
        EXPECT_EQ(f.wait_for(std::chrono::seconds(5)), std::future_status::ready)
            << "3-thread ring must not deadlock";
    }
    EXPECT_EQ(other_failures.load(), 0) << "all failures must be ImportError";
    EXPECT_EQ(import_errors.load(), kThreads * 20) << "every import reports the cycle";
    std::remove(a.c_str());
    std::remove(b.c_str());
    std::remove(c.c_str());
}

TEST(gt_ascript_engine, MultiThreadBadPropagation) {

    std::string x = "tmp-alxscpt-gtest/alx_mt_bad_x.axc";
    {
        std::ofstream ofs(x);
        ofs << "import \"tmp-alxscpt-gtest/alx_mt_bad_missing.axc\" as m; def fx() { return 1; }" << std::endl;
    }

    constexpr int kThreads = 2;
    std::atomic<int> import_errors{0};
    std::atomic<int> other_failures{0};
    std::vector<std::future<void>> futs;
    start_gate gate(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        futs.emplace_back(std::async(std::launch::async, [&] {
            gate.wait();
            auto* eng = engine::create();
            for (int i = 0; i < 20; ++i) {
                auto r = eng->exec(alx::bytes_view(alx::bytes(
                                       "import \"tmp-alxscpt-gtest/alx_mt_bad_x.axc\" as m; 1;")),
                                   "");
                if (r.error == error_type::ImportError) ++import_errors;
                else ++other_failures;
            }
            delete eng;
        }));
    }
    for (auto& f : futs) {
        EXPECT_EQ(f.wait_for(std::chrono::seconds(5)), std::future_status::ready)
            << "bad propagation must not deadlock";
    }
    EXPECT_EQ(other_failures.load(), 0) << "all failures must be ImportError";
    EXPECT_EQ(import_errors.load(), kThreads * 20) << "every import reports the failure";
    std::remove(x.c_str());
}

TEST(gt_ascript_engine, MultiThreadFlyLifecycle) {

    std::string tmp = "tmp-alxscpt-gtest/alx_mt_life.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "def f() { return 7; }" << std::endl;
    }

    std::vector<std::string> events;
    auto* obs = engine::create();
    obs->get_csys().connect([&](uint_64, const std::string& s) { events.push_back(s); });
    {
        auto* eng = engine::create();
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"tmp-alxscpt-gtest/alx_mt_life.axc\" as m; m.f();")),
                           "");
        EXPECT_EQ(r.error, error_type::NoError);
        EXPECT_EQ(r.value.to<int_64>(), 7);
        delete eng;
    }
    {
        auto* eng = engine::create();
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"tmp-alxscpt-gtest/alx_mt_life.axc\" as m; m.f();")),
                           "");
        EXPECT_EQ(r.error, error_type::NoError);
        EXPECT_EQ(r.value.to<int_64>(), 7);
        delete eng;
    }
    int load = 0, unload = 0;
    for (auto& e : events) {
        if (e.find("ast load:") != std::string::npos) ++load;
        else if (e.find("ast unload:") != std::string::npos) ++unload;
    }
    EXPECT_EQ(load, 2) << "each engine generation parses the module once";
    EXPECT_EQ(unload, 2) << "each resource generation evicts once (hot reload path)";
    obs->get_csys().clear();
    delete obs;
    std::remove(tmp.c_str());
}

TEST(gt_ascript_engine, MultiThreadEventTracking) {

    std::string tmp = "tmp-alxscpt-gtest/alx_mt_evt.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "def f() { return 1; }" << std::endl;
    }

    constexpr int kThreads = 6;
    struct tracker {
        std::mutex mtx;
        std::vector<std::string> ev;
    };
    std::vector<std::unique_ptr<tracker>> tr;
    for (int i = 0; i < kThreads; ++i) tr.push_back(std::make_unique<tracker>());

    start_gate gate(kThreads);
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            gate.wait();
            auto* eng = engine::create();

            uint_64 my_tid = this_tid();
            eng->get_csys().connect([&, t, my_tid](uint_64 tid_, const std::string& s) {
                if (tid_ != my_tid) return;
                std::lock_guard<std::mutex> lk(tr[t]->mtx);
                tr[t]->ev.push_back(s);
            });

            eng->exec(alx::bytes_view(alx::bytes(
                          "import \"tmp-alxscpt-gtest/alx_mt_evt.axc\" as m1; m1.f();")),
                      "");
            eng->exec(alx::bytes_view(alx::bytes(
                          "import \"tmp-alxscpt-gtest/alx_mt_evt.axc\" as m2; m2.f();")),
                      "");
            delete eng;
        });
    }
    for (auto& th : threads) th.join();

    int load_total = 0, unload_total = 0;
    for (int t = 0; t < kThreads; ++t) {
        int load = 0, unload = 0;
        std::lock_guard<std::mutex> lk(tr[t]->mtx);
        for (auto& e : tr[t]->ev) {
            if (e.find("ast load:") != std::string::npos) ++load;
            else if (e.find("ast unload:") != std::string::npos) ++unload;
        }

        load_total += load;
        unload_total += unload;
    }
    EXPECT_GE(load_total, 1) << "module parsed at least once";
    EXPECT_EQ(load_total, unload_total) << "every load has a matching unload";
    std::remove(tmp.c_str());
}

TEST(gt_ascript_engine, MultiThreadErrorCleanup) {

    std::string tmp = "tmp-alxscpt-gtest/alx_mt_err.axc";
    {
        std::ofstream ofs(tmp);
        ofs << "throw \"boom\";" << std::endl;
    }

    constexpr int kThreads = 4;
    constexpr int kIters = 10;

    std::atomic<int> import_errors{0};
    std::atomic<int> wrong_errors{0};
    std::atomic<int> loads{0};
    std::atomic<int> unloads{0};
    std::atomic<int> cerr_msgs{0};
    std::vector<std::thread> threads;
    start_gate gate(kThreads);

    auto* obs = engine::create();
    obs->get_csys().connect([&](uint_64, const std::string& s) {
        if (s.find("ast load:") != std::string::npos) ++loads;
        else if (s.find("ast unload:") != std::string::npos) ++unloads;
    });
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            gate.wait();
            auto* eng = engine::create();
            eng->on_cerr.connect([&](const std::string&) { ++cerr_msgs; });
            for (int i = 0; i < kIters; ++i) {
                auto r = eng->exec(alx::bytes_view(alx::bytes(
                                       "import \"tmp-alxscpt-gtest/alx_mt_err.axc\" as m; 1;")),
                                   "");
                if (r.error == error_type::ImportError) ++import_errors;
                else ++wrong_errors;
            }
            delete eng;
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(wrong_errors.load(), 0) << "every import fails with ImportError (wrapped)";
    EXPECT_EQ(import_errors.load(), kThreads * kIters);

    EXPECT_GE(loads.load(), 1) << "module parsed at least once";
    EXPECT_EQ(unloads.load(), loads.load()) << "every load has a matching unload";
    EXPECT_GT(cerr_msgs.load(), 0) << "errors surfaced via on_cerr";

    {
        std::ofstream ofs(tmp);
        ofs << "def f() { return 42; }" << std::endl;
    }
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_mt_err.axc\" as m; m.f();")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 42) << "hot reload: fixed file loads fresh";
    obs->get_csys().clear();
    delete eng;
    delete obs;
    std::remove(tmp.c_str());
}

struct hook_rec {
    uint_64 budget = 0;
    uint_64 calls = 0;
    uint_64 last_insn = 0;
    bool interrupted = false;
};

static bool test_hook(hook_info& info) {
    hook_rec* r = static_cast<hook_rec*>(info.hkdt);
    ++r->calls;
    r->last_insn = info.info.to<int_64>(0);
    if (r->budget && info.info.to<int_64>(0) >= static_cast<int_64>(r->budget)) {
        r->interrupted = true;
        *info.desc = "budget exhausted";
        return false;
    }
    return true;
}

TEST(gt_ascript_engine, HookLongLoopBudget) {
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 500;
    eng->set_hook(test_hook, &rec, 100);
    auto res = eng->exec(alx::bytes_view(alx::bytes("var i = 0; while (1) { i = i + 1; }")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError);
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — budget exhausted")
        << "reject reason (desc) propagates into the result";
    EXPECT_TRUE(rec.interrupted);
    EXPECT_GE(rec.calls, 5u);
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, HookInFlightThrow) {
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 2;
    eng->set_hook(test_hook, &rec, 1);
    auto res = eng->exec(alx::bytes_view(alx::bytes("1 + (2 + 3);")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError)
        << "in-flight op throw must not mask the interrupt";
    EXPECT_TRUE(rec.interrupted);
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, HookTryUncatchable) {
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 3;
    eng->set_hook(test_hook, &rec, 1);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "var x = 0; try { 1 + 2; } catch(e) { x = 1; }")),
                         "");
    EXPECT_EQ(res.error, error_type::InterruptedError)
        << "script try/catch cannot swallow an interrupt";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, HookOffRestore) {
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 50;
    eng->set_hook(test_hook, &rec, 10);
    auto res = eng->exec(alx::bytes_view(alx::bytes("var i = 0; while (1) { i = i + 1; }")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError);

    eng->set_hook(nullptr, nullptr, 0);
    auto res2 = eng->exec(alx::bytes_view(alx::bytes(
                              "var i = 0; while (i < 1000) { i = i + 1; }\ni;")),
                          "");
    EXPECT_EQ(res2.error, error_type::NoError);
    EXPECT_EQ(res2.value.to<int_64>(), 1000);
    delete eng;
}

TEST(gt_ascript_engine, HookContinue) {
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 0;
    eng->set_hook(test_hook, &rec, 5);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "var s = 0; for (var i = 0; i < 100; i++) { s = s + i; }\ns;")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 4950);
    EXPECT_GT(rec.calls, 0u);
    EXPECT_EQ(rec.last_insn, rec.calls) << "insn_count = checkpoint ordinal (one per interval)";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, HookModuleInitInterrupt) {
    std::string mod = "tmp-alxscpt-gtest/alx_hook_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "var M = 42;\nvar i = 0; while (i < 400) { i = i + 1; }" << std::endl;
    }
    auto* eng = engine::create();
    hook_rec rec;
    rec.budget = 60;
    eng->set_hook(test_hook, &rec, 10);

    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "import \"tmp-alxscpt-gtest/alx_hook_mod.axc\" as m;\nm.M;")),
                         "");
    EXPECT_EQ(res.error, error_type::InterruptedError)
        << "interrupted module init surfaces as InterruptedError (same-walker short-circuit)";
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — budget exhausted")
        << "interrupt reason rides along (init -> exec exit, same engine)";

    eng->set_hook(nullptr, nullptr, 0);
    auto res2 = eng->exec(alx::bytes_view(alx::bytes(
                              "import \"tmp-alxscpt-gtest/alx_hook_mod.axc\" as m;\nm.M;")),
                          "");
    EXPECT_EQ(res2.error, error_type::NoError);
    EXPECT_EQ(res2.value.to<int_64>(), 42);
    delete eng;
    std::remove(mod.c_str());
}

struct gate_rec {
    std::string reject_containing;
    std::string reason = "blocked by policy";
    std::string last_path;
    uint_64 import_events = 0;
    uint_64 link_events = 0;
    uint_64 exec_events = 0;
};

static bool gate_hook(hook_info& info) {
    gate_rec* r = static_cast<gate_rec*>(info.hkdt);
    if (info.type == hook_event::exec) {
        ++r->exec_events;
        return true;
    }
    r->last_path = info.info.to<std::string>();
    if (info.type == hook_event::import) {
        ++r->import_events;
    } else {
        ++r->link_events;
    }
    if (!r->reject_containing.empty() &&
        r->last_path.find(r->reject_containing) != std::string::npos) {
        *info.desc = r->reason;
        return false;
    }
    return true;
}

TEST(gt_ascript_engine, HookImportGateReject) {
    std::string mod = "tmp-alxscpt-gtest/alx_gate_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "var M = 42;" << std::endl;
    }
    auto* eng = engine::create();
    gate_rec rec;
    rec.reject_containing = "alx_gate_mod";
    eng->set_hook(gate_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "import \"tmp-alxscpt-gtest/alx_gate_mod.axc\" as m;\nm.M;")),
                         "");
    EXPECT_EQ(res.error, error_type::InterruptedError)
        << "any hook false = interrupt (uncatchable), not a catchable ImportError";
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — blocked by policy")
        << "reject reason (hook-written desc) surfaces in the interrupt";
    EXPECT_EQ(rec.import_events, 1u);
    EXPECT_EQ(rec.last_path, fs::absolute("tmp-alxscpt-gtest/alx_gate_mod.axc").string()) << "gate receives the resolved path";
    EXPECT_EQ(rec.exec_events, 0u) << "interval=0 -> no exec checkpoint events";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, GateIsolationPerEngine) {

    std::string x = "tmp-alxscpt-gtest/alx_gate_x.axc";
    std::string y = "tmp-alxscpt-gtest/alx_gate_y.axc";
    {
        std::ofstream ofs(x);
        ofs << "import \"tmp-alxscpt-gtest/alx_gate_y.axc\" as y; def fx() { return y.Y; }" << std::endl;
        std::ofstream ofs2(y);
        ofs2 << "var Y = 1;" << std::endl;
    }

    {
        auto* eng = engine::create();
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"tmp-alxscpt-gtest/alx_gate_x.axc\" as x; x.fx();")),
                           "");
        EXPECT_EQ(r.error, error_type::NoError);
        EXPECT_EQ(r.value.to<int_64>(), 1);
        delete eng;
    }

    {
        auto* eng = engine::create();
        gate_rec rec;
        rec.reject_containing = "alx_gate_y";
        eng->set_hook(gate_hook, &rec, 0);
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"tmp-alxscpt-gtest/alx_gate_x.axc\" as x; x.fx();")),
                           "");
        EXPECT_EQ(r.error, error_type::InterruptedError)
            << "dependency closure of X contains banned Y -> nested gate false = interrupt";
        EXPECT_EQ(rec.last_path, fs::absolute("tmp-alxscpt-gtest/alx_gate_y.axc").string())
            << "the gate saw the banned dependency path";
        eng->set_hook(nullptr, nullptr, 0);
        delete eng;
    }

    {
        auto* eng = engine::create();
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"tmp-alxscpt-gtest/alx_gate_x.axc\" as x; x.fx();")),
                           "");
        EXPECT_EQ(r.error, error_type::NoError);
        EXPECT_EQ(r.value.to<int_64>(), 1);
        delete eng;
    }

    std::remove(x.c_str());
    std::remove(y.c_str());
}

TEST(gt_ascript_engine, HookGateResolvedPath) {
    std::string mod = "tmp-alxscpt-gtest/alx_gate_resolve.axc";
    {
        std::ofstream ofs(mod);
        ofs << "var M = 7;" << std::endl;
    }
    auto* eng = engine::create();
    eng->set_search_paths({"tmp-alxscpt-gtest"});
    gate_rec rec;
    rec.reject_containing = "alx_gate_resolve";
    eng->set_hook(gate_hook, &rec, 0);

    auto res = eng->exec(alx::bytes_view(alx::bytes("import \"alx_gate_resolve.axc\" as m;")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError);
    EXPECT_EQ(rec.last_path, fs::absolute("tmp-alxscpt-gtest/alx_gate_resolve.axc").string());
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, HookLinkGateReject) {

    std::string so_path = "tmp-alxscpt-gtest/alx_gate_any_so.so";
    {
        std::ofstream ofs(so_path);
        ofs << "not a valid shared object" << std::endl;
    }
    auto* eng = engine::create();
    gate_rec rec;
    rec.reject_containing = "alx_gate_any_so";
    eng->set_hook(gate_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "link \"" + so_path + "\" as fake;")),
                         "");
    EXPECT_EQ(res.error, error_type::InterruptedError) << "link reject = interrupt (firewall)";
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — blocked by policy");
    EXPECT_EQ(rec.link_events, 1u);
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
    std::remove(so_path.c_str());
}

TEST(gt_ascript_engine, HookGateOffRestore) {
    std::string mod = "tmp-alxscpt-gtest/alx_gate_off.axc";
    {
        std::ofstream ofs(mod);
        ofs << "var M = 9;" << std::endl;
    }
    auto* eng = engine::create();
    gate_rec rec;
    rec.reject_containing = "alx_gate_off";
    eng->set_hook(gate_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "import \"tmp-alxscpt-gtest/alx_gate_off.axc\" as m;")),
                         "");
    EXPECT_EQ(res.error, error_type::InterruptedError);

    eng->set_hook(nullptr, nullptr, 0);
    auto res2 = eng->exec(alx::bytes_view(alx::bytes(
                              "import \"tmp-alxscpt-gtest/alx_gate_off.axc\" as m;\nm.M;")),
                          "");
    EXPECT_EQ(res2.error, error_type::NoError);
    EXPECT_EQ(res2.value.to<int_64>(), 9);
    delete eng;
    std::remove(mod.c_str());
}

struct trap_rec {
    uint_64 traps = 0;
    uint_64 execs = 0;
    bool stepping = false;
    uint_64 orig_freq = 0;
    std::vector<std::string> notes;
    int_64 last_arg = -1;
};

static bool trap_hook(hook_info& info) {
    auto* r = static_cast<trap_rec*>(info.hkdt);
    if (info.type == hook_event::trap) {
        ++r->traps;
        auto& m = info.info.to<varmap>();
        r->notes.push_back(m.contain("here") ? "here" : "no-here");
        if (m.contain("args")) r->last_arg = m.value("args").to<int_64>(0);
    }
    return true;
}

TEST(gt_ascript_engine, TrapBasic) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(trap_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "trap();\ntrap(42);\n\"done\";")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<std::string>(), "done");
    EXPECT_EQ(rec.traps, 2u);
    EXPECT_EQ(rec.notes.size(), 2u);
    EXPECT_EQ(rec.notes[0], "here") << "trap() -> {here: posmap} only";
    EXPECT_EQ(rec.last_arg, 42) << "trap(x) -> args key carries the evaluated value";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, TrapNoHook) {
    auto* eng = engine::create();
    auto res = eng->exec(alx::bytes_view(alx::bytes("trap(); trap(1+2); 7;")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 7) << "trap without set_hook is a no-op";
    delete eng;
}

static bool trap_break_hook(hook_info& info) {
    if (info.type != hook_event::trap) return true;
    *info.desc = "debugger stop";
    return false;
}

TEST(gt_ascript_engine, TrapInterruptUncatchable) {
    auto* eng = engine::create();
    eng->set_hook(trap_break_hook, nullptr, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes("try { trap(); } catch (e) { }")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError)
        << "trap false -> interrupt, uncatchable (same as exec checkpoint)";
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — debugger stop");
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, TrapArgCountError) {
    auto* eng = engine::create();
    auto res = eng->exec(alx::bytes_view(alx::bytes("trap(1, 2, 3);")), "");
    EXPECT_EQ(res.error, error_type::ParseError) << "more than 2 arguments = compile error";
    delete eng;
}

TEST(gt_ascript_engine, TrapConditional) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(trap_hook, &rec, 0);

    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "trap(1, 42); trap(1==1, 7);")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(rec.traps, 2u);
    EXPECT_EQ(rec.last_arg, 7) << "last trap args overwrites";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, TrapConditionalSkip) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(trap_hook, &rec, 0);

    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "trap(0, 1/0); trap(0==1, 2/0);")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(rec.traps, 0u) << "falsy condition -> trap skipped, action not evaluated";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, TrapConditionalNoHook) {
    auto* eng = engine::create();
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "trap(1, 1/0); trap(1, 1/0); 7;")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 7) << "no hook -> no expr evaluated";
    delete eng;
}

static bool step_hook(hook_info& info) {
    auto* r = static_cast<trap_rec*>(info.hkdt);
    if (info.type == hook_event::trap) {
        ++r->traps;
        r->orig_freq = info.freq;
        info.freq = 1;
        r->stepping = true;
    } else if (info.type == hook_event::exec && r->stepping) {
        ++r->execs;
        info.freq = r->orig_freq;
        r->stepping = false;
    }
    return true;
}

TEST(gt_ascript_engine, TrapFreqStep) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(step_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes("trap(); var x = 1; x;")), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 1);
    EXPECT_EQ(rec.traps, 1u);
    EXPECT_EQ(rec.execs, 1u) << "stepping: exactly one exec checkpoint after trap";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

static bool throw_hook(hook_info&) { throw std::runtime_error("hook boom"); }

TEST(gt_ascript_engine, TrapHookThrow) {
    auto* eng = engine::create();
    eng->set_hook(throw_hook, nullptr, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes("trap();")), "");
    EXPECT_EQ(res.error, error_type::InterruptedError) << "hook callback throw = interrupt";
    EXPECT_EQ(res.value.to<std::string>(),
              "execution interrupted — hook callback threw exception");
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

static bool wkdt_hook(hook_info& info) {
    auto* r = static_cast<trap_rec*>(info.hkdt);
    if (info.type != hook_event::trap) return true;
    r->notes.push_back(info.wkfm_func(0));
    for (auto& k : info.wkfm_data_keys(0)) r->notes.push_back("k:" + k);
    auto& m = info.info.to<varmap>();
    if (m.contain("args")) r->last_arg = m.value("args").to<int_64>(0);
    return true;
}

TEST(gt_ascript_engine, WkdtFrameQueries) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(wkdt_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "var g = 10;\ndef f(a) { var b = a + 1; trap(a); return b; }\nf(5);")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 6);
    EXPECT_EQ(rec.notes[0], "f") << "frame 0 = current function frame";
    EXPECT_EQ(rec.last_arg, 5) << "trap(a) carries the arg value";
    bool has_a = false, has_b = false;
    for (size_t i = 1; i < rec.notes.size(); i++) {
        if (rec.notes[i] == "k:a") has_a = true;
        if (rec.notes[i] == "k:b") has_b = true;
    }
    EXPECT_TRUE(has_a && has_b) << "frame locals: param a + local b";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, WkdtEvalBlockFrames) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(wkdt_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "eval(\"trap();\");\nif (1) { trap(); }")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(rec.notes[0], "eval") << "eval anonymous frame named \"eval\"";
    EXPECT_EQ(rec.notes[1], "?") << "block frame is not a function frame";
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

static bool wkent_hook(hook_info& info) {
    auto* r = static_cast<trap_rec*>(info.hkdt);
    if (info.type != hook_event::trap) return true;
    const void* root = info.wken_root();
    for (auto& k : info.wken_data_keys(root)) r->notes.push_back("e:" + k);
    auto* gv = info.wken_data_cptr(root, "g");
    r->notes.push_back(gv && gv->is<int_64>() ? "g:int" : "g:miss");
    auto* fv = info.wken_data_cptr(root, "f");
    r->notes.push_back(fv && info.wkis_func(fv) ? "f:func" : "f:miss");
    auto* av = info.wken_data_cptr(root, "a");
    r->notes.push_back(av ? "a:in-store" : "a:frame-only");
    r->notes.push_back(info.wkis_ent(fv) ? "f:ent" : "f:not-ent");
    return true;
}

TEST(gt_ascript_engine, WkdtEntQueries) {
    auto* eng = engine::create();
    trap_rec rec;
    eng->set_hook(wkent_hook, &rec, 0);
    auto res = eng->exec(alx::bytes_view(alx::bytes(
                             "var g = 10;\ndef f(a) { trap(a); }\nf(5);")),
                         "");
    EXPECT_EQ(res.error, error_type::NoError);
    bool has_g = false, has_f = false;
    for (auto& n : rec.notes) {
        if (n == "e:g") has_g = true;
        if (n == "e:f") has_f = true;
    }
    EXPECT_TRUE(has_g && has_f) << "ent-level globals visible from root";
    EXPECT_EQ(rec.notes[rec.notes.size() - 4], "g:int");
    EXPECT_EQ(rec.notes[rec.notes.size() - 3], "f:func");
    EXPECT_EQ(rec.notes[rec.notes.size() - 2], "a:frame-only")
        << "frame locals excluded from ent store";
    EXPECT_EQ(rec.notes[rec.notes.size() - 1], "f:not-ent");
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_engine, EvalBasic) {
    auto* eng = engine::create();
    auto r1 = eng->exec(alx::bytes_view(alx::bytes("return eval(\"1+1;\");")), "");
    EXPECT_EQ(r1.error, error_type::NoError);
    EXPECT_EQ(r1.value.to<int_64>(), 2);
    auto r2 = eng->exec(alx::bytes_view(alx::bytes("return eval(\"return 42;\");")), "");
    EXPECT_EQ(r2.value.to<int_64>(), 42);
    auto r3 = eng->exec(alx::bytes_view(alx::bytes("return eval(\"var t = 10; t * 2;\");")), "");
    EXPECT_EQ(r3.value.to<int_64>(), 20);
    delete eng;
}

TEST(gt_ascript_engine, EvalParams) {
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "return eval(\"a + b;\", map{\"a\": 1, \"b\": 2});")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 3);

    auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                            "var a = 100; return eval(\"a;\", map{\"a\": 1});")),
                        "");
    EXPECT_EQ(r2.value.to<int_64>(), 1);
    delete eng;
}

TEST(gt_ascript_engine, EvalClosure) {
    auto* eng = engine::create();

    auto r1 = eng->exec(alx::bytes_view(alx::bytes("var x = 10; return eval(\"x + 1;\");")), "");
    EXPECT_EQ(r1.value.to<int_64>(), 11);

    auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                            "var z = 10; var y = eval(\"z = 99; z;\"); return z * 100 + y;")),
                        "");
    EXPECT_EQ(r2.value.to<int_64>(), 9999);
    delete eng;
}

TEST(gt_ascript_engine, EvalMultiLayer) {
    auto* eng = engine::create();

    auto r1 = eng->exec(alx::bytes_view(alx::bytes(
                            "var s = \"1+1;\"; return eval(\"return eval(s) * 2;\");")),
                        "");
    EXPECT_EQ(r1.value.to<int_64>(), 4);

    auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                            "var l1 = \"var l2 = \\\"return eval(\\\\\\\"2+2;\\\\\\\") + 1;\\\"; return eval(l2);\";"
                            "return eval(l1);")),
                        "");
    EXPECT_EQ(r2.value.to<int_64>(), 5);

    auto r3 = eng->exec(alx::bytes_view(alx::bytes(
                            "def d(v) { return v * 2; } var fn = \"d\"; return eval(\"return @(fn)(21);\");")),
                        "");
    EXPECT_EQ(r3.value.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, EvalDefScoping) {
    auto* eng = engine::create();

    auto r1 = eng->exec(alx::bytes_view(alx::bytes(
                            "return eval(\"def g(a) { return a * 2; } return g(21);\");")),
                        "");
    EXPECT_EQ(r1.value.to<int_64>(), 42);
    auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                            "eval(\"def g(a) { return 1; }\"); try { @(\"g\")(1); }"
                            "catch (e) { return 7; } return 0;")),
                        "");
    EXPECT_EQ(r2.value.to<int_64>(), 7);
    delete eng;
}

TEST(gt_ascript_engine, EvalErrors) {
    auto* eng = engine::create();

    auto r1 = eng->exec(alx::bytes_view(alx::bytes(
                            "try { eval(123); } catch (e) { return 1; } return 0;")),
                        "");
    EXPECT_EQ(r1.value.to<int_64>(), 1);

    auto r2 = eng->exec(alx::bytes_view(alx::bytes(
                            "try { eval(\"if(\"); } catch (e) { return 2; } return 0;")),
                        "");
    EXPECT_EQ(r2.value.to<int_64>(), 2);

    auto r3 = eng->exec(alx::bytes_view(alx::bytes(
                            "try { eval(\"a;\", [1]); } catch (e) { return 3; } return 0;")),
                        "");
    EXPECT_EQ(r3.value.to<int_64>(), 3);

    auto r4 = eng->exec(alx::bytes_view(alx::bytes("return eval(\"\") == null;")), "");
    EXPECT_EQ(r4.error, error_type::NoError);
    EXPECT_EQ(r4.value.to<bool>(), true);
    delete eng;
}

TEST(gt_ascript_engine, Eval_SyntaxErrorIsFirstWithPosition) {

    auto* eng = engine::create();
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    auto res = eng->exec(alx::bytes_view(alx::bytes("eval(\"1 +;\");")), "");
    EXPECT_EQ(res.error, error_type::ParseError);
    EXPECT_NE(err.find("Unexpected token"), std::string::npos);
    EXPECT_NE(err.find("at 1:4"), std::string::npos);
    delete eng;
}

TEST(gt_ascript_engine, Eval_DeepNestReportsGuard) {

    auto* eng = engine::create();
    std::string err;
    eng->on_cerr.connect([&](const std::string& s) { err = s; });
    std::string src = "eval(\"" + std::string(256, '(') + "1" + std::string(256, ')') + ";\");";
    auto res = eng->exec(alx::bytes_view(alx::bytes(src.data(), src.size())), "");
    EXPECT_EQ(res.error, error_type::ParseError);
    EXPECT_NE(err.find("nesting too deep"), std::string::npos);
    delete eng;
}

TEST(gt_ascript_engine, EvalTcoInside) {
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "return eval(\"def g(n) { if (n <= 0) return 0; return g(n - 1); }"
                           " return g(100);\");")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 0);
    delete eng;
}

TEST(gt_ascript_engine, EvalHereInside) {
    auto* eng = engine::create();

    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "var h = eval(\"here();\"); return h.row >= 1 && h.col >= 1 && h.file == \"\";")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<bool>(), true);
    delete eng;
}

TEST(gt_ascript_engine, Load_PresetValue) {
    auto* eng = engine::create();
    auto* p = eng->load("x", true);
    ASSERT_NE(p, nullptr);
    *p = (int_64) 42;
    auto r = eng->exec(alx::bytes_view(alx::bytes("return x * 2;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 84);
    delete eng;
}

TEST(gt_ascript_engine, Load_ReadAfterExec) {
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("var result = 1 + 2;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    auto* p = eng->load("result");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->to<int_64>(), 3);
    delete eng;
}

TEST(gt_ascript_engine, Load_CreateIfNotExist) {
    auto* eng = engine::create();
    auto* p1 = eng->load("new_var", true);
    ASSERT_NE(p1, nullptr);
    auto* p2 = eng->load("nonexistent", false);
    EXPECT_EQ(p2, nullptr);
    delete eng;
}

TEST(gt_ascript_engine, Load_ModifyAndExec) {
    auto* eng = engine::create();
    *eng->load("counter", true) = (int_64) 10;
    eng->exec(alx::bytes_view(alx::bytes("var r1 = counter + 5;")), "");
    EXPECT_EQ(eng->load("r1")->to<int_64>(), 15);
    *eng->load("counter") = (int_64) 100;
    eng->exec(alx::bytes_view(alx::bytes("var r2 = counter + 1;")), "");
    EXPECT_EQ(eng->load("r2")->to<int_64>(), 101);
    delete eng;
}

TEST(gt_ascript_engine, Load_NoVarDecl) {
    auto* eng = engine::create();
    *eng->load("n", true) = (int_64) 100;
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "var sum = 0; var i = 0; while (i < n) { sum = sum + i; i = i + 1; } sum;")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 4950);
    delete eng;
}

TEST(gt_ascript_engine, Running_IdleIsFalse) {
    auto* eng = engine::create();
    EXPECT_FALSE(eng->running());
    delete eng;
}

TEST(gt_ascript_engine, Running_DuringExec) {
    auto* eng = engine::create();
    std::atomic<bool> saw_running{false};
    eng->set_hook(
        [](hook_info& hi) -> bool {
            auto* flag = static_cast<std::atomic<bool>*>(hi.hkdt);
            flag->store(true, std::memory_order_release);
            return true;
        },
        &saw_running, 1);
    eng->exec(alx::bytes_view(alx::bytes("var i = 0; while (i < 100000) { i = i + 1; } i;")), "");
    eng->set_hook(nullptr, nullptr, 0);
    EXPECT_TRUE(saw_running.load());
    delete eng;
}

TEST(gt_ascript_engine, SetInterrupt_BreaksExec) {
    auto* eng = engine::create();
    eng->set_hook(
        [](hook_info& hi) -> bool {
            auto* eng_ptr = static_cast<engine*>(hi.hkdt);
            static int count = 0;
            if (++count > 5) {
                eng_ptr->set_interrupt();
                return false;
            }
            return true;
        },
        eng, 1);
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "var s = 0; var i = 0; while (i < 10000000) { s = s + i; i = i + 1; } s;")),
                       "");
    eng->set_hook(nullptr, nullptr, 0);
    EXPECT_EQ(r.error, error_type::InterruptedError);
    EXPECT_FALSE(eng->running());
    delete eng;
}

TEST(gt_ascript_engine, SetInterrupt_CrossThread) {
    auto* eng = engine::create();
    std::atomic<bool> started{false};
    eng->set_hook(
        [](hook_info& hi) -> bool {
            auto* flag = static_cast<std::atomic<bool>*>(hi.hkdt);
            flag->store(true, std::memory_order_release);
            return true;
        },
        &started, 1);
    auto fut = std::async(std::launch::async, [&] {
        return eng->exec(alx::bytes_view(alx::bytes(
                             "var s = 0; var i = 0; while (i < 100000000) { s = s + i; i = i + 1; } s;")),
                         "");
    });
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
    eng->set_interrupt();
    auto r = fut.get();
    eng->set_hook(nullptr, nullptr, 0);
    EXPECT_EQ(r.error, error_type::InterruptedError);
    EXPECT_FALSE(eng->running());
    delete eng;
}

TEST(gt_ascript_engine, Running_AfterExecCompletes) {
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("42;")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_FALSE(eng->running());
    delete eng;
}

TEST(gt_ascript_engine, ImportAutoSuffix_Axp) {

    std::string mod = "tmp-alxscpt-gtest/alx_auto_sfx.axp";
    {
        std::ofstream ofs(mod);
        ofs << "def v() { return 10; }" << std::endl;
    }
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_auto_sfx\" as m; m.v();")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 10);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, ImportAutoSuffix_Axc) {

    std::string mod = "tmp-alxscpt-gtest/alx_auto_sfx2.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def v() { return 20; }" << std::endl;
    }
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_auto_sfx2\" as m; m.v();")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 20);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, ImportAutoSuffix_ExactExtension) {

    std::string mod = "tmp-alxscpt-gtest/alx_auto_sfx3.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def v() { return 30; }" << std::endl;
    }
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_auto_sfx3.axc\" as m; m.v();")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 30);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_engine, ImportAutoSuffix_AxpPriority) {

    std::string axp = "tmp-alxscpt-gtest/alx_auto_pri.axp";
    std::string axc = "tmp-alxscpt-gtest/alx_auto_pri.axc";
    {
        std::ofstream ofs(axp);
        ofs << "def v() { return 1; }" << std::endl;
    }
    {
        std::ofstream ofs(axc);
        ofs << "def v() { return 2; }" << std::endl;
    }
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_auto_pri\" as m; m.v();")),
                       "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1) << ".axp should take priority over .axc";
    delete eng;
    std::remove(axp.c_str());
    std::remove(axc.c_str());
}

TEST(gt_ascript_engine, ImportAutoSuffix_NotFound) {

    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "import \"tmp-alxscpt-gtest/alx_auto_nope\" as m; 1;")),
                       "");
    EXPECT_EQ(r.error, error_type::ImportError);
    delete eng;
}

TEST(gt_ascript_engine, LinkAutoSuffix_NotFound) {

    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "link \"tmp-alxscpt-gtest/alx_auto_link_nope\" as f; 1;")),
                       "");
    EXPECT_EQ(r.error, error_type::ImportError);
    delete eng;
}

TEST(gt_ascript_engine, LinkAutoSuffix_ExactExtension) {

    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes(
                           "link \"tmp-alxscpt-gtest/alx_auto_link_exact.so\" as f; 1;")),
                       "");
    EXPECT_EQ(r.error, error_type::ImportError);
    delete eng;
}

TEST(gt_ascript_engine, AxpImport_GeometryModules) {

    const std::string dir = "/tmp/axp-geom-test/";
    const std::string geom = dir + "geom/";
    fs::create_directories(geom);

    auto write = [](const std::string& p, const char* c) {
        std::ofstream o(p);
        o << c;
    };

    write(geom + "point.axc", R"(
var px = 0;
var py = 0;
def init(x, y) { px = x; py = y; }
def dist(ox, oy) { return (px - ox) * (px - ox) + (py - oy) * (py - oy); }
)");

    write(geom + "circle.axc", R"x(
import "point.axc" as pt;
var radius = 0;
def init(cx, cy, r) { pt.init(cx, cy); radius = r; }
def area() { return 3 * radius * radius; }
def to_string() { return "Circle(r=" + string(radius) + ")"; }
)x");

    write(geom + "rect.axc", R"(
var x1 = 0; var y1 = 0;
var x2 = 0; var y2 = 0;
def init(ax, ay, bx, by) { x1 = ax; y1 = ay; x2 = bx; y2 = by; }
def perimeter() { return 2 * (x2 - x1) + 2 * (y2 - y1); }
def area() { return (x2 - x1) * (y2 - y1); }
def to_string() { return "Rect[" + string(x1) + "," + string(y1) + "-" + string(x2) + "," + string(y2) + "]"; }
)");

    write(geom + "tri.axc", R"x(
var ax = 0; var ay = 0;
var bx = 0; var by = 0;
var cx = 0; var cy = 0;
def init(x1, y1, x2, y2, x3, y3) {
    ax = x1; ay = y1; bx = x2; by = y2; cx = x3; cy = y3;
}
def perimeter() {
    var a = ax - bx; var b = ay - by;
    var c = bx - cx; var d = by - cy;
    var e = cx - ax; var f = cy - ay;
    return a * a + b * b + c * c + d * d + e * e + f * f;
}
def area() {
    return (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by)) / 2;
}
def to_string() {
    return   "Tri[(" + string(ax) + "," + string(ay) + ")-(" +
             string(bx) + "," + string(by) + ")-(" +
             string(cx) + "," + string(cy) + ")]";
}
)x");

    write(dir + "main.axc", R"(
import "geom/circle.axc" as c;
import "geom/rect.axc" as r;
import "geom/tri.axc" as t;
c.init(0, 0, 5);
r.init(0, 0, 4, 3);
t.init(0, 0, 4, 0, 0, 3);
c.to_string() + " area=" + string(c.area()) + "\n" +
r.to_string() + " peri=" + string(r.perimeter()) + " area=" + string(r.area()) + "\n" +
t.to_string() + " area=" + string(t.area());
)");

    auto* c_eng = engine::create();
    bytes bin = c_eng->compile(dir + "main.axc", false, true);
    delete c_eng;
    ASSERT_FALSE(bin.empty());
    ASSERT_TRUE(varsolid::is_valid(bytes_view(bin)));

    const std::string axp_path = dir + "geom.axp";
    {
        std::ofstream o(axp_path, std::ios::binary);
        o.write(reinterpret_cast<const char*>(bin.data()), bin.size());
    }
    fs::remove_all(dir + "geom/");
    fs::remove(dir + "main.axc");

    {
        auto* eng = engine::create();
        auto r = eng->exec(axp_path);
        EXPECT_EQ(r.error, error_type::NoError) << r.value.to<std::string>();
        delete eng;
    }

    {
        auto* eng = engine::create();
        eng->set_search_paths({dir});
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"geom.axp\" as g; 1;")),
                           dir);
        EXPECT_EQ(r.error, error_type::NoError) << r.value.to<std::string>();
        delete eng;
    }

    {
        auto* eng = engine::create();
        eng->set_search_paths({dir});
        auto r = eng->exec(alx::bytes_view(alx::bytes(
                               "import \"geom\" as g; 1;")),
                           dir);
        EXPECT_EQ(r.error, error_type::NoError) << r.value.to<std::string>();
        delete eng;
    }

    fs::remove_all(dir);
}

TEST(gt_ascript_engine, Call_Basic) {
    auto* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes("def double_it(x) { return x * 2; }")), "");
    auto r = eng->call("double_it", {variant(int_64(21))});
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 42);
    delete eng;
}

TEST(gt_ascript_engine, Call_Counter) {

    auto* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes(
                  "var counter = 0;"
                  "def inc() { counter = counter + 1; return counter; }")),
              "");
    auto r1 = eng->call("inc", {});
    EXPECT_EQ(r1.error, error_type::NoError);
    EXPECT_EQ(r1.value.to<int_64>(), 1);
    auto r2 = eng->call("inc", {});
    EXPECT_EQ(r2.value.to<int_64>(), 2);
    auto r3 = eng->call("inc", {});
    EXPECT_EQ(r3.value.to<int_64>(), 3);
    delete eng;
}

TEST(gt_ascript_engine, Call_Interruptible) {
    // call is an engine entry like exec: a bounded, interruptible run
    auto* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes("def spin() { var i = 0; while (1) { i = i + 1; } }")), "");

    hook_rec rec;
    rec.budget = 500;
    eng->set_hook(test_hook, &rec, 100);
    auto res = eng->call("spin", {});
    EXPECT_EQ(res.error, error_type::InterruptedError) << res.value.to<std::string>();
    EXPECT_EQ(res.value.to<std::string>(), "execution interrupted — budget exhausted");
    EXPECT_TRUE(rec.interrupted);

    // the flag is cleared on the way out: the next entry is not poisoned by a stale interrupt
    eng->set_hook(nullptr, nullptr, 0);
    auto r2 = eng->exec(alx::bytes_view(alx::bytes("1 + 1;")), "");
    EXPECT_EQ(r2.error, error_type::NoError);
    EXPECT_EQ(r2.value.to<int_64>(), 2);
    delete eng;
}

TEST(gt_ascript_engine, Call_FibTailRecursion) {

    auto* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes(
                  "def fib(n, a, b) {"
                  "  if (n == 0) { return a; }"
                  "  return fib(n - 1, b, a + b);"
                  "}"
                  "def fib_wrap(n) { return fib(n, 0, 1); }")),
              "");

    auto r = eng->call("fib_wrap", {variant(int_64(10))});
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 55);

    auto r0 = eng->call("fib_wrap", {variant(int_64(0))});
    EXPECT_EQ(r0.value.to<int_64>(), 0);

    auto r1 = eng->call("fib_wrap", {variant(int_64(1))});
    EXPECT_EQ(r1.value.to<int_64>(), 1);
    delete eng;
}

TEST(gt_ascript_engine, Call_NotFound) {
    auto* eng = engine::create();
    auto r = eng->call("no_such_func", {});
    EXPECT_EQ(r.error, error_type::NameError);
    delete eng;
}

TEST(gt_ascript_engine, Call_ScriptError) {

    auto* eng = engine::create();
    eng->exec(alx::bytes_view(alx::bytes("def bad() { return 1 / 0; }")), "");
    auto r = eng->call("bad", {});
    EXPECT_EQ(r.error, error_type::DivZeroError);
    delete eng;
}

TEST(gt_ascript_engine, Iload_expression) {
    auto* eng = engine::create();
    auto r = eng->exec(alx::bytes_view(alx::bytes("var v = @(\"1+2\");\nv;\n")), "");
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 3);
    delete eng;
}

TEST(gt_ascript_engine, Parse_NestOverflowReportsOnce) {

    auto* eng = engine::create();
    std::vector<compile_error> errors;
    eng->on_cmpl.connect([&](const compile_error& e) { errors.push_back(e); });
    std::string src = "return " + std::string(300, '(') + "1" + std::string(300, ')') + ";";
    bytes bin = eng->compile(alx::bytes_view(alx::bytes(src.data(), src.size())), "", false);
    EXPECT_TRUE(bin.empty());
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "nesting too deep");
    delete eng;
}

TEST(gt_ascript_engine, Iload_nesting_is_bounded) {

    auto* eng = engine::create();
    std::string src = "var v = @(\"" + std::string(10000, '(') + "\");\nv;\n";
    auto r = eng->exec(alx::bytes_view(alx::bytes(src.data(), src.size())), "");
    EXPECT_NE(r.error, error_type::NoError);
    delete eng;
}

TEST(gt_ascript_engine, HookInterruptEmptyBodyLoop) {

    auto* eng = engine::create();
    std::atomic<bool> done{false};
    error_type err = error_type::NoError;
    std::thread t([&] {
        err = eng->exec(alx::bytes_view(alx::bytes("for (;;) {}")), "").error;
        done = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    eng->set_interrupt();
    for (int i = 0; i < 30 && !done.load(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!done.load()) {
        t.detach();
        ADD_FAILURE() << "for (;;) {} ignored set_interrupt()";
        return;
    }
    t.join();
    EXPECT_EQ(err, error_type::InterruptedError);
    delete eng;
}
