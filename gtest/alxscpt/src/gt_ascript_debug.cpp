/** ****************************************************************
 * \file   gt_ascript_debug.cpp
 * \brief  Debug positions — O_DEBUG markers, uncaught-error row/col, debug hook events
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace alx;
using namespace alx::script;

namespace {

    std::string exec_err(engine* _eng, const std::string& _src) {
        std::string err;
        _eng->on_cerr.connect([&](const std::string& _s) { err = _s; });
        _eng->exec(bytes_view(bytes(_src)), "");
        return err;
    }

    struct dbg_rec {
        uint_64 exec_events = 0;
        uint_64 debug_events = 0;
        std::vector<std::pair<int_64, int_64>> pos;
        std::vector<std::string> files;
        std::vector<std::vector<std::string>> chain;
        std::vector<std::vector<std::pair<int_64, int_64>>> chain_pos;
    };

    bool dbg_hook(hook_info& _info) {
        auto* r = static_cast<dbg_rec*>(_info.hkdt);
        if (_info.type == hook_event::exec) {
            ++r->exec_events;
            return true;
        }
        if (_info.type != hook_event::debug) return true;
        ++r->debug_events;
        varmap p = _info.wkdt_pos();
        int_64 row = p.contain("row") && p.value("row").is<int_64>() ? p.value("row").to<int_64>() : -1;
        int_64 col = p.contain("col") && p.value("col").is<int_64>() ? p.value("col").to<int_64>() : -1;
        r->pos.emplace_back(row, col);
        r->files.push_back(p.contain("file") && p.value("file").is<std::string>()
                               ? p.value("file").to<std::string>()
                               : std::string());
        std::vector<std::string> funcs;
        std::vector<std::pair<int_64, int_64>> pos;
        for (auto& m : _info.wkdt_fpos()) {
            funcs.push_back(m.contain("func") && m.value("func").is<std::string>()
                                ? m.value("func").to<std::string>()
                                : std::string());
            int_64 fr = m.contain("row") && m.value("row").is<int_64>() ? m.value("row").to<int_64>() : -1;
            int_64 fc = m.contain("col") && m.value("col").is<int_64>() ? m.value("col").to<int_64>() : -1;
            pos.emplace_back(fr, fc);
        }
        r->chain.push_back(std::move(funcs));
        r->chain_pos.push_back(std::move(pos));
        return true;
    }

}

TEST(gt_ascript_debug, TopLevelThrowPos) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng, "var a = 1;\nvar b = 2;\nthrow(\"boom\");\n");
    EXPECT_NE(err.find("Uncaught"), std::string::npos);
    EXPECT_NE(err.find("[::]:3:1"), std::string::npos)
        << "top-level throw must report row:col (was: no trace line at all): " << err;
    delete eng;
}

TEST(gt_ascript_debug, FunctionChainPos) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng,
                               "def f() {\n"
                               "    throw(\"boom\");\n"
                               "}\n"
                               "def g() {\n"
                               "    f();\n"
                               "}\n"
                               "def h() {\n"
                               "    g();\n"
                               "}\n"
                               "h();\n");

    EXPECT_NE(err.find("[::] f:2:5"), std::string::npos) << err;
    EXPECT_NE(err.find("[::] g:5:5"), std::string::npos) << err;
    EXPECT_NE(err.find("[::] h:8:5"), std::string::npos) << err;
    EXPECT_NE(err.find("[::]:10:1"), std::string::npos) << err;
    delete eng;
}

TEST(gt_ascript_debug, RecursiveFoldKeepsPos) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng,
                               "def f() {\n"
                               "    f();\n"
                               "}\n"
                               "f();\n");
    EXPECT_NE(err.find("StackError"), std::string::npos) << err;
    EXPECT_NE(err.find("x)"), std::string::npos) << "recursion still folds: " << err;
    EXPECT_NE(err.find("[::] f:2:5"), std::string::npos)
        << "the folded cycle carries the call-site position: " << err;
    delete eng;
}

TEST(gt_ascript_debug, EvalFramePos) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng,
                               "var a = 1;\n"
                               "eval(\"throw(\\\"boom\\\");\");\n");
    EXPECT_NE(err.find("[::]:2:1"), std::string::npos) << "eval call site: " << err;
    EXPECT_NE(err.find("[::] eval:1:1"), std::string::npos)
        << "anonymous eval frame used to be dropped from the trace entirely: " << err;
    delete eng;
}

TEST(gt_ascript_debug, ModuleInitPos) {
    std::string dir = "tmp-alxscpt-gtest";
    std::filesystem::create_directories(dir);
    std::string mod = dir + "/dbg_bad_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "var x = 1;\n"
            << "throw(\"bad init\");\n";
    }
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng, "import \"" + mod + "\" as m;\n1;\n");
    EXPECT_NE(err.find("in module"), std::string::npos) << err;
    EXPECT_NE(err.find("dbg_bad_mod.axc:"), std::string::npos)
        << "module init trace must name the module (not [::]): " << err;
    EXPECT_NE(err.find(":2:1"), std::string::npos)
        << "module-internal row/col (was: module name only): " << err;
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_debug, MarkerlessAxpNoStalePos) {

    bytes axp;
    {
        auto* ce = engine::create();
        axp = ce->compile(bytes_view(bytes("var a = 1;\nthrow(\"compiled boom\");\n")), "");
        delete ce;
    }
    ASSERT_FALSE(axp.empty());

    auto* eng = engine::create();
    eng->set_debug_enable(true);

    auto r1 = eng->exec(bytes_view(bytes("var x = 1;\nvar y = 2;\nx = x + y;\n")), "");
    ASSERT_EQ(r1.error, error_type::NoError);

    std::string err;
    eng->on_cerr.connect([&](const std::string& _s) { err = _s; });
    auto r2 = eng->exec(bytes_view(axp), "");
    EXPECT_NE(r2.error, error_type::NoError);
    EXPECT_NE(err.find("compiled boom"), std::string::npos) << err;
    EXPECT_NE(err.find("[::]"), std::string::npos) << "root line is always there: " << err;
    EXPECT_EQ(err.find(":3:1"), std::string::npos)
        << "stale position from the previous exec leaked into a marker-less run: " << err;
    delete eng;
}

TEST(gt_ascript_debug, BareBodyFallsBackToOwningStmt) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);

    std::string err = exec_err(eng,
                               "def f() {\n"
                               "    throw(\"boom\");\n"
                               "}\n"
                               "if (1)\n"
                               "    f();\n");
    EXPECT_NE(err.find("[::] f:2:5"), std::string::npos) << err;
    EXPECT_NE(err.find("[::]:4:1"), std::string::npos) << err;
    EXPECT_EQ(err.find(":5:5"), std::string::npos)
        << "the bare body has no marker of its own: " << err;
    delete eng;
}

TEST(gt_ascript_debug, BareConditionStillAccurate) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);

    std::string err = exec_err(eng,
                               "var k = 0;\n"
                               "if (1 / k) 1;\n");
    EXPECT_NE(err.find("Uncaught"), std::string::npos);
    EXPECT_NE(err.find("[::]:2:1"), std::string::npos) << err;
    delete eng;
}

TEST(gt_ascript_debug, SwitchCaseBodyScopeUnchanged) {

    const char* src =
        "var k = 1;\n"
        "switch (k) {\n"
        "case 1:\n"
        "    var v = 7;\n"
        "}\n"
        "v;\n";
    for (int pass = 0; pass < 2; pass++) {
        auto* eng = engine::create();
        if (pass) eng->set_debug_enable(true);
        auto res = eng->exec(bytes_view(bytes(src)), "");
        EXPECT_EQ(res.error, error_type::NoError) << "pass " << pass;
        EXPECT_TRUE(res.value.is<int_64>()) << "pass " << pass;
        EXPECT_EQ(res.value.to<int_64>(), 7) << "pass " << pass;
        delete eng;
    }
}

TEST(gt_ascript_debug, SwitchMultiStmtCaseHasMarkers) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    dbg_rec rec;
    eng->set_hook(dbg_hook, &rec, 0);
    auto res = eng->exec(bytes_view(bytes(
                             "var r = 0;\n"
                             "var k = 2;\n"
                             "switch (k) {\n"
                             "case 1:\n"
                             "    r = 1;\n"
                             "case 2:\n"
                             "    r = 2;\n"
                             "    r = r + 1;\n"
                             "    break;\n"
                             "}\n"
                             "r;\n")),
                         "");
    eng->set_hook(nullptr, nullptr, 0);
    ASSERT_EQ(res.error, error_type::NoError);
    EXPECT_EQ(res.value.to<int_64>(), 3);
    bool has7 = false, has8 = false;
    for (auto& p : rec.pos) {
        if (p == std::make_pair(int_64(7), int_64(5))) has7 = true;
        if (p == std::make_pair(int_64(8), int_64(5))) has8 = true;
        EXPECT_NE(p, std::make_pair(int_64(5), int_64(5))) << "untaken case body must not fire";
    }
    EXPECT_TRUE(has7) << "case body statement 7:5";
    EXPECT_TRUE(has8) << "case body statement 8:5";
    delete eng;
}

TEST(gt_ascript_debug, ModuleFrameFile) {
    std::string dir = "tmp-alxscpt-gtest";
    std::filesystem::create_directories(dir);
    std::string mod = dir + "/dbg_call_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def m() {\n"
            << "    throw(\"boom\");\n"
            << "}\n";
    }
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    std::string err = exec_err(eng, "import \"" + mod + "\" as m;\nm.m();\n");
    EXPECT_NE(err.find("[dbg_call_mod.axc] m:2:5"), std::string::npos)
        << "a module function frame names its own file: " << err;
    EXPECT_NE(err.find("[::]:2:1"), std::string::npos)
        << "the root script has no file to claim: " << err;
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_debug, CompileWithMarkersRoundTrip) {
    std::string dir = "tmp-alxscpt-gtest";
    std::filesystem::create_directories(dir);
    std::string mod = dir + "/dbg_dep_mod.axc";
    {
        std::ofstream ofs(mod);
        ofs << "def m() {\n    return 5;\n}\n";
    }
    bytes axp;
    {
        auto* ce = engine::create();
        ce->set_debug_enable(true);

        axp = ce->compile(bytes_view(bytes("import \"" + mod + "\" as m;\nm.m();\n")), "");
        delete ce;
    }
    ASSERT_FALSE(axp.empty()) << "compile with markers produced nothing";

    auto* eng = engine::create();
    eng->set_debug_enable(true);
    auto res = eng->exec(bytes_view(axp), "");
    EXPECT_EQ(res.error, error_type::NoError);
    EXPECT_TRUE(res.value.is<int_64>());
    EXPECT_EQ(res.value.to<int_64>(), 5);
    delete eng;
    std::remove(mod.c_str());
}

TEST(gt_ascript_debug, DumpShowsMarkers) {
    const char* src = "var a = 1;\nif (a) { a = 2; }\n";

    std::string plain = engine::prtast(bytes_view(bytes(src)));
    EXPECT_EQ(plain.find("DEBUG"), std::string::npos) << plain;

    bytes axp;
    {
        auto* ce = engine::create();
        ce->set_debug_enable(true);
        axp = ce->compile(bytes_view(bytes(src)), "");
        delete ce;
    }
    ASSERT_FALSE(axp.empty());
    std::string dump = engine::prtast(bytes_view(axp));
    EXPECT_NE(dump.find("(DEBUG 1 1 0)"), std::string::npos) << dump;
    EXPECT_NE(dump.find("(DEBUG 2 10 20)"), std::string::npos)
        << "the marker inside the if-body block: " << dump;
}

TEST(gt_ascript_debug, OffNoPos) {
    auto* eng = engine::create();
    std::string err = exec_err(eng,
                               "def f() {\n"
                               "    throw(\"boom\");\n"
                               "}\n"
                               "f();\n");
    EXPECT_NE(err.find("Uncaught"), std::string::npos);
    EXPECT_EQ(err.find("[::]:"), std::string::npos) << "no row:col when off: " << err;
    delete eng;
}

TEST(gt_ascript_debug, OffNoDebugEvents) {
    auto* eng = engine::create();
    dbg_rec rec;
    eng->set_hook(dbg_hook, &rec, 1);
    eng->exec(bytes_view(bytes("var i = 0;\nwhile (i < 20) {\n    i = i + 1;\n}\ni;\n")), "");
    eng->set_hook(nullptr, nullptr, 0);
    EXPECT_GT(rec.exec_events, 0u);
    EXPECT_EQ(rec.debug_events, 0u) << "no markers in the AST, no events to fire";
    delete eng;
}

TEST(gt_ascript_debug, InsnCountUnchanged) {
    const char* src = "var i = 0;\nwhile (i < 200) {\n    i = i + 1;\n}\ndef f(x) { return x * 2; }\nf(i);\n";
    dbg_rec off, on;
    variant off_val;
    for (int pass = 0; pass < 2; pass++) {
        auto* eng = engine::create();
        dbg_rec& rec = pass ? on : off;
        if (pass) eng->set_debug_enable(true);
        eng->set_hook(dbg_hook, &rec, 1);
        auto res = eng->exec(bytes_view(bytes(src)), "");
        eng->set_hook(nullptr, nullptr, 0);
        ASSERT_EQ(res.error, error_type::NoError);
        if (!pass) {
            off_val = res.value;
        } else {
            EXPECT_TRUE(res.value.is<int_64>() && off_val.is<int_64>() &&
                        res.value.to<int_64>() == off_val.to<int_64>())
                << "same result with and without markers";
        }
        delete eng;
    }
    EXPECT_GT(off.exec_events, 0u);
    EXPECT_EQ(on.exec_events, off.exec_events)
        << "markers must not consume step budget (hook interval gating)";
    EXPECT_GT(on.debug_events, 0u) << "debug events fire when on";
}

TEST(gt_ascript_debug, HookEventPerStatement) {
    auto* eng = engine::create();
    eng->set_debug_enable(true);
    dbg_rec rec;
    eng->set_hook(dbg_hook, &rec, 0);
    auto res = eng->exec(bytes_view(bytes(
                             "var a = 1;\n"
                             "a = a + 1;\n"
                             "def f() {\n"
                             "    return a;\n"
                             "}\n"
                             "f();\n")),
                         "");
    eng->set_hook(nullptr, nullptr, 0);
    ASSERT_EQ(res.error, error_type::NoError);

    ASSERT_EQ(rec.debug_events, 5u) << "one debug event per statement";
    EXPECT_EQ(rec.pos[0], std::make_pair(int_64(1), int_64(1)));
    EXPECT_EQ(rec.pos[1], std::make_pair(int_64(2), int_64(1)));
    EXPECT_EQ(rec.pos[2], std::make_pair(int_64(3), int_64(1))) << "the def statement itself";
    EXPECT_EQ(rec.pos[3], std::make_pair(int_64(6), int_64(1))) << "the call site";
    EXPECT_EQ(rec.pos[4], std::make_pair(int_64(4), int_64(5)))
        << "inside the def body the marker tracks the body statement";

    ASSERT_EQ(rec.chain[4].size(), 2u);
    EXPECT_EQ(rec.chain[4][0], "f");
    EXPECT_EQ(rec.chain[4][1], "");
    EXPECT_EQ(rec.chain_pos[4][0], std::make_pair(int_64(4), int_64(5)));
    EXPECT_EQ(rec.chain_pos[4][1], std::make_pair(int_64(6), int_64(1)));
    ASSERT_EQ(rec.chain[0].size(), 1u) << "top level has no frame";
    for (auto& f : rec.files) EXPECT_EQ(f, "") << "root script has no file to blame";
    delete eng;
}
