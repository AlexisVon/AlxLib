// Copyright (c) 2026 AlexisVon
#include "ascript.h"
#include "typed_probe_type.h"

using namespace alx;
using namespace alx::script;

namespace {

    void fn_make_box(fwrap& _fw) {
        _fw.freturn(variant(anyptr_ex<alx_test::probe_box>::make(new alx_test::probe_box())));
    }

    void fn_make_int(fwrap& _fw) {
        _fw.freturn(variant(anyptr_ex<int_64>::make(new int_64(7))));
    }

    std::string fn_desc(const anyptr& _h) {
        const alx_test::probe_box* b = anyptr_ex<alx_test::probe_box>::as(_h);
        return b ? std::to_string(b->w) + "x" + std::to_string(b->h) : std::string();
    }

    variant fn_view(const anyptr& _h) {
        if (!anyptr_ex<alx_test::probe_box>::as(_h)) return variant();
        varmap m;
        m["format"] = variant(std::string("raw"));
        m["data"] = variant(bytes("view-bytes"));
        return variant(std::move(m));
    }

}

extern "C" void alexis_script_load(fwrap& _fw) {
    _fw.bind("make_box", fn_make_box);
    _fw.bind("make_int", fn_make_int);
    _fw.bind(anyptr_ex<alx_test::probe_box>::rid, "probe_box", fn_desc, fn_view);
}
