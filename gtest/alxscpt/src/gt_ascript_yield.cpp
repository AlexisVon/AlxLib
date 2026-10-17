/** ****************************************************************
 * \file   gt_yield.cpp
 * \brief  The yield service — script yield(), engine::yield, the heartbeat
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace alx;
using namespace alx::script;

namespace {

    engine* g_eng = nullptr;
    std::vector<std::future<result>> g_futs;
    std::vector<std::string> g_marks;
    bool g_args_survived = false;

    // $post(name, pri?, expiry?, arg?) -- queue one delivery
    void native_post(fwrap& _f) {
        std::string name = _f[0].to<std::string>();
        uint_32 pri = _f.size() > 1 ? static_cast<uint_32>(_f[1].to<int_64>()) : 0;
        uint_32 exp = _f.size() > 2 ? static_cast<uint_32>(_f[2].to<int_64>()) : 0;
        varvec args;
        if (_f.size() > 3) args.push_back(variant(_f[3]));
        g_futs.push_back(g_eng->yield(name, std::move(args), pri, exp));
        _f.freturn(variant());
    }

    // $refuse() -- an rvalue delivery into a full queue must leave the caller's vector intact
    void native_refuse(fwrap& _f) {
        varvec args;
        args.push_back(variant(static_cast<int_64>(9)));
        g_futs.push_back(g_eng->yield("f", std::move(args)));
        g_args_survived = args.size() == 1 && args[0].to<int_64>() == 9;
        _f.freturn(variant());
    }

    // $mark(name) -- record the order serviced calls ran in
    void native_mark(fwrap& _f) {
        g_marks.push_back(_f[0].to<std::string>());
        _f.freturn(variant());
    }

    // $nap(ms) -- hold the engine thread so the heartbeat can settle the queue
    void native_nap(fwrap& _f) {
        std::this_thread::sleep_for(std::chrono::milliseconds(_f[0].to<int_64>()));
        _f.freturn(variant());
    }

    std::atomic<fwrap::callback*> g_cb{nullptr};
    std::atomic<bool> g_armed{false};

    // $curry(name) -- mint a delivery handle for a worker thread
    void native_curry(fwrap& _f) {
        g_cb = _f.currying(_f[0].to<std::string>());
        _f.freturn(variant());
    }

    // $post_arm(name) -- post, then let the hook start rejecting checkpoints
    void native_post_arm(fwrap& _f) {
        native_post(_f);
        g_armed = true;
    }

    // $shrink(n) -- resize the yield queue from inside the run
    void native_shrink(fwrap& _f) {
        g_eng->set_max_yield(static_cast<size_t>(_f[0].to<int_64>()));
        _f.freturn(variant());
    }

    bool reject_hook(hook_info& _hi) {
        if (!g_armed.load()) return true;
        *_hi.desc = "reject";
        return false;
    }

    result run(engine* _e, const char* _src) {
        return _e->exec(alx::bytes_view(alx::bytes(_src)), "");
    }

    std::string write_mod(const char* _name, const char* _body) {
        std::string path = std::string("tmp-alxscpt-gtest/") + _name;
        std::ofstream ofs(path);
        ofs << _body << std::endl;
        return path;
    }

}

TEST(gt_ascript_yield, ServeAtTheYieldPoint) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto r = run(eng, "def f(a) { return a * 2; } $post(\"f\", 0, 0, 21); var c = yield(); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::NoError);
    EXPECT_EQ(fr.value.to<int_64>(), 42);
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, PriorityThenArrival) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    auto r = run(eng,
                 "def p1() { $mark(\"p1\"); } def p2() { $mark(\"p2\"); }"
                 "def p3() { $mark(\"p3\"); } def p4() { $mark(\"p4\"); }"
                 "$post(\"p1\", 1); $post(\"p2\", 5); $post(\"p3\", 5); $post(\"p4\", 2);"
                 "var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 4);
    ASSERT_EQ(g_marks.size(), 4u);
    EXPECT_EQ(g_marks[0], "p2");
    EXPECT_EQ(g_marks[1], "p3");
    EXPECT_EQ(g_marks[2], "p4");
    EXPECT_EQ(g_marks[3], "p1");
    for (auto& f : g_futs) EXPECT_EQ(f.get().error, error_type::NoError);
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, CountBudgetLeavesTheRestQueued) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    auto r = run(eng,
                 "def p1() { $mark(\"p1\"); } def p2() { $mark(\"p2\"); }"
                 "def p3() { $mark(\"p3\"); } def p4() { $mark(\"p4\"); }"
                 "$post(\"p1\", 1); $post(\"p2\", 5); $post(\"p3\", 5); $post(\"p4\", 2);"
                 "var c = yield(0, 2); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 2);
    ASSERT_EQ(g_marks.size(), 2u);
    EXPECT_EQ(g_marks[0], "p2");
    EXPECT_EQ(g_marks[1], "p3");

    ASSERT_EQ(g_futs.size(), 4u);
    EXPECT_EQ(g_futs[1].get().error, error_type::NoError);
    EXPECT_EQ(g_futs[2].get().error, error_type::NoError);
    for (int i : {0, 3}) {
        auto fr = g_futs[i].get();
        EXPECT_EQ(fr.error, error_type::YieldError);
        EXPECT_EQ(fr.value.to<std::string>(), "not executed");
    }
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, ResidencyWindowWaitsAndTheDrainDoesNot) {
    engine* eng = engine::create();
    auto r = run(eng, "var c1 = yield(60, 0); return c1;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 0);
    EXPECT_GE(r.elapsed_us, 50000);

    auto q = run(eng, "var c2 = yield(300, 0); return c2;");
    ASSERT_EQ(q.error, error_type::NoError);
    EXPECT_EQ(q.value.to<int_64>(), 0);
    auto f = run(eng, "var c3 = yield(0, 0); return c3;");
    ASSERT_EQ(f.error, error_type::NoError);
    EXPECT_LT(f.elapsed_us, 100000);
    delete eng;
}

TEST(gt_ascript_yield, NoRunRefusesWithNoMainFiber) {
    engine* eng = engine::create();
    varvec keep;
    keep.push_back(variant(static_cast<int_64>(9)));
    auto fr = eng->yield("f", std::move(keep)).get();
    EXPECT_EQ(fr.error, error_type::YieldError);
    EXPECT_EQ(fr.value.to<std::string>(), "no main fiber");
    EXPECT_EQ(keep.size(), 1u);
    EXPECT_EQ(keep[0].to<int_64>(), 9);
    auto fr2 = eng->yield("f", varvec()).get();
    EXPECT_EQ(fr2.value.to<std::string>(), "no main fiber");
    delete eng;
}

TEST(gt_ascript_yield, FullQueueRefusesAndLeavesTheArgs) {
    engine_config cfg;
    cfg.max_yield = 1;
    engine* eng = engine::create(cfg);
    EXPECT_EQ(eng->config().max_yield, 1u);
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    g_args_survived = false;
    eng->set_extend("post", native_post);
    eng->set_extend("refuse", native_refuse);
    eng->set_extend("mark", native_mark);
    auto r = run(eng,
                 "def f() { $mark(\"f\"); }"
                 "$post(\"f\"); $post(\"f\"); $refuse(); var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    EXPECT_TRUE(g_args_survived);
    ASSERT_EQ(g_futs.size(), 3u);
    EXPECT_EQ(g_futs[0].get().error, error_type::NoError);
    auto refused = g_futs[1].get();
    EXPECT_EQ(refused.error, error_type::YieldError);
    EXPECT_EQ(refused.value.to<std::string>(), "yield full");
    auto refused2 = g_futs[2].get();
    EXPECT_EQ(refused2.value.to<std::string>(), "yield full");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, ExpiredNeverRuns) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    eng->set_extend("nap", native_nap);
    auto r = run(eng,
                 "def f() { $mark(\"ran\"); }"
                 "$post(\"f\", 0, 20); $nap(150); var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 0);
    EXPECT_TRUE(g_marks.empty());
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::YieldError);
    EXPECT_EQ(fr.value.to<std::string>(), "expired");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, ConcurrentDeliveriesExpireWhileTheEngineRuns) {
    engine* eng = engine::create();
    eng->set_extend("nap", native_nap);
    auto fut = std::async(std::launch::async, [&] {
        return run(eng, "$nap(300); var c = yield(0, 0); return c;");
    });
    for (int i = 0; i < 5000 && !eng->running(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(eng->running());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<std::future<result>> futs;
    for (int i = 0; i < 50; i++) futs.push_back(eng->yield("f", varvec(), 0, 10));

    auto r = fut.get();
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 0);
    for (auto& f : futs) {
        ASSERT_EQ(f.wait_for(std::chrono::seconds(2)), std::future_status::ready);
        auto fr = f.get();
        EXPECT_EQ(fr.error, error_type::YieldError);
        EXPECT_EQ(fr.value.to<std::string>(), "expired");
    }
    delete eng;
}

TEST(gt_ascript_yield, UnlimitedCapacityAdmitsEverything) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    eng->set_extend("shrink", native_shrink);
    std::string src = "$shrink(0); def f() { $mark(\"f\"); }";
    for (int i = 0; i < 300; i++) src += "$post(\"f\");";
    src += "var c = yield(0, 0); return c;";
    auto r = run(eng, src.c_str());
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 300);
    EXPECT_EQ(g_marks.size(), 300u);
    for (auto& f : g_futs) EXPECT_EQ(f.get().error, error_type::NoError);
    g_futs.clear();
    g_marks.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, NoYieldPointMeansNotExecuted) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto r = run(eng, "def f() { return 1; } $post(\"f\"); return 7;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 7);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::YieldError);
    EXPECT_EQ(fr.value.to<std::string>(), "not executed");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, UncaughtNestedYieldEndsThatDelivery) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    auto r = run(eng,
                 "def f() { yield(0, 0); $mark(\"after\"); }"
                 "$post(\"f\"); var c = yield(0, 0); var x = 5; return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    EXPECT_TRUE(g_marks.empty());
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::YieldError);
    EXPECT_EQ(fr.value.to<std::string>(), "nested yield");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, AServicedCallMayCatchItsNestedYield) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    auto r = run(eng,
                 "def f() { try { yield(0, 0); } catch (e) { return e[\"what\"]; } return \"missed\"; }"
                 "$post(\"f\"); var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::NoError);
    EXPECT_EQ(fr.value.to<std::string>(), "YieldError");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, InitYieldIsAScriptLevelError) {
    std::string good = write_mod("alx_yield_init.axc",
                                 "var caught = \"\";"
                                 "try { yield(0, 0); } catch (e) { caught = e[\"what\"]; }"
                                 "def status() { return caught; }");
    std::string bad = write_mod("alx_yield_initbad.axc", "yield(0, 0);");

    engine* eng = engine::create();
    auto r = run(eng, ("import \"" + good + "\" as m; return m.status();").c_str());
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<std::string>(), "YieldError");
    delete eng;

    engine* eng2 = engine::create();
    auto r2 = run(eng2, ("import \"" + bad + "\" as m; return 1;").c_str());
    EXPECT_EQ(r2.error, error_type::ImportError);
    EXPECT_NE(r2.value.to<std::string>().find("YieldError"), std::string::npos);
    delete eng2;
}

TEST(gt_ascript_yield, ServicedCallRunsUnderRootPose) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto r = run(eng,
                 "def f() { return local; }"
                 "def main() { var local = 5; $post(\"f\"); yield(0, 0); return 1; }"
                 "main();");
    ASSERT_EQ(r.error, error_type::NoError);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::NameError);
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, MissingNameFailsInsideTheFuture) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto r = run(eng, "$post(\"nosuch\"); var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::NameError);
    EXPECT_EQ(fr.value.to<std::string>(), "function not found: nosuch");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, ServicedOutcomeMatchesCall) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto setup = run(eng, "def f() { return [1, 2][9]; }");
    ASSERT_EQ(setup.error, error_type::NoError);
    auto direct = eng->call("f", varvec());
    ASSERT_EQ(direct.error, error_type::IndexError);

    auto r = run(eng, "$post(\"f\"); var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, direct.error);
    EXPECT_EQ(fr.value.to<std::string>(), direct.value.to<std::string>());
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, CallRvalueOverloads) {
    engine* eng = engine::create();
    auto r0 = run(eng, "def f(a) { return a + 1; }");
    ASSERT_EQ(r0.error, error_type::NoError);

    varvec args;
    args.push_back(variant(static_cast<int_64>(9)));
    auto r = eng->call("f", std::move(args));
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 10);

    varvec keep;
    keep.push_back(variant(static_cast<int_64>(9)));
    auto r2 = eng->call("nosuch", std::move(keep));
    EXPECT_EQ(r2.error, error_type::NameError);
    EXPECT_EQ(keep.size(), 1u);
    EXPECT_EQ(keep[0].to<int_64>(), 9);
    delete eng;
}

TEST(gt_ascript_yield, HardInterruptSettlesEverything) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    auto fut = std::async(std::launch::async, [&] {
        return run(eng,
                   "def slow() { var i = 0; while (1) { i = i + 1; } }"
                   "$post(\"slow\"); yield(0, 0); return 1;");
    });
    for (int i = 0; i < 5000 && !eng->running(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(eng->running());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    eng->set_interrupt();
    auto r = fut.get();
    EXPECT_EQ(r.error, error_type::InterruptedError);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::InterruptedError);
    EXPECT_EQ(fr.value.to<std::string>(), "interrupted");
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, InterruptWakesTheResidencyWait) {
    engine* eng = engine::create();
    auto fut = std::async(std::launch::async, [&] {
        return run(eng, "yield(30000, 0); return 1;");
    });
    for (int i = 0; i < 5000 && !eng->running(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(eng->running());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    eng->set_interrupt();
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(fut.get().error, error_type::InterruptedError);
    delete eng;
}

TEST(gt_ascript_yield, CurryingHandleDeliversFromAWorkerThread) {
    engine* eng = engine::create();
    g_eng = eng;
    g_cb = nullptr;
    eng->set_extend("curry", native_curry);
    auto fut = std::async(std::launch::async, [&] {
        return run(eng,
                   "def f(a) { return a * 3; }"
                   "$curry(\"f\"); var c = yield(3000, 2); return c;");
    });
    for (int i = 0; i < 5000 && !g_cb; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_NE(g_cb.load(), nullptr);

    varvec a1;
    a1.push_back(variant(static_cast<int_64>(7)));
    auto f1 = g_cb.load()->call(a1);
    varvec a2;
    a2.push_back(variant(static_cast<int_64>(2)));
    auto f2 = g_cb.load()->call(std::move(a2));
    auto r1 = f1.get();
    auto r2 = f2.get();
    EXPECT_EQ(r1.error, error_type::NoError);
    EXPECT_EQ(r1.value.to<int_64>(), 21);
    EXPECT_EQ(r2.error, error_type::NoError);
    EXPECT_EQ(r2.value.to<int_64>(), 6);
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(fut.get().value.to<int_64>(), 2);
    delete g_cb.load();
    g_cb = nullptr;
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, CurryingAllowsALaterDefinition) {
    engine* eng = engine::create();
    g_eng = eng;
    g_cb = nullptr;
    eng->set_extend("curry", native_curry);
    auto fut = std::async(std::launch::async, [&] {
        return run(eng,
                   "$curry(\"later\"); def later(a) { return a + 1; } yield(3000, 1); return 1;");
    });
    for (int i = 0; i < 5000 && !g_cb; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_NE(g_cb.load(), nullptr);
    varvec args;
    args.push_back(variant(static_cast<int_64>(41)));
    auto r = g_cb.load()->call(args).get();
    EXPECT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 42);
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(fut.get().error, error_type::NoError);
    delete g_cb.load();
    g_cb = nullptr;
    delete eng;
    g_eng = nullptr;
}

#ifdef ALX_TEST_LINK_CURRY_MINT_SO
TEST(gt_ascript_yield, ADeletedHandleLeavesTheEngineRunning) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    std::string src = std::string("link \"") + ALX_TEST_LINK_CURRY_MINT_SO + "\" as t;";
    src += "def f(a) { return a + 1; } t.mintdrop();";
    src += "$post(\"f\", 0, 0, 41); var c = yield(0, 0); return c;";
    auto r = run(eng, src.c_str());
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::NoError);
    EXPECT_EQ(fr.value.to<int_64>(), 42);
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}
#endif

#ifdef ALX_TEST_LINK_CURRY_LOAD_SO
TEST(gt_ascript_yield, CurryingRefusedInLoadEntry) {
    engine* eng = engine::create();
    auto r = run(eng, (std::string("link \"") + ALX_TEST_LINK_CURRY_LOAD_SO + "\" as t; 1;").c_str());
    EXPECT_EQ(r.error, error_type::LinkError);
    EXPECT_NE(r.value.to<std::string>().find("currying not available"), std::string::npos);
    delete eng;
}
#endif

#ifdef ALX_TEST_LINK_CURRY_CREATE_SO
TEST(gt_ascript_yield, CurryingRefusedInCreateEntry) {
    engine* eng = engine::create();
    auto r = run(eng,
                 (std::string("link \"") + ALX_TEST_LINK_CURRY_CREATE_SO + "\" as t; 1;").c_str());
    EXPECT_EQ(r.error, error_type::LinkError);
    EXPECT_NE(r.value.to<std::string>().find("currying not available"), std::string::npos);
    delete eng;
}
#endif

TEST(gt_ascript_yield, HardInterruptDrainsTheBacklog) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_marks.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("mark", native_mark);
    auto fut = std::async(std::launch::async, [&] {
        return run(eng,
                   "def slow() { var i = 0; while (1) { i = i + 1; } }"
                   "def later() { $mark(\"later\"); }"
                   "$post(\"slow\"); $post(\"later\"); yield(0, 0); return 1;");
    });
    for (int i = 0; i < 5000 && !eng->running(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(eng->running());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    eng->set_interrupt();
    auto r = fut.get();
    EXPECT_EQ(r.error, error_type::InterruptedError);
    ASSERT_EQ(g_futs.size(), 2u);
    for (auto& f : g_futs) {
        auto fr = f.get();
        EXPECT_EQ(fr.error, error_type::InterruptedError);
    }
    EXPECT_TRUE(g_marks.empty());
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, TheHookRejectReasonSettlesDeliveries) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    g_armed = false;
    eng->set_extend("post_arm", native_post_arm);
    eng->set_hook(reject_hook, nullptr, 1);
    auto r = run(eng, "def f() { } $post_arm(\"f\"); yield(0, 0); return 1;");
    EXPECT_EQ(r.error, error_type::InterruptedError);
    ASSERT_EQ(g_futs.size(), 1u);
    auto fr = g_futs[0].get();
    EXPECT_EQ(fr.error, error_type::InterruptedError);
    EXPECT_EQ(fr.value.to<std::string>(), "reject");
    g_armed = false;
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, MaxYieldChangeAdmitsImmediately) {
    engine* eng = engine::create();
    g_eng = eng;
    g_futs.clear();
    eng->set_extend("post", native_post);
    eng->set_extend("shrink", native_shrink);
    auto r = run(eng,
                 "def f() { } $shrink(1); $post(\"f\"); $post(\"f\"); $post(\"f\");"
                 "var c = yield(0, 0); return c;");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<int_64>(), 1);
    ASSERT_EQ(g_futs.size(), 3u);
    EXPECT_EQ(g_futs[0].get().error, error_type::NoError);
    for (int i = 1; i < 3; i++) {
        auto fr = g_futs[i].get();
        EXPECT_EQ(fr.error, error_type::YieldError);
        EXPECT_EQ(fr.value.to<std::string>(), "yield full");
    }
    g_futs.clear();
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, AHandleSurvivesReset) {
    engine* eng = engine::create();
    g_eng = eng;
    g_cb = nullptr;
    eng->set_extend("curry", native_curry);
    auto r = run(eng, "def f() { return 1; } $curry(\"f\"); return 0;");
    ASSERT_EQ(r.error, error_type::NoError);
    ASSERT_NE(g_cb.load(), nullptr);
    eng->reset();
    varvec args;
    auto fr = g_cb.load()->call(args).get();
    EXPECT_EQ(fr.error, error_type::YieldError);
    EXPECT_EQ(fr.value.to<std::string>(), "no main fiber");
    delete g_cb.load();
    g_cb = nullptr;
    delete eng;
    g_eng = nullptr;
}

TEST(gt_ascript_yield, YieldIsReserved) {
    engine* eng = engine::create();
    EXPECT_FALSE(eng->set_extend("yield", native_mark));
    auto r = run(eng, "var yield = 1;");
    EXPECT_EQ(r.error, error_type::ParseError);
    delete eng;
}

TEST(gt_ascript_yield, DefaultCapacityIs256) {
    engine* eng = engine::create();
    EXPECT_EQ(eng->config().max_yield, 256u);
    delete eng;
}

TEST(gt_ascript_yield, MaxYieldConfig) {
    engine_config cfg;
    cfg.max_yield = 3;
    engine* eng = engine::create(cfg);
    EXPECT_EQ(eng->config().max_yield, 3u);
    eng->set_max_yield(0);
    EXPECT_EQ(eng->config().max_yield, 0u);
    delete eng;
}
