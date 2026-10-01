/*****************************************************************/ /**
 * \file   ascript_compile.h
 * \brief  Script compile pipeline — import resolve, module embed, binary output
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_COMPILE_H_
#define _ALEXIS_SCRIPT_COMPILE_H_

#include "abytes.h"
#include "ascript.h"
#include "avariant.h"
#include <list>
#include <string>
#include <unordered_map>

namespace alx {
    namespace script {

        struct compile_result {
            // Module key ("@" + 16 hex of path+content hash) -> varmap {path, resolved, ast}
            std::unordered_map<std::string, variant> modules;
            // Paths nothing resolved: a non-empty list marks the product not self-contained
            std::list<std::string> imports;
            std::list<std::string> links;
        };

        // Top level of _ast only, no resolution: each O_IMPORT/O_LINK path lands as written
        void collect_deps(const varvec& _ast, compile_result& _out);

        // An empty _file, no modules and an empty _hint leave their outer keys out entirely
        bytes make_compile_binary(const std::string& _file, const varvec& _ast,
                                  const std::string& _etype, uint_64 _vtype,
                                  const compile_result& _result, bool _cmps,
                                  const bytes_view& _hint = {});

        // Re-verifies info.sha256 and caps info.size at 512MB before inflating anything
        bool decompress_inner(const varmap& _outer, varvec& _out_ast,
                              varmap& _out_modules);

        // Outer header only: the inner payload is never decompressed here
        bool get_dependencies(const bytes_view& _data, compile_result& _out);

        bool unpack(const bytes_view& _data, varmap& _out);

        // _eng is a fallback only: plain source is lexed and parsed here first
        std::string prtast(const bytes_view& _data, class engine* _eng = nullptr);
        std::string prtinf(const bytes_view& _data);
        bytes prtfmt(const bytes_view& _data);

    }
}

#endif
