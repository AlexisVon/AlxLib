/*****************************************************************/ /**
 * \file   gt_arpc.cpp
 * \brief  Unit tests for RPC framework (service_pack, service registry, call/response)
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "arpc.h"
#include "test_mocks.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace alx;
using namespace alx::test;

namespace {
    static bool wait_for(std::function<bool()> _cond, int _ms = 10000) {
        for (int i = 0; i < _ms / 50 && !_cond(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return _cond();
    }
}

TEST(gt_arpc, service_pack_create_basic) {
    auto sp = rpc::service_pack::create<int>(
        [](int _x, std::string& _err) -> int {
            return _x * 2;
        },
        "double_int");
    ASSERT_TRUE(sp.valid());

    varvec in;
    in.push_back(21);
    variant out;
    std::string err;
    sp.__function(in, out, err);
    EXPECT_EQ(err, "");
    EXPECT_EQ(out.to<int>(), 42);
}

TEST(gt_arpc, service_pack_create_multi_arg) {
    auto sp = rpc::service_pack::create<std::string, int>(
        [](const std::string& _a, int _b, std::string& _err) -> std::string {
            return _a + std::to_string(_b);
        },
        "concat");
    ASSERT_TRUE(sp.valid());

    varvec in;
    in.push_back(std::string("val"));
    in.push_back(99);
    variant out;
    std::string err;
    sp.__function(in, out, err);
    EXPECT_EQ(err, "");
    EXPECT_EQ(out.to_string(), "val99");
}

TEST(gt_arpc, service_pack_create_noerr) {
    auto sp = rpc::service_pack::create_noerr<int>(
        [](int _x) -> int {
            return _x + 10;
        },
        "add_ten");
    ASSERT_TRUE(sp.valid());

    varvec in;
    in.push_back(5);
    variant out;
    std::string err;
    sp.__function(in, out, err);
    EXPECT_EQ(err, "");
    EXPECT_EQ(out.to<int>(), 15);
}

TEST(gt_arpc, service_pack_invalid_param_count) {

    auto sp = rpc::service_pack::create<int>(
        [](int, std::string&) -> int { return 0; },
        "broken");

    varvec in;
    variant out;
    std::string err;
    sp.__function(in, out, err);
    EXPECT_EQ(err, "invalid params");
}

TEST(gt_arpc, consume_pack_create) {
    auto cp = rpc::consume_pack::create<int, std::string>(
        42,
        [](const variant&, const std::string&) {},
        false, rpc::cmps_expect::AUTO,
        123, std::string("hello"));
    ASSERT_TRUE(cp.valid());
    EXPECT_EQ(cp.__svid, 42);
    EXPECT_EQ(cp.__param.size(), 2);
    EXPECT_EQ(cp.__param[0].to<int>(), 123);
    EXPECT_EQ(cp.__param[1].to_string(), "hello");
}

TEST(gt_arpc, install_and_list) {
    auto* mock = new MockTransmit();
    rpc svc(mock, 1, 0xFF);
    ASSERT_TRUE(svc.open());

    auto sp = rpc::service_pack::create_noerr<>(
        []() -> int { return 1; },
        "test_svc");
    uint_64 svid = svc.install_service(sp);
    EXPECT_GT(svid, 0);

    varmap list = svc.list_service();
    EXPECT_TRUE(list.value(std::to_string(svid)).is_string());

    mock->connect_num_result = 0;
}

TEST(gt_arpc, install_multiple) {
    auto* mock = new MockTransmit();
    rpc svc(mock, 1, 0xFF);
    ASSERT_TRUE(svc.open());

    svc.install_service(rpc::service_pack::create_noerr<>(
        []() -> int { return 0; }, "s1"));
    svc.install_service(rpc::service_pack::create_noerr<>(
        []() -> int { return 0; }, "s2"));
    svc.install_service(rpc::service_pack::create_noerr<>(
        []() -> int { return 0; }, "s3"));

    varmap list = svc.list_service();
    EXPECT_EQ(list.size(), 3);

    mock->connect_num_result = 0;
}

TEST(gt_arpc, remove_service) {
    auto* mock = new MockTransmit();
    rpc svc(mock, 1, 0xFF);
    ASSERT_TRUE(svc.open());

    uint_64 svid = svc.install_service(rpc::service_pack::create_noerr<>(
        []() -> int { return 1; }, "remove_me"));
    EXPECT_TRUE(svc.list_service().value(std::to_string(svid)).is_string());

    svc.remove_service(svid);
    EXPECT_FALSE(svc.list_service().value(std::to_string(svid)).is_string());

    mock->connect_num_result = 0;
}

struct RpcPair {
    MockTransmit *srv_mock, *cli_mock;
    rpc *srv, *cli;

    RpcPair(uint_64 _pool = 1) {
        srv_mock = new MockTransmit();
        cli_mock = new MockTransmit();

        srv_mock->on_send = [this](const bytes_view& _d, uint_64 _l) {
            cli_mock->inject_recv(_d, _l);
        };
        cli_mock->on_send = [this](const bytes_view& _d, uint_64 _l) {
            srv_mock->inject_recv(_d, _l);
        };

        srv = new rpc(srv_mock, _pool, 0xFF);
        cli = new rpc(cli_mock, _pool, 0xFF);
    }

    ~RpcPair() {
        srv_mock->connect_num_result = 0;
        cli_mock->connect_num_result = 0;
        delete srv;
        delete cli;
    }
};

TEST(gt_arpc, basic_rpc_call) {
    RpcPair pair;
    ASSERT_TRUE(pair.srv->open());
    ASSERT_TRUE(pair.cli->open());

    uint_64 svid = pair.srv->install_service(
        rpc::service_pack::create_noerr<int>(
            [](int _x) -> int { return _x * 2; },
            "double", rpc::cmps_option::NEVER, true));

    std::atomic<bool> done{false};
    variant result;
    std::string error;

    pair.cli->consume_service(rpc::consume_pack::create<int>(
        svid,
        [&](const variant& _r, const std::string& _e) {
            result = _r;
            error = _e;
            done = true;
        },
        false, rpc::cmps_expect::AUTO,
        21));

    ASSERT_TRUE(wait_for([&] { return done.load(); }));
    EXPECT_EQ(error, "");
    EXPECT_EQ(result.to<int>(), 42);
}

TEST(gt_arpc, rpc_call_string_arg) {
    RpcPair pair;
    ASSERT_TRUE(pair.srv->open());
    ASSERT_TRUE(pair.cli->open());

    uint_64 svid = pair.srv->install_service(
        rpc::service_pack::create_noerr<std::string>(
            [](const std::string& _s) -> std::string { return _s + _s; },
            "echo_twice", rpc::cmps_option::NEVER, true));

    std::atomic<bool> done{false};
    variant result;
    std::string error;

    pair.cli->consume_service(rpc::consume_pack::create<std::string>(
        svid,
        [&](const variant& _r, const std::string& _e) {
            result = _r;
            error = _e;
            done = true;
        },
        false, rpc::cmps_expect::AUTO,
        std::string("ab")));

    ASSERT_TRUE(wait_for([&] { return done.load(); }));
    EXPECT_EQ(error, "");
    EXPECT_EQ(result.to_string(), "abab");
}

TEST(gt_arpc, service_throws_exception) {
    RpcPair pair;
    ASSERT_TRUE(pair.srv->open());
    ASSERT_TRUE(pair.cli->open());

    uint_64 svid = pair.srv->install_service(
        rpc::service_pack::create_noerr<>(
            []() -> int { throw std::runtime_error("boom"); return 0; },
            "thrower", rpc::cmps_option::NEVER, true));

    std::atomic<bool> done{false};
    std::string error;

    pair.cli->consume_service(rpc::consume_pack::create<>(
        svid,
        [&](const variant&, const std::string& _e) {
            error = _e;
            done = true;
        },
        false, rpc::cmps_expect::AUTO));

    ASSERT_TRUE(wait_for([&] { return done.load(); }));
    EXPECT_NE(error.find("exception"), std::string::npos);
}

TEST(gt_arpc, unknown_svid) {
    RpcPair pair;
    ASSERT_TRUE(pair.srv->open());
    ASSERT_TRUE(pair.cli->open());

    std::atomic<bool> done{false};
    std::string error;

    pair.cli->consume_service(rpc::consume_pack::create<>(
        99999,
        [&](const variant&, const std::string& _e) {
            error = _e;
            done = true;
        },
        false, rpc::cmps_expect::AUTO));

    ASSERT_TRUE(wait_for([&] { return done.load(); }));
    EXPECT_EQ(error, "service not found");
}

TEST(gt_arpc, threadpool_exec) {

    RpcPair pair(2);
    ASSERT_TRUE(pair.srv->open());
    ASSERT_TRUE(pair.cli->open());

    uint_64 svid = pair.srv->install_service(
        rpc::service_pack::create_noerr<int>(
            [](int _x) -> int { return _x + 1; },
            "inc", rpc::cmps_option::NEVER, false));

    std::atomic<bool> done{false};
    variant result;

    pair.cli->consume_service(rpc::consume_pack::create<int>(
        svid,
        [&](const variant& _r, const std::string&) {
            result = _r;
            done = true;
        },
        false, rpc::cmps_expect::AUTO,
        10));

    ASSERT_TRUE(wait_for([&] { return done.load(); }));
    EXPECT_EQ(result.to<int>(), 11);
}

TEST(gt_arpc, list_service_concurrent_install_remove) {
    auto* mock = new MockTransmit();
    rpc svc(mock, 1, 0xFF);
    ASSERT_TRUE(svc.open());

    std::atomic<bool> stop{false};
    std::thread reader([&] {
        while (!stop.load()) {
            varmap list = svc.list_service();
            (void) list.size();
        }
    });
    for (int i = 0; i < 1000; i++) {
        const uint_64 svid = svc.install_service(rpc::service_pack::create_noerr<>(
            []() -> int { return 1; }, "racer"));
        svc.remove_service(svid);
    }
    stop = true;
    reader.join();

    mock->connect_num_result = 0;
}
