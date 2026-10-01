/*****************************************************************/ /**
 * \file   gt_athread_safe.cpp
 * \brief  safe_queue tests — destroy wakeups
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "athread_safe.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>

using namespace alx;

TEST(gt_athread_safe, DestroyWakesBlockedPop) {

    for (int round = 0; round < 200; round++) {
        safe_queue<int> q;
        std::atomic<bool> returned{false};
        std::thread t([&] {
            int val = 0;
            q.block_pop(val);
            returned = true;
        });
        std::this_thread::yield();
        q.destroy();
        t.join();
        ASSERT_TRUE(returned.load()) << "round " << round;
    }
}

TEST(gt_athread_safe, DestroyWakesBlockedPush) {

    for (int round = 0; round < 200; round++) {
        safe_queue<int> q;
        q.push(1);
        std::atomic<bool> returned{false};
        std::thread t([&] {
            q.block_push(2, 1);
            returned = true;
        });
        std::this_thread::yield();
        q.destroy();
        t.join();
        ASSERT_TRUE(returned.load()) << "round " << round;
    }
}

TEST(gt_athread_safe, DestroyThenPushPopRejected) {
    safe_queue<int> q;
    q.push(1);
    q.destroy();
    EXPECT_TRUE(q.is_destroy());
    EXPECT_TRUE(q.empty());
    int val = 0;
    EXPECT_FALSE(q.pop(val));
    q.push(2);
    EXPECT_TRUE(q.empty());
}
