/*****************************************************************/ /**
 * \file   gt_acomm.cpp
 * \brief  Unit tests for comm base class (with MockTransmit)
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acomm.h"
#include "test_mocks.h"

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

class TestComm : public comm {
public:
    TestComm(transmit* _trans)
        : comm(_trans) {}

    bytes last_recv;
    uint_64 last_loc{0};

protected:
    void on_bytes_recv(const bytes_view& _data, uint_64 _loc) override {
        last_recv.append(_data);
        last_loc = _loc;
    }
};

TEST(gt_acomm, open_close) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);

    EXPECT_FALSE(tc.is_open());
    ASSERT_TRUE(tc.open());
    EXPECT_TRUE(tc.is_open());

    mock->connect_num_result = 0;
    tc.close();
    EXPECT_FALSE(tc.is_open());

}

TEST(gt_acomm, bytes_send_forward) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    ASSERT_TRUE(tc.bytes_send(bytes_view(bytes("ping", 4))));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() >= 4; }));
    EXPECT_EQ(mock->last_sent, bytes("ping", 4));

    mock->connect_num_result = 0;
}

TEST(gt_acomm, on_bytes_recv_dispatch) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    mock->inject_recv(bytes_view(bytes("hello", 5)), 42);
    EXPECT_EQ(tc.last_recv, bytes("hello", 5));
    EXPECT_EQ(tc.last_loc, 42);

    mock->connect_num_result = 0;
}

TEST(gt_acomm, is_connect) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    mock->connect_num_result = 1;
    EXPECT_TRUE(tc.is_connect());

    mock->connect_num_result = 0;
    EXPECT_FALSE(tc.is_connect());

    mock->connect_num_result = 0;
}

TEST(gt_acomm, close_with_loc) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    tc.close(0xABCD);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    SUCCEED();

    mock->connect_num_result = 0;
}

TEST(gt_acomm, clear_queue) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    tc.bytes_send(bytes_view(bytes("data", 4)));
    tc.clear();

    SUCCEED();

    mock->connect_num_result = 0;
}

TEST(gt_acomm, bytes_send_when_not_open) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);

    EXPECT_FALSE(tc.bytes_send(bytes_view(bytes("test", 4))));

}

TEST(gt_acomm, is_valid_null_trans) {
    TestComm tc(nullptr);
    EXPECT_FALSE(tc.is_valid());
    EXPECT_FALSE(tc.open());
}

TEST(gt_acomm, reopen_after_close) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    mock->connect_num_result = 0;
    tc.close();
    EXPECT_FALSE(tc.is_open());

    mock->is_open_result = true;
    mock->connect_num_result = 1;
    ASSERT_TRUE(tc.open());
    EXPECT_TRUE(tc.is_open());

    mock->connect_num_result = 0;
}

TEST(gt_acomm, comm_flag_signal) {
    auto* mock = new MockTransmit();
    TestComm tc(mock);
    ASSERT_TRUE(tc.open());

    uint_64 flag_loc = 0;
    bool flag_connected = false;
    tc.comm_flag.connect([&](uint_64 _loc, bool _conn) {
        flag_loc = _loc;
        flag_connected = _conn;
    });

    mock->inject_connect(0x1234, true, "connected");
    EXPECT_EQ(flag_loc, 0x1234);
    EXPECT_TRUE(flag_connected);

    mock->connect_num_result = 0;
}
