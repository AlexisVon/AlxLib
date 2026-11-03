// Copyright (c) 2026 AlexisVon
#include "ascript.h"
#include "typed_probe_type.h"

using namespace alx;
using namespace alx::script;

extern "C" void alexis_script_load(fwrap& _fw) {
    // The first fixture already owns this type id: turn the refusal into a loud load failure
    if (!_fw.bind(anyptr_ex<alx_test::probe_box>::rid, "dup_box", nullptr, nullptr))
        _fw.raise(std::string("duplicate type id"));
}
