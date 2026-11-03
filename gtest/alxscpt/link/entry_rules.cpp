// Copyright (c) 2026 AlexisVon
#include "ascript.h"
#include <atomic>

using namespace alx;
using namespace alx::script;

namespace {

    std::atomic<int> g_alive{0};

    struct counted {
        counted() { ++g_alive; }
        ~counted() { --g_alive; }
    };

    void fn_ok(fwrap& _fw) {
        _fw.freturn(variant(static_cast<int_64>(1)));
    }

    void fn_alive(fwrap& _fw) {
        _fw.freturn(variant(static_cast<int_64>(g_alive.load())));
    }

    void fn_typebind(fwrap& _fw) {
        // Must throw LinkError: type registration belongs to the load entry
        _fw.bind(anyptr_ex<int_64>::rid, "runtime_type", nullptr, nullptr);
    }

}

extern "C" void alexis_script_load(fwrap& _fw) {
    // Plain scope: a duplicate is a refusal
    if (!_fw.bind("once", fn_ok)) _fw.raise(std::string("first bind failed"));
    if (_fw.bind("once", fn_ok)) _fw.raise(std::string("duplicate bind accepted"));

    // Single method into an area: created empty when absent, refused on a duplicate
    if (!_fw.bind("get", fn_ok, "mt")) _fw.raise(std::string("area bind failed"));
    if (_fw.bind("get", fn_ok, "mt")) _fw.raise(std::string("duplicate method accepted"));

    // Area with an object: create only, and the handle is consumed either way
    if (!_fw.wrap<int_64>(new int_64(5), "box", {{"get", fn_ok}}))
        _fw.raise(std::string("wrap failed"));
    if (_fw.wrap<int_64>(new int_64(6), "box", {}))
        _fw.raise(std::string("wrap over a taken area accepted"));

    // The explicit object form, and its refusal on a taken name
    if (!_fw.bind("box2", anyptr_ex<int_64>::make(new int_64(11)), {{"get", fn_ok}}))
        _fw.raise(std::string("bind(area, object, methods) failed"));
    if (_fw.bind("box2", anyptr_ex<int_64>::make(new int_64(12)), {}))
        _fw.raise(std::string("bind(area, object, methods) over a taken area accepted"));

    // An empty area name is refused as well
    if (_fw.wrap<int_64>(new int_64(8), "", {{"m", fn_ok}}))
        _fw.raise(std::string("empty area accepted"));

    // A plain value under the area name is a refusal, not a forced conversion
    _fw.store("plain", variant(static_cast<int_64>(9)));
    if (_fw.bind("m2", fn_ok, "plain"))
        _fw.raise(std::string("bind into a plain value accepted"));
    variant* p = _fw.load("plain");
    if (!p || !p->is<int_64>() || p->to<int_64>() != 9)
        _fw.raise(std::string("a refused bind changed the stored value"));

    // Ownership: a successful wrap keeps the object, a refused one consumes (deletes) it
    if (!_fw.wrap<counted>(new counted(), "kept", {}))
        _fw.raise(std::string("wrap of a counted object failed"));
    if (_fw.wrap<counted>(new counted(), "kept", {}))
        _fw.raise(std::string("wrap over a taken area accepted (counted)"));
    if (!_fw.bind("alive", fn_alive)) _fw.raise(std::string("alive bind failed"));
    if (!_fw.bind("typebind", fn_typebind)) _fw.raise(std::string("typebind bind failed"));
}
