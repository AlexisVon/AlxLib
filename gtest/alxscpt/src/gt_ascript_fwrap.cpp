/** ****************************************************************
 * \file   gt_ascript_fwrap.cpp
 * \brief  Per-context fwrap rules: refusals and registration returns
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

#ifdef ALX_TEST_LINK_ENTRY_REFUSAL_SO
TEST(gt_ascript_fwrap, CallFaceRefusedInLoadEntry) {
    engine* eng = engine::create();
    std::string src = std::string("link \"") + ALX_TEST_LINK_ENTRY_REFUSAL_SO + "\" as t; 0;";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    EXPECT_EQ(r.error, error_type::ContextError);
    EXPECT_NE(r.value.to<std::string>().find("freturn not available"), std::string::npos);
    delete eng;
}
#endif

#ifdef ALX_TEST_LINK_ENTRY_CREATE_REFUSAL_SO
TEST(gt_ascript_fwrap, TypeBindRefusedInCreateEntry) {
    engine* eng = engine::create();
    std::string src = std::string("link \"") + ALX_TEST_LINK_ENTRY_CREATE_REFUSAL_SO + "\" as t; 0;";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    EXPECT_EQ(r.error, error_type::ContextError);
    EXPECT_NE(r.value.to<std::string>().find("type bind only available"), std::string::npos);
    delete eng;
}
#endif

#ifdef ALX_TEST_LINK_ENTRY_RULES_SO
TEST(gt_ascript_fwrap, DuplicateBindAndWrapRefuse) {
    engine* eng = engine::create();
    std::string src = std::string("link \"") + ALX_TEST_LINK_ENTRY_RULES_SO +
                      "\" as t; t.once() + t.box.get() + t.box2.get() + t.alive();";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    ASSERT_EQ(r.error, error_type::NoError) << r.value.to<std::string>();
    // once + box.get + box2.get + alive (the refused wrap's object was consumed)
    EXPECT_EQ(r.value.to<int_64>(), 4);
    delete eng;
}

TEST(gt_ascript_fwrap, TypeBindRefusedInNative) {
    engine* eng = engine::create();
    std::string src =
        std::string("link \"") + ALX_TEST_LINK_ENTRY_RULES_SO + "\" as t; t.typebind();";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    EXPECT_EQ(r.error, error_type::ContextError);
    EXPECT_NE(r.value.to<std::string>().find("type bind only available"), std::string::npos);
    delete eng;
}
#endif
