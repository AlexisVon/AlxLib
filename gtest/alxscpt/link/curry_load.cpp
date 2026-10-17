// Copyright (c) 2026 AlexisVon
#include "ascript.h"

// The load entry has no engine to name: minting a delivery handle must refuse with LinkError.
extern "C" void alexis_script_load(alx::script::fwrap& _fw) {
    _fw.currying("target");
}
