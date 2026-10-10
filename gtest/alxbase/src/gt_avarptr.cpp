/*****************************************************************/ /**
 * \file   gt_avarptr.cpp
 * \brief  Unit tests for variant + anyptr integration
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avariant.h"
#include <stdexcept>
#include <gtest/gtest.h>

using namespace alx;

namespace {
    struct Counter {
        int value;
        static int alive;
        explicit Counter(int v = 0) : value(v) { alive++; }
        Counter(const Counter& c) : value(c.value) { alive++; }
        ~Counter() { alive--; }
        static void reset() { alive = 0; }
    };
    int Counter::alive = 0;

    struct Apple {
        int x = 1;
    };
    struct Orange {
        int y = 2;
    };
    struct ThrowingCopy {
        ThrowingCopy() = default;
        ThrowingCopy(const ThrowingCopy&) { throw std::runtime_error("copy refused"); }
    };
}

TEST(gt_avarptr, construct_and_access) {
    Counter::reset();
    {
        variant v(anyptr_ex<Counter>::make(new Counter(42)));
        EXPECT_TRUE(v.is<anyptr>());
        EXPECT_FALSE(v.null());

        auto* c = anyptr_ex<Counter>::as(v.to<anyptr>());
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(c->value, 42);
        EXPECT_EQ(Counter::alive, 1);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, default_null) {
    variant v;
    EXPECT_TRUE(v.null());
    EXPECT_FALSE(v.is<anyptr>());
}

TEST(gt_avarptr, copy_variant_deep_copies_anyptr) {
    Counter::reset();
    {
        variant a(anyptr_ex<Counter>::make(new Counter(10)));
        variant b = a;
        EXPECT_EQ(Counter::alive, 2);

        auto* ca = anyptr_ex<Counter>::as(a.to<anyptr>());
        auto* cb = anyptr_ex<Counter>::as(b.to<anyptr>());
        ASSERT_NE(ca, nullptr);
        ASSERT_NE(cb, nullptr);
        EXPECT_NE(ca, cb);
        EXPECT_EQ(ca->value, 10);
        EXPECT_EQ(cb->value, 10);

        cb->value = 99;
        EXPECT_EQ(ca->value, 10);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, throwing_copy_leaves_the_handle_empty) {
    // a failed clone must leave a clean null handle, not the released pointer the destructor would delete twice
    auto h = anyptr_ex<ThrowingCopy>::make(new ThrowingCopy());
    auto src = anyptr_ex<ThrowingCopy>::make(new ThrowingCopy());
    EXPECT_THROW(h = src, std::runtime_error);
    EXPECT_TRUE(h.null());
    EXPECT_EQ(anyptr_ex<ThrowingCopy>::as(h), nullptr);
}

TEST(gt_avarptr, move_variant) {
    Counter::reset();
    {
        variant a(anyptr_ex<Counter>::make(new Counter(5)));
        variant b = std::move(a);
        EXPECT_EQ(Counter::alive, 1);

        EXPECT_TRUE(a.null());
        EXPECT_FALSE(b.null());

        auto* cb = anyptr_ex<Counter>::as(b.to<anyptr>());
        ASSERT_NE(cb, nullptr);
        EXPECT_EQ(cb->value, 5);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, type_check) {
    variant v(anyptr_ex<Apple>::make(new Apple));

    EXPECT_TRUE(v.is<anyptr>());
    EXPECT_NE(anyptr_ex<Apple>::as(v.to<anyptr>()), nullptr);
    EXPECT_EQ(anyptr_ex<Orange>::as(v.to<anyptr>()), nullptr);
}

TEST(gt_avarptr, is_vs_other_types) {
    variant v(anyptr_ex<Counter>::make(new Counter(1)));
    EXPECT_TRUE(v.is<anyptr>());
    EXPECT_FALSE(v.is<int_64>());
    EXPECT_FALSE(v.is<std::string>());
}

TEST(gt_avarptr, as_overwrite) {
    Counter::reset();
    variant v(anyptr_ex<Counter>::make(new Counter(1)));
    EXPECT_EQ(Counter::alive, 1);

    v.as<anyptr>() = anyptr_ex<Counter>::make(new Counter(2));
    EXPECT_EQ(Counter::alive, 1);

    auto* c = anyptr_ex<Counter>::as(v.to<anyptr>());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->value, 2);
}

TEST(gt_avarptr, vec_of_anyptr) {
    Counter::reset();
    {
        varvec vec;
        vec.push_back(variant(anyptr_ex<Counter>::make(new Counter(1))));
        vec.push_back(variant(anyptr_ex<Counter>::make(new Counter(2))));
        vec.push_back(variant(anyptr_ex<Counter>::make(new Counter(3))));
        EXPECT_EQ(Counter::alive, 3);

        EXPECT_EQ(anyptr_ex<Counter>::as(vec[0].to<anyptr>())->value, 1);
        EXPECT_EQ(anyptr_ex<Counter>::as(vec[1].to<anyptr>())->value, 2);
        EXPECT_EQ(anyptr_ex<Counter>::as(vec[2].to<anyptr>())->value, 3);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, vec_copy_preserves_anyptr) {
    Counter::reset();
    {
        varvec a;
        a.push_back(variant(anyptr_ex<Counter>::make(new Counter(10))));
        EXPECT_EQ(Counter::alive, 1);

        varvec b = a;
        EXPECT_EQ(Counter::alive, 2);

        auto* ca = anyptr_ex<Counter>::as(a[0].to<anyptr>());
        auto* cb = anyptr_ex<Counter>::as(b[0].to<anyptr>());
        ASSERT_NE(ca, nullptr);
        ASSERT_NE(cb, nullptr);
        EXPECT_NE(ca, cb);
        EXPECT_EQ(ca->value, 10);
        EXPECT_EQ(cb->value, 10);

        cb->value = 99;
        EXPECT_EQ(ca->value, 10);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, map_of_anyptr) {
    Counter::reset();
    {
        varmap m;
        m["a"] = variant(anyptr_ex<Counter>::make(new Counter(1)));
        m["b"] = variant(anyptr_ex<Counter>::make(new Counter(2)));
        EXPECT_EQ(Counter::alive, 2);

        EXPECT_EQ(anyptr_ex<Counter>::as(m.value("a").to<anyptr>())->value, 1);
        EXPECT_EQ(anyptr_ex<Counter>::as(m.value("b").to<anyptr>())->value, 2);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, lst_of_anyptr) {
    Counter::reset();
    {
        varlst lst;
        lst.push_back(variant(anyptr_ex<Counter>::make(new Counter(7))));
        lst.push_back(variant(anyptr_ex<Counter>::make(new Counter(8))));
        EXPECT_EQ(Counter::alive, 2);

        EXPECT_EQ(anyptr_ex<Counter>::as(lst.front().to<anyptr>())->value, 7);
        EXPECT_EQ(anyptr_ex<Counter>::as(lst.back().to<anyptr>())->value, 8);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, variant_self_copy_with_anyptr) {
    Counter::reset();
    variant v(anyptr_ex<Counter>::make(new Counter(42)));
    v = v;
    EXPECT_EQ(Counter::alive, 1);
    EXPECT_EQ(anyptr_ex<Counter>::as(v.to<anyptr>())->value, 42);
}

TEST(gt_avarptr, variant_self_move_with_anyptr) {
    Counter::reset();
    variant v(anyptr_ex<Counter>::make(new Counter(42)));
    v = std::move(v);
    EXPECT_EQ(Counter::alive, 1);
    EXPECT_EQ(anyptr_ex<Counter>::as(v.to<anyptr>())->value, 42);
}

TEST(gt_avarptr, nested_vec_of_anyptr) {
    Counter::reset();
    {
        varvec inner;
        inner.push_back(variant(anyptr_ex<Counter>::make(new Counter(5))));

        varvec outer;
        outer.push_back(variant(inner));
        EXPECT_EQ(Counter::alive, 2);

        varvec outer2 = outer;
        EXPECT_EQ(Counter::alive, 3);

        auto& inner2 = outer2[0].to_vec();
        auto* c = anyptr_ex<Counter>::as(inner2[0].to<anyptr>());
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(c->value, 5);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, make_ref_shares_object) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(10));
        EXPECT_FALSE(a.null());
        EXPECT_TRUE(a.is_ref());
        EXPECT_FALSE(a.shared());

        {
            anyptr b = a;
            EXPECT_TRUE(a.shared());
            EXPECT_TRUE(b.shared());
            EXPECT_EQ(Counter::alive, 1);

            auto* ca = anyptr_ex<Counter>::as(a);
            auto* cb = anyptr_ex<Counter>::as(b);
            EXPECT_EQ(ca, cb);
            EXPECT_EQ(ca->value, 10);
        }

        EXPECT_FALSE(a.shared());
        EXPECT_EQ(Counter::alive, 1);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, make_ref_multiple_shares) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(7));
        anyptr b = a;
        anyptr c = a;
        EXPECT_TRUE(a.shared());
        EXPECT_TRUE(b.shared());
        EXPECT_TRUE(c.shared());
        EXPECT_EQ(Counter::alive, 1);

        b = anyptr();
        EXPECT_TRUE(a.shared());
        EXPECT_EQ(Counter::alive, 1);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, make_ref_move_transfers) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(88));
        EXPECT_TRUE(a.is_ref());

        anyptr b = std::move(a);
        EXPECT_TRUE(a.null());
        EXPECT_FALSE(b.null());
        EXPECT_TRUE(b.is_ref());
        EXPECT_FALSE(b.shared());

        auto* c = anyptr_ex<Counter>::as(b);
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(c->value, 88);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, cpy_copy_creates_independent) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make(new Counter(5));
        EXPECT_FALSE(a.is_ref());

        anyptr b = a;
        EXPECT_EQ(Counter::alive, 2);

        auto* ca = anyptr_ex<Counter>::as(a);
        auto* cb = anyptr_ex<Counter>::as(b);
        EXPECT_NE(ca, cb);
        EXPECT_EQ(ca->value, 5);
        EXPECT_EQ(cb->value, 5);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, ref_copy_shares) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(5));
        EXPECT_TRUE(a.is_ref());

        anyptr b = a;
        EXPECT_EQ(Counter::alive, 1);

        auto* ca = anyptr_ex<Counter>::as(a);
        auto* cb = anyptr_ex<Counter>::as(b);
        EXPECT_EQ(ca, cb);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, as_works_for_ref_type) {
    anyptr a = anyptr_ex<Counter>::make_ref(new Counter(99));
    EXPECT_NE(anyptr_ex<Counter>::as(a), nullptr);
    EXPECT_EQ(anyptr_ex<Apple>::as(a), nullptr);
}

TEST(gt_avarptr, ref_release_last_dies) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(1));
        anyptr b = a;
        anyptr c = a;
        EXPECT_EQ(Counter::alive, 1);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, ref_move_then_original_dies) {
    Counter::reset();
    {
        anyptr a = anyptr_ex<Counter>::make_ref(new Counter(3));
        anyptr b = std::move(a);

        EXPECT_EQ(Counter::alive, 1);
    }
    EXPECT_EQ(Counter::alive, 0);
}

TEST(gt_avarptr, variant_stores_refowned) {
    Counter::reset();
    {
        variant v(anyptr_ex<Counter>::make_ref(new Counter(50)));
        EXPECT_TRUE(v.is<anyptr>());

        auto* c = anyptr_ex<Counter>::as(v.to<anyptr>());
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(c->value, 50);

        variant v2 = v;
        EXPECT_EQ(Counter::alive, 1);
        EXPECT_EQ(anyptr_ex<Counter>::as(v.to<anyptr>()),
                  anyptr_ex<Counter>::as(v2.to<anyptr>()));
    }
    EXPECT_EQ(Counter::alive, 0);
}
