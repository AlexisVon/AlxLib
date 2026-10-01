/*****************************************************************/ /**
 * \file   gt_acomm_ex.cpp
 * \brief  Unit tests for comm_ex wire protocol (pack/unpack/stream reassembly)
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acomm_ex.h"
#include "test_mocks.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace alx;
using namespace alx::test;

namespace {
    static bool wait_for(std::function<bool()> _cond, int _ms = 10000) {
        for (int i = 0; i < _ms / 50 && !_cond(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return _cond();
    }

    static struct PackerAnchor {
        MockTransmit* mock;
        comm_ex* ce;
        PackerAnchor() {
            mock = new MockTransmit();
            ce = new comm_ex(mock);
            ce->open();

            bytes dummy(64, '\0');
            mock->inject_recv(dummy);
        }
        ~PackerAnchor() {
            mock->connect_num_result = 0;
            delete ce;
        }
    } g_anchor;
}

TEST(gt_acomm_ex, roundtrip_basic) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("key", std::string("hello"));
    sent.insert("num", 42);
    sent.insert("flag", true);

    varmap received;
    uint_64 received_loc = uint_64_npos;
    ce.data_recv.connect([&](const varmap& _d, uint_64 _loc) {
        received = _d;
        received_loc = _loc;
    });

    ASSERT_TRUE(ce.data_send(sent, false));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    mock->inject_recv(mock->last_sent, 99);

    EXPECT_EQ(received.value("key").to_string(), "hello");
    EXPECT_EQ(received.value("num").to<int>(), 42);
    EXPECT_EQ(received.value("flag").to<bool>(), true);
    EXPECT_EQ(received_loc, 99);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, roundtrip_compress) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    std::string big(500, 'A');
    varmap sent;
    sent.insert("data", big);

    int count = 0;
    varmap received;
    ce.data_recv.connect([&](const varmap& _d, uint_64) {
        received = _d;
        count++;
    });

    ASSERT_TRUE(ce.data_send(sent, 0, true));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    EXPECT_LT(mock->last_sent.size(), big.size() + 64);

    mock->inject_recv(mock->last_sent);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(received.value("data").to_string(), big);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, send_empty_noop) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    ASSERT_TRUE(ce.data_send(varmap(), false));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(mock->last_sent.size(), 0);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, fragmented_receive) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("val", std::string(200, 'B'));

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    ce.data_send(sent, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    bytes full = mock->last_sent;
    ASSERT_GT(full.size(), 20);

    uint_64 a = full.size() / 3;
    uint_64 b = a * 2;
    mock->inject_recv(full.mid_view(0, a));
    EXPECT_EQ(count, 0);
    mock->inject_recv(full.mid_view(a, a));
    EXPECT_EQ(count, 0);
    mock->inject_recv(full.mid_view(b));
    EXPECT_EQ(count, 1);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, multiple_frames) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap s1, s2;
    s1.insert("id", 1);
    s2.insert("id", 2);

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    ce.data_send(s1, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));
    bytes p1 = mock->last_sent;

    ce.data_send(s2, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() != p1.size() || memcmp(mock->last_sent.data(), p1.data(), p1.size()) != 0; }));
    bytes p2 = mock->last_sent;

    bytes combined = p1;
    combined.append(p2);
    mock->inject_recv(combined);
    EXPECT_EQ(count, 2);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, corrupted_head) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("x", 1);
    ce.data_send(sent, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    bytes bad = mock->last_sent;
    bad.data()[0] ^= 0xFF;
    mock->inject_recv(bad);
    EXPECT_EQ(count, 0);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, corrupted_tail) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("x", 1);
    ce.data_send(sent, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    bytes bad = mock->last_sent;
    bad.data()[bad.size() - 1] ^= 0xFF;
    mock->inject_recv(bad);
    EXPECT_EQ(count, 0);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, corrupted_checksum) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("data", std::string(100, 'C'));
    ce.data_send(sent, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    bytes bad = mock->last_sent;
    uint_64 payload_start = 8 + 64;
    bad.data()[payload_start + 10] ^= 0xFF;
    mock->inject_recv(bad);
    EXPECT_EQ(count, 0);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, size_mismatch) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("x", 1);
    ce.data_send(sent, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    int count = 0;
    ce.data_recv.connect([&](const varmap&, uint_64) { count++; });

    bytes bad = mock->last_sent;
    bad = bad.mid(0, bad.size() - 4);
    mock->inject_recv(bad);
    EXPECT_EQ(count, 0);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, loc_passthrough) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("x", 1);

    std::vector<uint_64> locs;
    ce.data_recv.connect([&](const varmap&, uint_64 _loc) { locs.push_back(_loc); });

    ce.data_send(sent, 0xABCD, false);
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    mock->inject_recv(mock->last_sent, 0xABCD);
    ASSERT_EQ(locs.size(), 1);
    EXPECT_EQ(locs[0], 0xABCD);

    mock->connect_num_result = 0;
}

TEST(gt_acomm_ex, large_varmap) {
    auto* mock = new MockTransmit();
    comm_ex ce(mock);
    ASSERT_TRUE(ce.open());

    varmap sent;
    sent.insert("big", std::string(65536, 'D'));

    varmap received;
    ce.data_recv.connect([&](const varmap& _d, uint_64) { received = _d; });

    ASSERT_TRUE(ce.data_send(sent, false));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    mock->inject_recv(mock->last_sent);
    EXPECT_EQ(received.value("big").to_string(), std::string(65536, 'D'));

    mock->connect_num_result = 0;
}
