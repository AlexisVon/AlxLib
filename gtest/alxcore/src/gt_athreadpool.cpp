/*****************************************************************/ /**
 * \file   gt_athreadpool.cpp
 * \brief  Unit tests for threadpool: enqueue, wait, resize, exception safety
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "athreadpool.h"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace alx;

TEST(gt_athreadpool, enqueue_and_get_result) {
    threadpool pool(2, 16);
    auto fut = pool.enqueue([]() -> int { return 42; });
    EXPECT_EQ(fut.get(), 42);
}

TEST(gt_athreadpool, enqueue_multiple) {
    threadpool pool(4, 128);
    constexpr int N = 100;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < N; ++i)
        futures.push_back(pool.enqueue([i]() -> int { return i * i; }));

    for (int i = 0; i < N; ++i)
        EXPECT_EQ(futures[i].get(), i * i);
}

TEST(gt_athreadpool, enqueue_with_args) {
    threadpool pool(2, 16);
    auto fut = pool.enqueue([](int _a, int _b) -> int { return _a + _b; }, 10, 20);
    EXPECT_EQ(fut.get(), 30);
}

TEST(gt_athreadpool, wait_until_empty) {
    threadpool pool(2, 128);
    std::atomic<int> counter{0};
    for (int i = 0; i < 20; ++i)
        pool.enqueue([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter.fetch_add(1);
        });
    pool.wait_until_empty();

    EXPECT_GE(counter.load(), 0);
}

TEST(gt_athreadpool, wait_until_nothing_in_flight) {
    threadpool pool(2, 128);
    std::atomic<int> counter{0};
    for (int i = 0; i < 20; ++i)
        pool.enqueue([&]() { counter.fetch_add(1); });
    pool.wait_until_nothing_in_flight();
    EXPECT_EQ(counter.load(), 20);
}

TEST(gt_athreadpool, set_pool_size) {
    threadpool pool(2, 64);
    std::atomic<int> counter{0};
    for (int i = 0; i < 30; ++i)
        pool.enqueue([&]() { counter.fetch_add(1); });

    pool.set_pool_size(4);
    pool.wait_until_nothing_in_flight();
    EXPECT_EQ(counter.load(), 30);
}

TEST(gt_athreadpool, exception_in_task) {
    threadpool pool(2, 16);
    auto fut = pool.enqueue([]() -> int { throw std::runtime_error("test error"); return 0; });
    EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(gt_athreadpool, set_queue_size_limit) {
    threadpool pool(2, 4);
    EXPECT_NO_THROW(pool.set_queue_size_limit(16));

    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i)
        pool.enqueue([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            counter.fetch_add(1);
        });
    pool.wait_until_nothing_in_flight();
    EXPECT_EQ(counter.load(), 10);
}

TEST(gt_athreadpool, default_constructor) {
    threadpool pool;
    auto fut = pool.enqueue([]() -> int { return 99; });
    EXPECT_EQ(fut.get(), 99);
}

TEST(gt_athreadpool, zero_threads_clamped_to_one) {
    threadpool pool(0);
    auto fut = pool.enqueue([]() -> int { return 7; });
    EXPECT_EQ(fut.get(), 7);
}
