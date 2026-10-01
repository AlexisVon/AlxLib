/*****************************************************************/ /**
 * \file   gt_alogger.cpp
 * \brief  Unit tests for log_wapper RAII and global_logger interception
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "alogger.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <thread>

using namespace alx;

TEST(gt_alogger, log_wapper_basic) {

    {
        log_info info;
        info << "test message";
    }
    SUCCEED();
}

TEST(gt_alogger, log_wapper_with_type) {
    {
        log_debug dbg;
        dbg.type("TEST") << "debug message";
    }
    SUCCEED();
}

static std::string g_captured_msg;
static log_level g_captured_level = log_level::info;
static std::vector<log_level> g_levels;

static void intercept_handle(const std::string& _msg, log_level _lv) {
    g_captured_msg = _msg;
    g_captured_level = _lv;
}

static void levels_handle(const std::string&, log_level _lv) {
    g_levels.push_back(_lv);
}

TEST(gt_alogger, set_log_handle_intercept) {
    g_captured_msg.clear();
    auto old_handle = global_logger::instance()->set_log_handle(intercept_handle);

    {
        log_warn warn;
        warn << "intercepted";
    }

    EXPECT_NE(g_captured_msg.find("intercepted"), std::string::npos);
    EXPECT_EQ(g_captured_level, log_level::warn);

    global_logger::instance()->set_log_handle(old_handle);
}

TEST(gt_alogger, log_levels) {
    g_levels.clear();
    auto old_handle = global_logger::instance()->set_log_handle(levels_handle);

    {
        log_info() << "i";
        log_debug() << "d";
        log_error() << "e";
    }

    ASSERT_EQ(g_levels.size(), 3);
    EXPECT_EQ(g_levels[0], log_level::info);
    EXPECT_EQ(g_levels[1], log_level::debug);
    EXPECT_EQ(g_levels[2], log_level::error);

    global_logger::instance()->set_log_handle(old_handle);
}

TEST(gt_alogger, logger_from_json_string) {

    logger lg(R"({"prt_time":false,"prt_thrd":false,"prt_level":false,"stdout":{"type":"cout","level":"info","is_flush":false}})");
    lg.log("test message", log_level::info);
    SUCCEED();
}

TEST(gt_alogger, log_wapper_numeric) {
    g_captured_msg.clear();
    auto old = global_logger::instance()->set_log_handle(intercept_handle);
    {
        log_info info;
        info << 42 << 3.14;
    }
    EXPECT_NE(g_captured_msg.find("42"), std::string::npos);
    global_logger::instance()->set_log_handle(old);
}

TEST(gt_alogger, AsyncQueueLimitZeroDoesNotBlock) {

    const std::string cfg_head = R"({"prt_time":false,"prt_thrd":false,"prt_level":false,"file":{"type":"file","level":"info","is_flush":false,"is_async":true,"file_path":"tmp-alxcore-gtest/alx_gt_alogger_async.log",)";
    const std::string cfgs[2] = {cfg_head + R"("queue_limit":0}})", cfg_head + R"("is_append":false}})"};

    for (const std::string& cfg : cfgs) {
        auto* lg = new logger(cfg);
        std::atomic<bool> done{false};
        std::thread t([lg, &done] {
            lg->log("async line", log_level::info);
            done = true;
        });
        for (int i = 0; i < 200 && !done.load(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EXPECT_TRUE(done.load());
        if (done.load()) {
            t.join();
            delete lg;
        } else {
            t.detach();
        }
    }
    std::remove("tmp-alxcore-gtest/alx_gt_alogger_async.log");
}

TEST(gt_alogger, FirstLogRace) {

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++)
        threads.emplace_back([] { global_logger::instance()->log("race line", log_level::info); });
    for (auto& t : threads) t.join();
    SUCCEED();
}

TEST(gt_alogger, global_logger_singleton) {
    auto* a = global_logger::instance();
    auto* b = global_logger::instance();
    EXPECT_EQ(a, b);
}
