/*****************************************************************/ /**
 * \file   gt_aregex_pcre2.cpp
 * \brief  Unit tests for regex_pcre2
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aregex_pcre2.h"

#include "aregex_ex.h"

#include <gtest/gtest.h>

#include <iostream>
#include <type_traits>

using namespace alx;

namespace {
    template <typename A, typename B>
    struct mirrors : std::false_type {};
    template <typename R, typename C1, typename C2, typename... A>
    struct mirrors<R (C1::*)(A...) const, R (C2::*)(A...) const> : std::true_type {};
}

#define MIRRORED(_ret, _name, ...)                                                                                      \
    static_assert(mirrors<decltype(static_cast<_ret (regex_ex::*)(__VA_ARGS__) const>(&regex_ex::_name)),               \
                          decltype(static_cast<_ret (regex_pcre2::*)(__VA_ARGS__) const>(&regex_pcre2::_name))>::value, \
                  #_name " drifted between regex_ex and regex_pcre2")

MIRRORED(bool, is_compliant, const char*, uint_64);
MIRRORED(std::string, find, const char*, uint_64);
MIRRORED(std::vector<std::string>, find_all, const char*, uint_64);
MIRRORED(std::vector<std::string>, match_group, const char*, uint_64);
MIRRORED(std::vector<std::string>, match_group_all, const char*, uint_64);
MIRRORED(std::string, replace, const std::string&, const std::string&);
MIRRORED(std::string, replace_all, const std::string&, const std::string&);

#undef MIRRORED

static_assert(mirrors<decltype(&regex_ex::is_valid), decltype(&regex_pcre2::is_valid)>::value, "is_valid drifted");
static_assert(mirrors<decltype(&regex_ex::get_pattern), decltype(&regex_pcre2::get_pattern)>::value,
              "get_pattern drifted");

TEST(gt_aregex_pcre2, is_compliant_matches_regex_ex) {
    regex_pcre2 pc("a+b");
    regex_ex st("a+b");
    ASSERT_TRUE(pc.is_valid());

    EXPECT_TRUE(pc.is_compliant("aaab"));
    EXPECT_FALSE(pc.is_compliant("aaac"));
    EXPECT_EQ(pc.is_compliant("aaab"), st.is_compliant("aaab"));
    EXPECT_EQ(pc.is_compliant("aaac"), st.is_compliant("aaac"));

    regex_pcre2 alt("a|ab");
    ASSERT_TRUE(alt.is_valid());
    EXPECT_TRUE(alt.is_compliant("ab"));
    EXPECT_FALSE(alt.is_compliant("abc"));
}

TEST(gt_aregex_pcre2, find_matches_regex_ex) {
    const char* patterns[] = {"[0-9]+", "a+b", "(\\d+)-(\\d+)", "^\\w+"};
    const char* subjects[] = {"ab123cd", "xaaaby", "12-345", "hello world", "no digits"};

    for (const char* p : patterns) {
        regex_pcre2 pc(p);
        regex_ex st(p);
        ASSERT_TRUE(pc.is_valid()) << p;
        for (const char* s : subjects) {
            EXPECT_EQ(pc.find(s), st.find(s)) << p << " / " << s;
        }
    }
}

TEST(gt_aregex_pcre2, find_all_matches_regex_ex) {
    regex_pcre2 pc("[0-9]+");
    regex_ex st("[0-9]+");
    EXPECT_EQ(pc.find_all("a1b22c333"), st.find_all("a1b22c333"));
    EXPECT_EQ(pc.find_all("no digits here"), st.find_all("no digits here"));
}

TEST(gt_aregex_pcre2, match_group_matches_regex_ex) {
    regex_pcre2 pc("(\\d+)-(\\d+)");
    regex_ex st("(\\d+)-(\\d+)");
    EXPECT_EQ(pc.match_group("x12-345y"), st.match_group("x12-345y"));
    EXPECT_EQ(pc.match_group("nothing"), st.match_group("nothing"));
    EXPECT_EQ(pc.match_group_all("1-2 34-56"), st.match_group_all("1-2 34-56"));

    regex_pcre2 unset("(a)|(b)");
    ASSERT_TRUE(unset.is_valid());
    std::vector<std::string> g = unset.match_group("b");
    ASSERT_EQ(g.size(), 3U);
    EXPECT_EQ(g[0], "b");
    EXPECT_EQ(g[1], "");
    EXPECT_EQ(g[2], "b");
    EXPECT_EQ(g, regex_ex("(a)|(b)").match_group("b"));
}

TEST(gt_aregex_pcre2, replace_matches_regex_ex) {
    regex_pcre2 pc("[0-9]+");
    regex_ex st("[0-9]+");
    EXPECT_EQ(pc.replace("a1b2", "#"), st.replace("a1b2", "#"));
    EXPECT_EQ(pc.replace_all("a1b2", "#"), st.replace_all("a1b2", "#"));

    regex_pcre2 grp("(\\d+)-(\\d+)");
    regex_ex grp_st("(\\d+)-(\\d+)");
    EXPECT_EQ(grp.replace_all("12-34", "$2:$1"), grp_st.replace_all("12-34", "$2:$1"));

    EXPECT_EQ(grp.replace("12-34", "[${1}|$0]"), "[12|12-34]");
}

TEST(gt_aregex_pcre2, empty_match_advance_measured_against_regex_ex) {

    const char* patterns[] = {"a*?", "a*", "\\d*", "(a)*"};
    const char* subjects[] = {"a", "a1", "aab", "b", ""};

    for (const char* p : patterns) {
        regex_pcre2 pc(p);
        regex_ex st(p);
        ASSERT_TRUE(pc.is_valid()) << p;
        for (const char* s : subjects) {
            std::vector<std::string> pc_all = pc.find_all(s);
            std::vector<std::string> st_all = st.find_all(s);
            std::cout << "[dialect] find_all(\"" << p << "\", \"" << s << "\"): pcre2={";
            for (const std::string& v : pc_all) std::cout << "\"" << v << "\" ";
            std::cout << "} std={";
            for (const std::string& v : st_all) std::cout << "\"" << v << "\" ";
            std::cout << "}" << std::endl;
            EXPECT_EQ(pc_all, st_all) << p << " / " << s;
        }
    }

    regex_pcre2 lazy("a*?");
    std::vector<std::string> all = lazy.find_all("a");
    ASSERT_EQ(all.size(), 3U);
    EXPECT_EQ(all[0], "");
    EXPECT_EQ(all[1], "a");
    EXPECT_EQ(all[2], "");
}

TEST(gt_aregex_pcre2, find_all_agrees_on_a_zero_width_match_behind_the_offset) {

    const char* patterns[] = {"\\bx*", "\\Bx*", "x*\\b", "(?=\\s)x*"};
    const char* subjects[] = {"ab c", "a b", "x y"};

    for (const char* p : patterns) {
        regex_pcre2 pc(p);
        regex_ex st(p);
        ASSERT_TRUE(pc.is_valid()) << p;
        ASSERT_TRUE(st.is_valid()) << p;
        for (const char* s : subjects) {
            EXPECT_EQ(pc.find_all(s), st.find_all(s)) << p << " / " << s;
            EXPECT_EQ(pc.match_group_all(s), st.match_group_all(s)) << p << " / " << s;
        }
    }
}

TEST(gt_aregex_pcre2, find_all_spans_is_the_same_scan_as_find_all) {

    const char* patterns[] = {"\\d+", "a*", "x*", "(\\d)"};
    const char* subjects[] = {"a12b34", "aaa", "abc", ""};

    for (const char* p : patterns) {
        regex_pcre2 re(p);
        ASSERT_TRUE(re.is_valid()) << p;
        for (const char* s : subjects) {
            std::vector<std::pair<uint_64, uint_64>> spans = re.find_all_spans(s);
            std::vector<std::string> texts = re.find_all(s);
            std::string subject(s);
            ASSERT_EQ(spans.size(), texts.size()) << p << " / " << s;
            for (size_t i = 0; i < spans.size(); ++i)
                EXPECT_EQ(texts[i], subject.substr(spans[i].first, spans[i].second)) << p << " / " << s;
        }
    }
}

TEST(gt_aregex_pcre2, find_all_spans_places_a_zero_width_match_after_the_cursor) {

    regex_pcre2 re("(?<=\\s)x*");
    ASSERT_TRUE(re.is_valid());

    std::vector<std::pair<uint_64, uint_64>> spans = re.find_all_spans("a x y");
    ASSERT_EQ(spans.size(), 2U);
    EXPECT_EQ(spans[0].first, 2U);
    EXPECT_EQ(spans[0].second, 1U);
    EXPECT_EQ(spans[1].first, 4U);
    EXPECT_EQ(spans[1].second, 0U);
    EXPECT_EQ(re.find_all("a x y"), (std::vector<std::string>{"x", ""}));
}

TEST(gt_aregex_pcre2, find_all_spans_reports_why_the_scan_stopped) {

    regex_pcre2 re("(a+)+$");
    ASSERT_TRUE(re.is_valid());
    re.set_step_limit(1000);

    EXPECT_TRUE(re.find_all_spans(std::string(30, 'a') + "!").empty());
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::limit);
    EXPECT_EQ(re.last_error_code(), -47);
}

TEST(gt_aregex_pcre2, replacement_dialect_differs_from_regex_ex) {

    regex_pcre2 pc("(a)");
    regex_ex st("(a)");
    ASSERT_TRUE(pc.is_valid());

    std::cout << "[dialect] replace(\"a\", \"$12\"): pcre2=\"" << pc.replace("a", "$12") << "\" std=\""
              << st.replace("a", "$12") << "\"" << std::endl;
    EXPECT_EQ(pc.replace("a", "$12"), "a");
    EXPECT_TRUE(pc.last_status() == regex_pcre2::match_status::error);
    EXPECT_EQ(pc.replace("a", "${1}2"), "a2");

    EXPECT_EQ(pc.replace("a", "[$0|$_]"), "[a|a]");

    regex_pcre2 unset("(a)|(b)");
    ASSERT_TRUE(unset.is_valid());
    EXPECT_EQ(unset.replace("b", "<$1>"), "<>");
}

TEST(gt_aregex_pcre2, catastrophic_pattern_is_pruned_before_it_backtracks) {

    regex_pcre2 re("(a+)+b");
    ASSERT_TRUE(re.is_valid());
    re.set_step_limit(1000);

    EXPECT_EQ(re.find(std::string(30, 'a')), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::no_match);
}

TEST(gt_aregex_pcre2, step_limit_bounds_what_the_prune_cannot_reach) {

    regex_pcre2 re("^(a+)+$");
    ASSERT_TRUE(re.is_valid());
    re.set_step_limit(100000);

    const std::string subject = std::string(30, 'a') + "!";
    EXPECT_EQ(re.find(subject), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::limit);
    EXPECT_EQ(re.last_error_code(), -47);

    re.set_step_limit(0);
    EXPECT_EQ(re.find("aaa"), "aaa");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::ok);
    EXPECT_EQ(re.find(subject), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::limit);
}

namespace {
    bool count_down(void* _ud) {
        int* budget = static_cast<int*>(_ud);
        return --(*budget) > 0;
    }
}

TEST(gt_aregex_pcre2, callout_interrupts_a_running_match) {
    regex_pcre2 re("^(a+)+$");
    ASSERT_TRUE(re.is_valid());
    re.set_step_limit(0);

    int budget = 1000;
    re.set_callout(count_down, &budget);
    EXPECT_EQ(re.find(std::string(30, 'a') + "!"), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::interrupted);
    EXPECT_EQ(re.last_error_code(), -37);
    EXPECT_LE(budget, 0);

    int plenty = 1000000;
    re.set_callout(count_down, &plenty);
    EXPECT_EQ(re.find("aaa"), "aaa");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::ok);

    re.set_callout(nullptr, nullptr);
    EXPECT_EQ(re.find("aaa"), "aaa");
    budget = 0;
    EXPECT_EQ(re.find(std::string(30, 'a') + "!"), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::limit);
}

TEST(gt_aregex_pcre2, no_match_is_not_an_abort) {
    regex_pcre2 re("[0-9]+");
    ASSERT_TRUE(re.is_valid());
    EXPECT_EQ(re.find("no digits"), "");
    EXPECT_TRUE(re.last_status() == regex_pcre2::match_status::no_match);
    EXPECT_EQ(re.last_error_code(), -1);
}

TEST(gt_aregex_pcre2, invalid_pattern) {
    regex_pcre2 bad("(");
    EXPECT_FALSE(bad.is_valid());
    EXPECT_EQ(bad.find("anything"), "");
    EXPECT_TRUE(bad.last_status() == regex_pcre2::match_status::error);
    EXPECT_EQ(bad.replace("x", "y"), "x");
}

TEST(gt_aregex_pcre2, one_instance_is_one_matcher) {
    static_assert(!std::is_copy_constructible_v<regex_pcre2> && !std::is_copy_assignable_v<regex_pcre2>,
                  "regex_pcre2 must not become copyable: an instance owns its hook and its match block");
    static_assert(std::is_move_constructible_v<regex_pcre2> && std::is_move_assignable_v<regex_pcre2>,
                  "regex_pcre2 must stay movable");

    const std::string subject = std::string(30, 'a') + "!";
    regex_pcre2 hooked("^(a+)+$");
    regex_pcre2 plain("^(a+)+$");
    ASSERT_TRUE(hooked.is_valid());
    ASSERT_TRUE(plain.is_valid());
    plain.set_step_limit(100000);

    int budget = 1000;
    hooked.set_callout(count_down, &budget);
    EXPECT_EQ(hooked.find(subject), "");
    EXPECT_TRUE(hooked.last_status() == regex_pcre2::match_status::interrupted);
    EXPECT_EQ(plain.find(subject), "");
    EXPECT_TRUE(plain.last_status() == regex_pcre2::match_status::limit);
    EXPECT_EQ(plain.last_error_code(), -47);

    budget = 1000;
    regex_pcre2 moved(std::move(hooked));
    EXPECT_EQ(moved.get_pattern(), "^(a+)+$");
    EXPECT_EQ(moved.find(subject), "");
    EXPECT_TRUE(moved.last_status() == regex_pcre2::match_status::interrupted);
    EXPECT_FALSE(hooked.is_valid());
}

TEST(gt_aregex_pcre2, move_keeps_the_compiled_pattern) {

    regex_pcre2 a("(\\d+)");
    ASSERT_TRUE(a.is_valid());
    regex_pcre2 b(std::move(a));
    EXPECT_TRUE(b.is_valid());
    EXPECT_EQ(b.find("x42y"), "42");
    EXPECT_FALSE(a.is_valid());

    regex_pcre2 c("zzz");
    c = std::move(b);
    EXPECT_EQ(c.find("x42y"), "42");
    EXPECT_EQ(c.get_pattern(), "(\\d+)");
}
