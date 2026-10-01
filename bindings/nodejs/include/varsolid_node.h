/*****************************************************************/ /**
 * \file   varsolid_node.h
 * \brief  V8-direct serialization/deserialization from binary (skips varmap)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_VARSOLID_NODE_H_
#define _ALEXIS_VARSOLID_NODE_H_

#include "avarsolid.h"
#include "global_node.h"

namespace alx {
    namespace varsolid_node {

        Local<Object> to_object(const bytes_view& _bytes, Isolate* _isolate);
        Local<Value> to_value(const bytes_view& _bytes, Isolate* _isolate);
        Local<Value> get_value(const bytes_view& _bytes, const std::list<std::string>& _path, Isolate* _isolate, const Local<Value>& _def = Local<Value>());

        bytes to_bytes(Local<Object> _obj, Isolate* _isolate);
    }
}

#endif
