// Copyright (c) 2026 AlexisVon

#include "ascript.h"
#include "astring.h"
#include <algorithm>

using namespace alx;
using namespace alx::script;

static void fn_find(fwrap& args) {
    if (args.size() == 0) {
        args.freturn();
        return;
    }
    if (args[0].is<varvec>()) {
        const varvec& v = args[0].to<varvec>();
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i].is<int_64>() && args.size() > 1 && args[1].is<int_64>() && v[i].to<int_64>() == args[1].to<int_64>()) {
                args.freturn(static_cast<int_64>(i));
                return;
            }
            if (v[i].is<double>() && args.size() > 1 && args[1].is<double>() && v[i].to<double>() == args[1].to<double>()) {
                args.freturn(static_cast<int_64>(i));
                return;
            }
            if (v[i].is<std::string>() && args.size() > 1 && args[1].is<std::string>() && v[i].to<std::string>() == args[1].to<std::string>()) {
                args.freturn(static_cast<int_64>(i));
                return;
            }
            if (v[i].is<bool>() && args.size() > 1 && args[1].is<bool>() && v[i].to<bool>() == args[1].to<bool>()) {
                args.freturn(static_cast<int_64>(i));
                return;
            }
        }
        args.freturn(static_cast<int_64>(-1));
    } else if (args[0].is<std::string>()) {
        if (args.size() < 2 || !args[1].is<std::string>()) {
            args.freturn(static_cast<int_64>(-1));
            return;
        }
        const std::string& src = args[0].to<std::string>();
        const std::string& sub = args[1].to<std::string>();
        size_t pos = src.find(sub);
        args.freturn(static_cast<int_64>(pos == std::string::npos ? -1 : static_cast<int_64>(pos)));
    } else {
        args.freturn(static_cast<int_64>(-1));
    }
}

static void fn_keys(fwrap& args) {
    if (args.size() == 0 || !args[0].is<varmap>()) {
        args.freturn();
        return;
    }
    const varmap& m = args[0].to<varmap>();
    varvec result;
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        result.push_back(variant(it.key()));
    args.freturn(std::move(result));
}

static void fn_values(fwrap& args) {
    if (args.size() == 0 || !args[0].is<varmap>()) {
        args.freturn();
        return;
    }
    const varmap& m = args[0].to<varmap>();
    varvec result;
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        result.push_back(it.value());
    args.freturn(std::move(result));
}

static void fn_has(fwrap& args) {
    if (args.size() == 0 || !args[0].is<varmap>()) {
        args.freturn(false);
        return;
    }
    const varmap& m = args[0].to<varmap>();
    if (args.size() < 2 || !args[1].is<std::string>()) {
        args.freturn(false);
        return;
    }
    args.freturn(m.contain(args[1].to<std::string>()));
}

static void fn_size(fwrap& args) {
    if (args.size() == 0) {
        args.freturn();
        return;
    }
    if (args[0].is<varmap>()) {
        const varmap& m = args[0].to<varmap>();
        int_64 count = 0;
        for (auto it = m.cbegin(); it != m.cend(); ++it) count++;
        args.freturn(count);
    } else if (args[0].is<varvec>()) {
        args.freturn(static_cast<int_64>(args[0].to<varvec>().size()));
    } else if (args[0].is<varlst>()) {
        args.freturn(static_cast<int_64>(args[0].to<varlst>().size()));
    } else if (args[0].is<std::string>()) {
        args.freturn(static_cast<int_64>(args[0].to<std::string>().size()));
    } else {
        args.freturn();
    }
}

static void fn_insert(fwrap& args) {
    if (args[0].is<varvec>()) {
        varvec& vec = args[0].as<varvec>();
        if (args.size() < 3 || !args[1].is<int_64>()) {
            args.freturn(vec);
            return;
        }
        int_64 p = args[1].to<int_64>();
        if (p < 0) p = 0;
        if (p > static_cast<int_64>(vec.size())) p = static_cast<int_64>(vec.size());
        variant val = args[2];
        vec.insert(vec.begin() + static_cast<size_t>(p), val);
        args.freturn(vec);
    } else if (args[0].is<varmap>()) {
        varmap& m = args[0].as<varmap>();
        if (args.size() < 3 || !args[1].is<std::string>()) {
            args.freturn(m);
            return;
        }
        std::string key = args[1].to<std::string>();
        m[key] = args[2];
        args.freturn(m);
    } else {
        args.freturn();
    }
}

