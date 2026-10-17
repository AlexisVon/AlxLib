// Copyright (c) 2026 AlexisVon
#include "ascript.h"
#include <stdexcept>

// The load entry fails with a C++ exception: the interpreter must report it as NativeError.
extern "C" void alexis_script_load(alx::script::fwrap& _fw) {
    (void) _fw;
    throw std::runtime_error("load boom");
}
