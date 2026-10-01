/*****************************************************************/ /**
 * \file   ascript_trace.h
 * \brief  Call-stack trace compression — repeated-segment folding
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_TRACE_H_
#define _ALEXIS_SCRIPT_TRACE_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace alx {
    namespace script {

        // A repeat node is the one with children: one period, emitted `repeat` times, text kept empty
        struct trace_item {
            std::string text;
            size_t repeat;
            std::vector<trace_item> children;
            trace_item() : repeat(1) {}
            trace_item(std::string t, size_t r, std::vector<trace_item> c)
                : text(std::move(t)), repeat(r), children(std::move(c)) {}
        };

        // Folds, never drops: expanding the tree reproduces _seq -- same frames, same order
        std::vector<trace_item> compress_trace(const std::vector<std::string>& _seq);

        // Appends to _out; _indent is prefixed to every line emitted at this level
        void format_trace(const std::vector<trace_item>& _items, std::string& _out,
                          const std::string& _indent);

    }
}

#endif
