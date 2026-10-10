// Copyright (c) 2026 AlexisVon

#include "ascript.h"
#include "astring.h"
using namespace alx;
using namespace alx::script;

static void fn_set_value(fwrap& args) {
    if (args.size() < 2 || !args[0].is<std::string>()) {
        args.freturn();
        return;
    }
    std::string key = args[0].to<std::string>();
    args.store(key, args[1]);
    args.freturn(args[1]);
}

static void fn_get_value(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn();
        return;
    }
    std::string key = args[0].to<std::string>();
    variant* pv = args.load(key);
    if (pv) args.freturn(*pv);
    else args.freturn(variant(false));
}

static void fn_has_value(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn(variant(false));
        return;
    }
    std::string key = args[0].to<std::string>();
    variant* pv = args.load(key);
    args.freturn(variant(pv != nullptr));
}

static void fn_del_value(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn(variant(false));
        return;
    }
    std::string key = args[0].to<std::string>();
    args.freturn(variant(args.remove(key)));
}

static void fn_read_parent(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn();
        return;
    }
    std::string name = args[0].to<std::string>();
    variant* v = args.iload(name);
    if (v) args.freturn(*v);
    else args.freturn(variant(false));
}

static void fn_call_script(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.freturn();
        return;
    }
    std::string fname = args[0].to<std::string>();
    varvec call_args;
    for (size_t i = 1; i < args.size(); i++) call_args.push_back(args[i]);
    variant ret = args.call(fname, call_args);
    args.freturn(ret);
}

static void fn_raise_error(fwrap& args) {
    std::string msg = (args.size() > 0 && args[0].is<std::string>())
                          ? args[0].to<std::string>()
                          : "error from link";
    args.raise(variant(msg), error_type::RuntimeError);
}

#include <stdexcept>
static void fn_throw_native(fwrap& args) {
    std::string msg = (args.size() > 0 && args[0].is<std::string>())
                          ? args[0].to<std::string>()
                          : "native exception";
    throw std::runtime_error(msg);
}

static void fn_echo(fwrap& args) {
    args.freturn(args.size() > 0 ? args[0] : variant());
}

static void fn_write_arg(fwrap& args) {
    args[0] = variant(static_cast<int_64>(999));
    args.freturn(args[0]);
}

extern "C" void alexis_script_load(fwrap& args) {
    args.bind("set", fn_set_value);
    args.bind("get", fn_get_value);
    args.bind("has", fn_has_value);
    args.bind("del", fn_del_value);
    args.bind("read_parent", fn_read_parent);
    args.bind("echo", fn_echo, "sub");
    args.bind("write_arg", fn_write_arg);
    args.bind("call_script", fn_call_script);
    args.bind("raise_error", fn_raise_error);
    args.bind("throw_native", fn_throw_native);
}

extern "C" void alexis_script_create(fwrap& args) {
    args.bind("set", fn_set_value);
    args.bind("get", fn_get_value);
    args.bind("has", fn_has_value);
    args.bind("del", fn_del_value);
    args.bind("read_parent", fn_read_parent);
    args.bind("echo", fn_echo, "sub");
    args.bind("write_arg", fn_write_arg);
    args.bind("call_script", fn_call_script);
    args.bind("raise_error", fn_raise_error);
    args.bind("throw_native", fn_throw_native);
}
