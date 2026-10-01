// Copyright (c) 2026 AlexisVon

#include "ascript.h"
#include "astring.h"
#include <algorithm>
#include <cctype>

using namespace alx;
using namespace alx::script;

namespace {
    inline std::string arg0_s(fwrap& args) {
        return (args.size() > 0 && args[0].is<std::string>()) ? args[0].to<std::string>() : std::string();
    }
}

static void fn_trim(fwrap& args) {
    std::string s = arg0_s(args);
    size_t l = 0, r = s.size();
    while (l < r && std::isspace(static_cast<unsigned char>(s[l]))) l++;
    while (l < r && std::isspace(static_cast<unsigned char>(s[r - 1]))) r--;
    args.freturn(variant(s.substr(l, r - l)));
}

static void fn_upper(fwrap& args) {
    std::string s = arg0_s(args);
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    args.freturn(variant(std::move(s)));
}

static void fn_lower(fwrap& args) {
    std::string s = arg0_s(args);
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    args.freturn(variant(std::move(s)));
}

static void fn_replace(fwrap& args) {
    if (args.size() < 3 || !args[0].is<std::string>() || !args[1].is<std::string>() || !args[2].is<std::string>()) {
        args.freturn();
        return;
    }
    std::string s = args[0].to<std::string>();
    const std::string& from = args[1].to<std::string>();
    const std::string& to = args[2].to<std::string>();
    if (from.empty()) {
        args.freturn(variant(s));
        return;
    }
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    args.freturn(variant(std::move(s)));
}

static void fn_startswith(fwrap& args) {
    if (args.size() < 2 || !args[0].is<std::string>() || !args[1].is<std::string>()) {
        args.freturn(variant(false));
        return;
    }
    const std::string& s = args[0].to<std::string>();
    const std::string& pfx = args[1].to<std::string>();
    args.freturn(variant(s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0));
}

static void fn_endswith(fwrap& args) {
    if (args.size() < 2 || !args[0].is<std::string>() || !args[1].is<std::string>()) {
        args.freturn(variant(false));
        return;
    }
    const std::string& s = args[0].to<std::string>();
    const std::string& sfx = args[1].to<std::string>();
    args.freturn(variant(s.size() >= sfx.size() && s.compare(s.size() - sfx.size(), sfx.size(), sfx) == 0));
}

static void fn_repeat(fwrap& args) {
    if (args.size() < 2 || !args[0].is<std::string>() || !args[1].is<int_64>()) {
        args.freturn();
        return;
    }
    const std::string& s = args[0].to<std::string>();
    int_64 n = args[1].to<int_64>();
    if (n <= 0) {
        args.freturn(variant(std::string()));
        return;
    }
    std::string r;
    r.reserve(s.size() * static_cast<size_t>(n));
    for (int_64 i = 0; i < n; i++) r += s;
    args.freturn(variant(std::move(r)));
}

static void fn_split(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn(varvec());
        return;
    }
    const std::string& src = args[0].to<std::string>();
    std::string delim = (args.size() > 1 && args[1].is<std::string>()) ? args[1].to<std::string>() : "";
    varvec result;
    if (delim.empty()) {
        for (char c : src) result.push_back(variant(std::string(1, c)));
    } else {
        auto parts = strutil::split(src, delim);
        for (auto& p : parts) result.push_back(variant(std::move(p)));
    }
    args.freturn(std::move(result));
}

static void fn_join(fwrap& args) {
    if (args.size() == 0) {
        args.freturn();
        return;
    }
    if (!args[0].is<varvec>() && !args[0].is<varlst>()) {
        args.raise(variant(std::string("join: expected vec or lst")), error_type::TypeError);
        return;
    }
    std::string d = (args.size() > 1 && args[1].is<std::string>()) ? args[1].to<std::string>() : "";
    std::string result;
    size_t idx = 0;
    auto append = [&](const variant& v) {
        if (idx++ > 0) result += d;
        if (v.is<std::string>()) result += v.to<std::string>();
        else if (v.is<int_64>()) result += std::to_string(v.to<int_64>());
        else if (v.is<double>()) result += std::to_string(v.to<double>());
        else if (v.is<bool>()) result += v.to<bool>() ? "true" : "false";
    };
    if (args[0].is<varvec>()) {
        for (auto& e : args[0].to<varvec>()) append(e);
    } else {
        for (auto& e : args[0].to<varlst>()) append(e);
    }
    args.freturn(result);
}

void register_st(fwrap& args) {
    args.bind("trim", fn_trim, "st");
    args.bind("upper", fn_upper, "st");
    args.bind("lower", fn_lower, "st");
    args.bind("replace", fn_replace, "st");
    args.bind("startswith", fn_startswith, "st");
    args.bind("endswith", fn_endswith, "st");
    args.bind("repeat", fn_repeat, "st");
    args.bind("split", fn_split, "st");
    args.bind("join", fn_join, "st");
}
