/** ****************************************************************
 * \file   gt_ascript_types.cpp
 * \brief  Link type/desc/view entry points and the host query surface
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************/

#include "ascript.h"
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

#ifdef ALX_TEST_LINK_TYPED_PROBE_SO

namespace {

    const char* probe_link() { return ALX_TEST_LINK_TYPED_PROBE_SO; }

    // Load the fixture and leave a box handle in the root variable b
    bool seed_box(engine* _eng) {
        std::string src =
            std::string("link \"") + probe_link() + "\" as t; var b; b = t.make_box(); 0;";
        return _eng->exec(bytes_view(bytes(src)), "").error == error_type::NoError;
    }

    struct proxy_rec {
        bool saw_trap = false;
        std::string type;
        bool desc_ok = false;
        std::string desc;
        bool view_ok = false;
    };

    bool proxy_hook(hook_info& _info) {
        if (_info.type != hook_event::trap) return true;
        proxy_rec* r = static_cast<proxy_rec*>(_info.hkdt);
        r->saw_trap = true;
        const variant* v = _info.wken_data_cptr(_info.wken_root(), "b");
        if (!v) return true;
        r->type = _info.wktype(*v);
        if (!v->is<anyptr>()) return true;
        r->desc_ok = _info.wkdesc(v->to<anyptr>(), r->desc);
        variant out;
        r->view_ok = _info.wkview(v->to<anyptr>(), out);
        return true;
    }

    const char* host_namer(const variant&, void*) { return "host_name"; }

    std::string bytes_text(const bytes& _b) {
        return std::string(reinterpret_cast<const char*>(_b.data()), _b.size());
    }

}

TEST(gt_ascript_types, LinkNamesItsOwnHandle) {
    engine* eng = engine::create();
    ASSERT_TRUE(seed_box(eng)) << "fixture link failed to load";
    const variant* slot = eng->load("b");
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(eng->type(*slot), "probe_box");

    auto r = eng->exec(bytes_view(bytes("type(b);")), "");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_EQ(r.value.to<std::string>(), "probe_box") << "the script sees the same name";
    delete eng;
}

TEST(gt_ascript_types, DescAndViewComeFromTheClaimingLink) {
    engine* eng = engine::create();
    ASSERT_TRUE(seed_box(eng));
    const anyptr& h = eng->load("b")->to<anyptr>();

    std::string d;
    EXPECT_TRUE(eng->desc(h, d));
    EXPECT_EQ(d, "640x480");

    variant out;
    ASSERT_TRUE(eng->view(h, out));
    ASSERT_TRUE(out.is<varmap>());
    const varmap& m = out.to<varmap>();
    EXPECT_EQ(m.value("format").to<std::string>(""), "raw");
    EXPECT_EQ(bytes_text(m.value("data").to<bytes>()), "view-bytes");
    delete eng;
}

TEST(gt_ascript_types, UnclaimedHandleFallsThrough) {
    engine* eng = engine::create();
    ASSERT_TRUE(seed_box(eng));
    auto r = eng->exec(bytes_view(bytes("var n; n = t.make_int(); 0;")), "");
    ASSERT_EQ(r.error, error_type::NoError);

    const variant* slot = eng->load("n");
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(eng->type(*slot), "unknown");

    const anyptr& h = slot->to<anyptr>();
    std::string d;
    EXPECT_FALSE(eng->desc(h, d));
    variant out;
    EXPECT_FALSE(eng->view(h, out));
    delete eng;
}

TEST(gt_ascript_types, EmptyHandleReadsAsNull) {
    engine* eng = engine::create();
    anyptr empty;
    EXPECT_EQ(eng->type(variant(anyptr())), "null");
    std::string d;
    EXPECT_FALSE(eng->desc(empty, d));
    variant out;
    EXPECT_FALSE(eng->view(empty, out));
    delete eng;
}

TEST(gt_ascript_types, LinkNameBeatsTypeExtend) {
    engine* eng = engine::create();
    eng->set_type_extend(host_namer);
    ASSERT_TRUE(seed_box(eng));
    EXPECT_EQ(eng->type(*eng->load("b")), "probe_box");
    EXPECT_EQ(eng->type(variant(1.5f)), "host_name")
        << "the extend callback still names unmodeled values";
    delete eng;
}

TEST(gt_ascript_types, HookProxiesSeeTheSameAnswers) {
    engine* eng = engine::create();
    proxy_rec rec;
    eng->set_hook(proxy_hook, &rec, 0);
    std::string src =
        std::string("link \"") + probe_link() + "\" as t; var b; b = t.make_box(); trap();";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    ASSERT_EQ(r.error, error_type::NoError);
    EXPECT_TRUE(rec.saw_trap);
    EXPECT_EQ(rec.type, "probe_box");
    EXPECT_TRUE(rec.desc_ok);
    EXPECT_EQ(rec.desc, "640x480");
    EXPECT_TRUE(rec.view_ok);
    eng->set_hook(nullptr, nullptr, 0);
    delete eng;
}

TEST(gt_ascript_types, DuplicateRegistrationFailsTheLink) {
    engine* eng = engine::create();
    ASSERT_TRUE(seed_box(eng));
    std::string src =
        std::string("link \"") + ALX_TEST_LINK_TYPED_PROBE_DUP_SO + "\" as d; 0;";
    auto r = eng->exec(bytes_view(bytes(src)), "");
    EXPECT_EQ(r.error, error_type::RuntimeError);
    EXPECT_NE(r.value.to<std::string>().find("duplicate type id"), std::string::npos);
    EXPECT_EQ(eng->type(*eng->load("b")), "probe_box") << "the first registration stands";
    delete eng;
}

#endif
