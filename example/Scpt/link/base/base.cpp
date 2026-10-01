// Copyright (c) 2026 AlexisVon

#include "ascript.h"

using namespace alx::script;

void register_mt(fwrap&);
void register_ex(fwrap&);
void register_st(fwrap&);
void register_sys(fwrap&);
void register_fs(fwrap&);

extern "C" void alexis_script_load(fwrap& args) {
    register_mt(args);
    register_ex(args);
    register_st(args);
    register_sys(args);
    register_fs(args);
}

