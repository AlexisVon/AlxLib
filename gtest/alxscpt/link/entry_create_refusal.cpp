// Copyright (c) 2026 AlexisVon
#include "ascript.h"

using namespace alx;
using namespace alx::script;

extern "C" void alexis_script_create(fwrap& _fw) {
    // Type registration belongs to the load entry; a create entry must be refused with LinkError
    if (_fw.bind(anyptr_ex<int_64>::rid, "runtime_type", nullptr, nullptr))
        _fw.raise(std::string("type bind accepted outside the load entry"));
}