static void fn_remove_at(fwrap& args) {
    if (!args[0].is<varvec>()) {
        args.freturn();
        return;
    }
    varvec& vec = args[0].as<varvec>();
    if (args.size() < 2 || !args[1].is<int_64>()) {
        args.freturn(vec);
        return;
    }
    int_64 idx = args[1].to<int_64>();
    if (idx < 0 || idx >= static_cast<int_64>(vec.size())) {
        args.freturn(vec);
        return;
    }
    vec.erase(vec.begin() + static_cast<size_t>(idx));
    args.freturn(vec);
}

static void fn_sort(fwrap& args) {
    if (!args[0].is<varvec>()) {
        args.freturn();
        return;
    }
    varvec& vec = args[0].as<varvec>();
    std::sort(vec.begin(), vec.end(), [](const variant& x, const variant& y) {
        if (x.is<int_64>() && y.is<int_64>()) return x.to<int_64>() < y.to<int_64>();
        if (x.is<double>() && y.is<double>()) return x.to<double>() < y.to<double>();
        if (x.is<std::string>() && y.is<std::string>()) return x.to<std::string>() < y.to<std::string>();
        return false;
    });
    args.freturn(vec);
}

static void fn_reverse(fwrap& args) {
    if (!args[0].is<varvec>()) {
        args.freturn();
        return;
    }
    varvec& vec = args[0].as<varvec>();
    std::reverse(vec.begin(), vec.end());
    args.freturn(vec);
}

static void fn_merge(fwrap& args) {
    variant other = args[1];

    if (args[0].is<varmap>()) {
        if (!other.is<varmap>()) {
            args.raise(variant(std::string("merge: map requires map argument")), error_type::TypeError);
            return;
        }
        varmap& m = args[0].as<varmap>();
        const varmap& s = other.to<varmap>();
        for (auto it = s.cbegin(); it != s.cend(); ++it)
            m[it.key()] = it.value();
        args.freturn(m);
    } else if (args[0].is<varvec>()) {
        varvec& vec = args[0].as<varvec>();
        if (other.is<varvec>()) {
            const varvec& o = other.to<varvec>();
            vec.insert(vec.end(), o.begin(), o.end());
        } else if (other.is<varlst>()) {
            for (auto& e : other.to<varlst>()) vec.push_back(std::move(e));
        } else {
            args.raise(variant(std::string("merge: vec requires vec/lst argument")), error_type::TypeError);
            return;
        }
        args.freturn(vec);
    } else if (args[0].is<varlst>()) {
        varlst& lst = args[0].as<varlst>();
        if (other.is<varlst>()) {
            for (auto& e : other.to<varlst>()) lst.push_back(std::move(e));
        } else if (other.is<varvec>()) {
            for (auto& e : other.to<varvec>()) lst.push_back(e);
        } else {
            args.raise(variant(std::string("merge: lst requires vec/lst argument")), error_type::TypeError);
            return;
        }
        args.freturn(lst);
    } else {
        args.raise(variant(std::string("merge: unsupported type")), error_type::TypeError);
    }
}

void register_ex(fwrap& args) {

    args.bind("find", fn_find, "ex");
    args.bind("keys", fn_keys, "ex");
    args.bind("values", fn_values, "ex");
    args.bind("has", fn_has, "ex");
    args.bind("size", fn_size, "ex");

    args.bind("insert", fn_insert, "ex");
    args.bind("remove", fn_remove_at, "ex");
    args.bind("sort", fn_sort, "ex");
    args.bind("reverse", fn_reverse, "ex");
    args.bind("merge", fn_merge, "ex");
}
