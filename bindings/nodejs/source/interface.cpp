/*****************************************************************/ /**
 * \file   interface.cpp
 * \brief  Node.js addon — module interface
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "interface.h"
#include "abytes.h"
#include "args_wrap.h"
#include "avarsolid.h"
#include "v8-local-handle.h"
#include "varsolid_node.h"

void init(Local<Object> _exports, Local<Value> _module, void* _priv) {
    REG_FUNCTION(version, _exports);
    REG_FUNCTION(encode, _exports);
    REG_FUNCTION(decode, _exports);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init);

void version(args_wrap _args) {
    _args.SetReturn(std::string("v" ALXVAR_VERSION ", build: " __TIME__ " " __DATE__));
}

void encode(args_wrap _args) {
    if (!_args.isObject(0)) return _args.exception("encode(Object)", args_wrap::TypeError);
    _args.SetReturn(varsolid_node::to_bytes(_args.getObject(0), _args.getIsolate()));
}

void decode(args_wrap _args) {
    if (!_args.isArrayBuffer(0)) return _args.exception("decode(Uint8Array)", args_wrap::TypeError);
    if (_args.size() == 1)
        return _args.SetReturn(varsolid_node::to_object(_args.getArrayBuffer(0), _args.getIsolate()));
    else {
        std::list<std::string> keys;
        for (int i = 1; i < _args.size(); i++)
            if (!_args.isString(i)) return _args.exception("decode(Uint8Array, String...)", args_wrap::TypeError);
            else keys.push_back(_args.getString(i));
        return _args.SetReturn(varsolid_node::get_value(_args.getArrayBuffer(0), keys, _args.getIsolate()));
    }
}