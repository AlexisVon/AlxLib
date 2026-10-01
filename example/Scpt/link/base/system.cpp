// Copyright (c) 2026 AlexisVon

#include "aplatform.h"
#include "ascript.h"

using namespace alx;
using namespace alx::script;

static void fn_exec(fwrap& args) {
    if (args.size() == 0 || !args[0].is<std::string>()) {
        args.raise(variant(std::string("exec: expected command string")), error_type::ArgError);
        return;
    }
    std::string cmd = args[0].to<std::string>();
    uint_32 timeout = (args.size() > 1 && args[1].is<int_64>()) ? static_cast<uint_32>(args[1].to<int_64>()) : 1000;

    exec_result r = exec_sync(cmd, timeout);

    varmap result;
    result["code"] = variant(static_cast<int_64>(r.excode));
    result["output"] = variant(r.output);
    result["success"] = variant(r.success);
    args.freturn(variant(std::move(result)));
}

static void fn_sleep(fwrap& args) {
    if (args.size() == 0 || !args[0].is<int_64>()) {
        args.raise(variant(std::string("sleep: expected milliseconds")), error_type::ArgError);
        return;
    }
    high_precision_sleep(static_cast<uint_64>(args[0].to<int_64>()));
}

void register_sys(fwrap& args) {
    args.bind("exec", fn_exec, "sys");
    args.bind("sleep", fn_sleep, "sys");
}
