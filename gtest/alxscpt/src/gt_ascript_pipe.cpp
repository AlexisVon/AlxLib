/** ****************************************************************
 * \file   gt_ascript_pipe.cpp
 * \brief  host IO pipe unit tests — set_pipe routing via fwrap
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

static std::string g_out;
static std::string g_in;

static void ext_pipe_test(fwrap& fw) {
    auto& cfg = fw.config();
    if (cfg.pipe_out_ptr) cfg.pipe_out_ptr(&g_out, cfg.pipe_out_ud);
    if (cfg.pipe_in_ptr) g_in = cfg.pipe_in_ptr(cfg.pipe_in_ud);
    fw.freturn(variant(cfg.pipe_in_ptr != nullptr));
}

TEST(gt_ascript_pipe, Default_AccessorsNull) {
    auto* eng = engine::create();
    eng->set_extend("pipe_probe", [](fwrap& fw) {
        auto& cfg = fw.config();
        fw.freturn(variant(cfg.pipe_in_ptr == nullptr && cfg.pipe_out_ptr == nullptr));
    });
    alx::bytes src("$pipe_probe();");
    auto res = eng->exec(bytes_view(src), "");
    ASSERT_EQ(res.error, error_type::NoError) << res.value.to<std::string>();
    EXPECT_TRUE(res.value.to<bool>());
    delete eng;
}

TEST(gt_ascript_pipe, SetPipe_ExtSeesChannels) {
    g_out = "hello";
    g_in = "from_host";
    auto* eng = engine::create();
    eng->set_extend("pipe_test", ext_pipe_test);
    eng->set_pipe([](void*) -> std::string { return "from_host"; },
                  [](const std::string* _s, void*) { g_out = *_s; });

    alx::bytes src("$pipe_test();");
    auto res = eng->exec(bytes_view(src), "");
    ASSERT_EQ(res.error, error_type::NoError) << res.value.to<std::string>();
    EXPECT_TRUE(res.value.to<bool>());
    EXPECT_EQ(g_in, "from_host");
    EXPECT_EQ(g_out, "hello");
    delete eng;
}

TEST(gt_ascript_pipe, SetPipe_PerEngineIsolation) {
    static std::string out_a, out_b;
    auto* eng_a = engine::create();
    auto* eng_b = engine::create();
    eng_a->set_extend("pipe_test", ext_pipe_test);
    eng_b->set_extend("pipe_test", ext_pipe_test);
    eng_a->set_pipe(nullptr, [](const std::string* _s, void*) { out_a = *_s; });
    eng_b->set_pipe(nullptr, [](const std::string* _s, void*) { out_b = *_s; });

    alx::bytes src("$pipe_test();");
    g_out = "x";
    out_a.clear();
    out_b.clear();
    ASSERT_EQ(eng_a->exec(bytes_view(src), "").error, error_type::NoError);
    EXPECT_EQ(out_a, "x");
    EXPECT_TRUE(out_b.empty());

    out_a.clear();
    out_b.clear();
    ASSERT_EQ(eng_b->exec(bytes_view(src), "").error, error_type::NoError);
    EXPECT_EQ(out_b, "x");
    EXPECT_TRUE(out_a.empty());
    delete eng_a;
    delete eng_b;
}

TEST(gt_ascript_pipe, SetPipe_ResetToNull) {
    auto* eng = engine::create();
    eng->set_extend("pipe_probe", [](fwrap& fw) {
        auto& cfg = fw.config();
        fw.freturn(variant(cfg.pipe_in_ptr == nullptr && cfg.pipe_out_ptr == nullptr));
    });
    eng->set_pipe([](void*) -> std::string { return "x"; },
                  [](const std::string*, void*) {});
    eng->set_pipe(nullptr, nullptr);

    alx::bytes src("$pipe_probe();");
    auto res = eng->exec(bytes_view(src), "");
    ASSERT_EQ(res.error, error_type::NoError) << res.value.to<std::string>();
    EXPECT_TRUE(res.value.to<bool>());
    delete eng;
}
