// Copyright (c) 2026 AlexisVon
#include "ascript.h"

using namespace alx;
using namespace alx::script;

extern "C" void alexis_script_load(fwrap& _fw) {
    // The call face belongs to a native; from an entry point it must throw LinkError
    _fw.freturn(variant(static_cast<int_64>(1)));
}
