/*****************************************************************/ /**
 * \file   gt_afiber.cpp
 * \brief  Unit tests for fiber (lightweight coroutines)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afiber.h"
#include <gtest/gtest.h>

using namespace alx;

class gt_afiber : public ::testing::Test {
protected:
    void SetUp() override { fiber::initialize(); }
    void TearDown() override { fiber::deinitialize(); }
};

TEST_F(gt_afiber, initialize_creates_main) {
    EXPECT_NE(fiber::main(), nullptr);
    EXPECT_EQ(fiber::main(), fiber::current());
    EXPECT_EQ(fiber::main()->get_state(), fiber::FIBER_RUNNING);
}

TEST_F(gt_afiber, create_yield_state) {
    fiber::state captured = {};

    fiber* f = fiber::create([&]() {
        captured = fiber::current()->get_state();
        fiber::yield();
        captured = fiber::current()->get_state();
    });
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->get_state(), fiber::FIBER_READY);

    fiber::yield(f);
    EXPECT_EQ(captured, fiber::FIBER_RUNNING);
    EXPECT_NE(f->get_state(), fiber::FIBER_FINISHED);

    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);

    delete f;
}

TEST_F(gt_afiber, destroy_before_run) {
    fiber* f = fiber::create([]() {});
    ASSERT_NE(f, nullptr);
    delete f;
}

TEST_F(gt_afiber, state_ready_to_running) {
    fiber* f = fiber::create([&]() {
        EXPECT_EQ(fiber::current()->get_state(), fiber::FIBER_RUNNING);
    });
    EXPECT_EQ(f->get_state(), fiber::FIBER_READY);
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, state_suspended) {
    fiber* f = fiber::create([&]() {
        fiber::yield();
    });
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_SUSPENDED);
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, state_finished) {
    fiber* f = fiber::create([]() {   });
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);

    fiber::yield(f);
    EXPECT_EQ(fiber::current(), fiber::main());
    delete f;
}

TEST_F(gt_afiber, two_fibers_ping_pong) {
    std::string trace;

    fiber* a = fiber::create([&]() {
        trace += "A1>";
        fiber::yield();
        trace += "A2>";
    });
    fiber* b = fiber::create([&]() {
        trace += "B1>";
        fiber::yield();
        trace += "B2>";
    });

    fiber::yield(a);
    fiber::yield(b);
    fiber::yield(a);
    fiber::yield(b);

    EXPECT_EQ(trace, "A1>B1>A2>B2>");
    EXPECT_EQ(a->get_state(), fiber::FIBER_FINISHED);
    EXPECT_EQ(b->get_state(), fiber::FIBER_FINISHED);
    delete a;
    delete b;
}

TEST_F(gt_afiber, set_and_get_data) {
    fiber* f = fiber::create([]() {});
    f->set_data(42);
    EXPECT_EQ(f->get_data().to<int>(), 42);

    f->set_data(std::string("hello"));
    EXPECT_EQ(f->get_data().to_string(), "hello");

    varmap map;
    map.insert("key", 123);
    f->set_data(map);
    EXPECT_EQ(f->get_data().to<varmap>().value("key").to<int>(), 123);

    delete f;
}

TEST_F(gt_afiber, data_accessible_in_fiber) {
    int val = 0;
    fiber* f = fiber::create([&]() {
        val = fiber::current()->get_data().to<int>();
    });
    f->set_data(99);
    fiber::yield(f);
    EXPECT_EQ(val, 99);
    delete f;
}

TEST_F(gt_afiber, main_is_current_outside_fiber) {
    EXPECT_EQ(fiber::current(), fiber::main());
}

TEST_F(gt_afiber, current_in_fiber) {
    fiber* created = nullptr;
    fiber* f = fiber::create([&]() {
        created = fiber::current();
        EXPECT_EQ(created, f);
        EXPECT_NE(created, fiber::main());
    });
    fiber::yield(f);
    EXPECT_EQ(created, f);
    delete f;
}

TEST_F(gt_afiber, is_fiber_in_fiber) {
    bool inside = false;
    fiber* f = fiber::create([&]() { inside = fiber::is_fiber(); });
    fiber::yield(f);
    EXPECT_TRUE(inside);
    delete f;
}

TEST_F(gt_afiber, yield_no_args_returns_to_main) {
    bool reached = false;
    fiber* f = fiber::create([&]() {
        reached = true;
        fiber::yield();
    });
    fiber::yield(f);
    EXPECT_TRUE(reached);
    EXPECT_EQ(fiber::current(), fiber::main());
    delete f;
}

TEST_F(gt_afiber, yield_to_self_noop) {
    fiber* f = fiber::create([&]() {
        fiber::yield(fiber::current());
        fiber::yield();
    });
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_SUSPENDED);
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, yield_to_nullptr_noop) {
    fiber* f = fiber::create([]() {});
    fiber::yield(nullptr);
    fiber::yield(f);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, fiber_throws_no_crash) {
    fiber* f = fiber::create([]() { throw std::runtime_error("boom"); });
    EXPECT_NO_THROW(fiber::yield(f));
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    EXPECT_EQ(fiber::current(), fiber::main());
    delete f;
}

TEST_F(gt_afiber, fiber_runs_after_exception) {
    bool second_ran = false;
    fiber* f1 = fiber::create([]() { throw 42; });
    fiber* f2 = fiber::create([&]() { second_ran = true; });

    fiber::yield(f1);
    fiber::yield(f2);
    EXPECT_TRUE(second_ran);
    delete f1;
    delete f2;
}

TEST_F(gt_afiber, custom_stack_size) {
    bool ran = false;
    fiber* f = fiber::create(
        [&]() { ran = true; },
        16384, 65536);
    ASSERT_NE(f, nullptr);

    fiber::yield(f);
    EXPECT_TRUE(ran);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, invalid_stack_size_returns_null) {
    fiber* f = fiber::create([]() {}, 1024, 512);
    EXPECT_EQ(f, nullptr);
}

TEST_F(gt_afiber, large_frame_fits_default_stack) {
    bool ran = false;
    fiber* f = fiber::create([&]() {
        volatile uint_8 frame[256 * 1024];
        for (size_t i = 0; i < sizeof(frame); i += 4096) frame[i] = (uint_8) (i >> 12);
        ran = true;
    });
    ASSERT_NE(f, nullptr);

    fiber::yield(f);
    EXPECT_TRUE(ran);
    EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
    delete f;
}

TEST_F(gt_afiber, recreate_after_finish) {
    for (int i = 0; i < 10; ++i) {
        int counter = 0;
        fiber* f = fiber::create([&]() { counter = i + 1; });
        fiber::yield(f);
        EXPECT_EQ(counter, i + 1);
        EXPECT_EQ(f->get_state(), fiber::FIBER_FINISHED);
        delete f;
    }
}

TEST_F(gt_afiber, multiple_fibers_not_run) {
    std::vector<fiber*> fibers;
    for (int i = 0; i < 20; ++i) {
        fibers.push_back(fiber::create([i]() { (void) i; }));
        ASSERT_NE(fibers.back(), nullptr);
    }
    for (auto* f : fibers) delete f;
}
