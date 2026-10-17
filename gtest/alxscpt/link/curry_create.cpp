// Copyright (c) 2026 AlexisVon
#include "ascript.h"

// The create entry has no engine either: minting a delivery handle must refuse with LinkError.
extern "C" void alexis_script_create(alx::script::fwrap& _fw) {
    _fw.currying("target");
}
