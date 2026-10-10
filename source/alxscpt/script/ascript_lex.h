/*****************************************************************/ /**
 * \file   ascript_lex.h
 * \brief  Script lexer — hand-crafted state machine
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_LEX_H_
#define _ALEXIS_SCRIPT_LEX_H_

#include "abase.h"
#include "abytes.h"
#include "ascript.h"
#include "ascript_enum.h"
#include <functional>
#include <string>
#include <vector>

namespace alx {
    namespace script {

        // Reserved words of the lexer's keyword table: control keywords and type words (int, vec, ...).
        bool is_reserved_name(const std::string& _name);

        struct token_meta {
            tk_enum type;
            uint_32 ofst;
            uint_32 size;
            // 1-based; col is not a UTF-8 character column (only the error reports compute one).
            uint_32 row;
            uint_32 col;

            // String and char literals: offset of each '\' from the token start; the parser, not the lexer, decodes them.
            std::vector<uint_32> esc;
        };

        class token_list {
        public:
            token_list() = default;
            explicit token_list(const bytes_view& _source) { tokenize(_source); }

            // Returns false when any T_ERROR token was emitted; those tokens stay in the list and _on_error has seen each.
            bool tokenize(const bytes_view& _source,
                          const alx::signal<const compile_error&>* _on_error = nullptr,
                          const std::string& _file_path = std::string());

            // Only text() is bounds-checked (an out-of-range _i gives an empty view); the rest index m_meta unchecked.
            size_t count() const { return m_meta.size(); }
            tk_enum type(size_t _i) const { return m_meta[_i].type; }
            bytes_view text(size_t _i) const;
            uint_32 row(size_t _i) const { return m_meta[_i].row; }
            uint_32 col(size_t _i) const { return m_meta[_i].col; }
            uint_32 ofst(size_t _i) const { return m_meta[_i].ofst; }
            uint_32 size(size_t _i) const { return m_meta[_i].size; }
            const std::vector<uint_32>& esc(size_t _i) const { return m_meta[_i].esc; }

            const bytes_view& source() const { return m_source; }

            // Appends meta only -- text() still reads through the source set by tokenize(), empty for a never-tokenized list.
            void push_back(tk_enum _t, uint_32 _ofst, uint_32 _size, uint_32 _row, uint_32 _col,
                           std::vector<uint_32> _esc = {}) {
                m_meta.push_back(token_meta{_t, _ofst, _size, _row, _col, std::move(_esc)});
            }

        private:
            bytes_view m_source;
            std::vector<token_meta> m_meta;
        };

    }
}

#endif
