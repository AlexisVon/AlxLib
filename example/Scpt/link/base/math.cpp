// Copyright (c) 2026 AlexisVon

#include "ascript.h"
#include <cmath>

using namespace alx;
using namespace alx::script;

namespace {
    inline bool is_num(const variant& v) { return v.is<int_64>() || v.is<double>(); }
    inline double as_double(const variant& v) {
        return v.is<double>() ? v.to<double>() : static_cast<double>(v.to<int_64>());
    }
    inline double arg0_d(const fwrap& args) { return as_double(args[0]); }
    inline double arg1_d(const fwrap& args) { return as_double(args[1]); }

    inline void check_1arg(fwrap& args, const char* name) {
        if (args.size() == 0 || !is_num(args[0]))
            args.raise(variant(std::string(name) + " expects a number"), error_type::ArgError);
    }
    inline void check_2arg(fwrap& args, const char* name) {
        if (args.size() < 2 || !is_num(args[0]) || !is_num(args[1]))
            args.raise(variant(std::string(name) + " expects two numbers"), error_type::ArgError);
    }
}

static void fn_abs(fwrap& args) {
    check_1arg(args, "abs");
    if (args[0].is<int_64>()) {
        int_64 v = args[0].to<int_64>();
        args.freturn(variant(v < 0 ? -v : v));
    } else {
        args.freturn(variant(std::abs(args[0].to<double>())));
    }
}

static void fn_sqrt(fwrap& args) {
    check_1arg(args, "sqrt");
    args.freturn(variant(std::sqrt(arg0_d(args))));
}

static void fn_pow(fwrap& args) {
    check_2arg(args, "pow");
    args.freturn(variant(std::pow(arg0_d(args), arg1_d(args))));
}

static void fn_min(fwrap& args) {
    check_2arg(args, "min");
    if (args[0].is<int_64>() && args[1].is<int_64>()) {
        int_64 a = args[0].to<int_64>(), b = args[1].to<int_64>();
        args.freturn(variant(a < b ? a : b));
    } else {
        double a = arg0_d(args), b = arg1_d(args);
        args.freturn(variant(a < b ? a : b));
    }
}

static void fn_max(fwrap& args) {
    check_2arg(args, "max");
    if (args[0].is<int_64>() && args[1].is<int_64>()) {
        int_64 a = args[0].to<int_64>(), b = args[1].to<int_64>();
        args.freturn(variant(a > b ? a : b));
    } else {
        double a = arg0_d(args), b = arg1_d(args);
        args.freturn(variant(a > b ? a : b));
    }
}

static void fn_ceil(fwrap& args) {
    check_1arg(args, "ceil");
    args.freturn(variant(static_cast<int_64>(std::ceil(arg0_d(args)))));
}

static void fn_floor(fwrap& args) {
    check_1arg(args, "floor");
    args.freturn(variant(static_cast<int_64>(std::floor(arg0_d(args)))));
}

static void fn_round(fwrap& args) {
    check_1arg(args, "round");
    args.freturn(variant(static_cast<int_64>(std::round(arg0_d(args)))));
}

static void fn_sin(fwrap& args) {
    check_1arg(args, "sin");
    args.freturn(variant(std::sin(arg0_d(args))));
}

static void fn_cos(fwrap& args) {
    check_1arg(args, "cos");
    args.freturn(variant(std::cos(arg0_d(args))));
}

static void fn_tan(fwrap& args) {
    check_1arg(args, "tan");
    args.freturn(variant(std::tan(arg0_d(args))));
}

static void fn_log(fwrap& args) {
    check_1arg(args, "log");
    args.freturn(variant(std::log(arg0_d(args))));
}

static void fn_log2(fwrap& args) {
    check_1arg(args, "log2");
    args.freturn(variant(std::log2(arg0_d(args))));
}

static void fn_log10(fwrap& args) {
    check_1arg(args, "log10");
    args.freturn(variant(std::log10(arg0_d(args))));
}

static void fn_exp(fwrap& args) {
    check_1arg(args, "exp");
    args.freturn(variant(std::exp(arg0_d(args))));
}

static void fn_pi(fwrap& args) {
    args.freturn(variant(3.14159265358979323846));
}

static void fn_e(fwrap& args) {
    args.freturn(variant(2.71828182845904523536));
}

void register_mt(fwrap& args) {
    args.bind("abs", fn_abs, "mt");
    args.bind("sqrt", fn_sqrt, "mt");
    args.bind("pow", fn_pow, "mt");
    args.bind("min", fn_min, "mt");
    args.bind("max", fn_max, "mt");
    args.bind("ceil", fn_ceil, "mt");
    args.bind("floor", fn_floor, "mt");
    args.bind("round", fn_round, "mt");
    args.bind("sin", fn_sin, "mt");
    args.bind("cos", fn_cos, "mt");
    args.bind("tan", fn_tan, "mt");
    args.bind("log", fn_log, "mt");
    args.bind("log2", fn_log2, "mt");
    args.bind("log10", fn_log10, "mt");
    args.bind("exp", fn_exp, "mt");
    args.bind("pi", fn_pi, "mt");
    args.bind("e", fn_e, "mt");
}
