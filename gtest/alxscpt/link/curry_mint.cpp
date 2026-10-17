// Copyright (c) 2026 AlexisVon
#include "ascript.h"

using namespace alx;
using namespace alx::script;

namespace {

    // Mint a handle and drop it at once: its destructor must not touch anything engine-owned.
    void fn_mintdrop(fwrap& _fw) {
        delete _fw.currying("target");
        _fw.freturn(variant(static_cast<int_64>(1)));
    }

}

extern "C" void alexis_script_load(fwrap& _fw) {
    _fw.bind("mintdrop", fn_mintdrop);
}

extern "C" void alexis_script_create(fwrap& _fw) {
    _fw.bind("mintdrop", fn_mintdrop);
}
