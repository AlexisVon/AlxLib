/*****************************************************************/ /**
 * \file   gt_adatetime.cpp
 * \brief  Unit tests for datetime: construction, accessors, arithmetic, elapsed
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "adatetime.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace alx;

TEST(gt_adatetime, construct_and_accessors) {

    datetime dt(2025, 6, 15, 12, 30, 45, 0, 0);
    EXPECT_EQ(dt.year(), 2025);
    EXPECT_EQ(dt.month(), 6);
    EXPECT_EQ(dt.day(), 15);
    EXPECT_EQ(dt.hour(), 12);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 45);
}

TEST(gt_adatetime, add_seconds) {
    datetime dt(2025, 1, 1, 0, 0, 0, 0, 0);
    datetime later = dt.add_seconds(3600);
    EXPECT_EQ(later.hour(), 1);
}

TEST(gt_adatetime, add_milliseconds) {
    datetime dt(2025, 1, 1, 0, 0, 0, 0, 0);
    datetime later = dt.add_milliseconds(1000);
    EXPECT_EQ(later.second(), 1);
}

TEST(gt_adatetime, elapsed_ms) {
    datetime dt;
    dt.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    uint_64 elapsed = dt.elapsed_ms();
    EXPECT_GE(elapsed, 40);
}

TEST(gt_adatetime, to_string_format) {
    datetime dt(2025, 6, 15, 12, 30, 45, 0, 0);
    std::string s = dt.to_string();
    EXPECT_NE(s.find("2025"), std::string::npos);
    EXPECT_NE(s.find("06"), std::string::npos);
    EXPECT_NE(s.find("15"), std::string::npos);
}

TEST(gt_adatetime, current_is_valid) {
    datetime now = datetime::current();

    EXPECT_GE(now.year(), 2020);
}

TEST(gt_adatetime, add_microseconds) {
    datetime dt(2025, 1, 1, 0, 0, 0, 0, 0);
    datetime later = dt.add_microseconds(1500);
    EXPECT_GT(later.millisecond(), 0);
}

TEST(gt_adatetime, elapsed_us_ns) {
    datetime dt;
    dt.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint_64 us = dt.elapsed_us();
    uint_64 ns = dt.elapsed_ns();
    EXPECT_GT(us, 0);
    EXPECT_GT(ns, us);
}

TEST(gt_adatetime, default_constructor) {
    datetime dt;
    EXPECT_GE(dt.year(), 2020);
}
