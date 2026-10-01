/*****************************************************************/ /**
 * \file   ascript_lex.cpp
 * \brief  Script lexer — hand-crafted DFA implementation
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_lex.h"
#include "astring.h"
#include <unordered_map>

namespace alx {
    namespace script {

        // Reserved words: is_reserved_name() and the host-side set_extend/set_define gate on this table.
        static const std::unordered_map<std::string, tk_enum> s_keywords = {
            {"var", T_VAR},
            {"def", T_DEF},
            {"if", T_IF},
            {"else", T_ELSE},
            {"while", T_WHILE},
            {"for", T_FOR},
            {"break", T_BREAK},
            {"continue", T_CONTINUE},
            {"return", T_RETURN},
            {"import", T_IMPORT},
            {"link", T_LINK},
            {"as", T_AS},
            {"try", T_TRY},
            {"catch", T_CATCH},
            {"delete", T_DELETE},
            {"throw", T_THROW},
            {"switch", T_SWITCH},
            {"case", T_CASE},
            {"default", T_DEFAULT},
            {"true", T_TRUE},
            {"false", T_FALSE},
            {"null", T_NULL_},
            {"nan", T_NAN},
            {"inf", T_INF},
            {"int", T_INT},
            {"float", T_FLOAT},
            {"string", T_STRING},
            {"bool", T_BOOL},
            {"bytes", T_BYTES},
            {"vec", T_VEC},
            {"map", T_MAP},
            {"lst", T_LST},
            {"type", T_TYPE},
            {"env", T_ENV},
            {"here", T_HERE},
            {"trap", T_TRAP},
            {"eval", T_EVAL},
        };

        bool is_reserved_name(const std::string& _name) {
            return s_keywords.find(_name) != s_keywords.end();
        }

        static uint_32 utf8_col(const uint_8* data, uint_32 line_start, uint_32 index) {
            uint_32 col = 1;
            for (uint_32 i = line_start; i < index; i++) {
                if ((data[i] & 0xC0) != 0x80) col++;
            }
            return col;
        }

        // out[i] = byte offset of line i+1 (row-1 indexing); a trailing '\n' adds a start equal to size.
        static void build_line_starts(const uint_8* data, uint_32 size,
                                      std::vector<uint_32>& out) {
            out.clear();
            out.push_back(0);
            for (uint_32 i = 0; i < size; i++) {
                if (data[i] == '\n') out.push_back(i + 1);
            }
        }

        inline bool is_digit(uint_8 c) { return c >= '0' && c <= '9'; }
        inline bool is_oct_digit(uint_8 c) { return c >= '0' && c <= '7'; }
        inline bool is_bin_digit(uint_8 c) { return c == '0' || c == '1'; }
        inline bool is_hex_digit(uint_8 c) {
            return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }
        inline bool is_alpha(uint_8 c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }
        inline bool is_alnum(uint_8 c) { return is_alpha(c) || is_digit(c); }
        inline bool is_space(uint_8 c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

        bytes_view token_list::text(size_t _i) const {
            if (_i >= m_meta.size()) return bytes_view();
            return m_source.mid(m_meta[_i].ofst, m_meta[_i].size);
        }

        bool token_list::tokenize(const bytes_view& _source,
                                  const alx::signal<const compile_error&>* _on_error,
                                  const std::string& _file_path) {
            // m_source is a view: the caller's buffer must outlive this list and every text() view.
            m_source = _source;
            m_meta.clear();

            const uint_8* data = reinterpret_cast<const uint_8*>(_source.data());
            uint_32 size = static_cast<uint_32>(_source.size());
            uint_32 pos = 0;
            uint_32 row = 1;
            uint_32 col = 1;
            // An error never stops the scan: it is reported, left in the stream as T_ERROR, and ok flags it.
            bool ok = true;

            auto emit = [&](tk_enum tt, uint_32 start, uint_32 len, uint_32 r, uint_32 c,
                            std::vector<uint_32> esc = {}) {
                m_meta.push_back(token_meta{tt, start, len, r, c, std::move(esc)});
            };

            uint_32 bom_skip = (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ? 3 : 0;
            pos = bom_skip;

            std::vector<uint_32> line_starts;
            build_line_starts(data, size, line_starts);
            // Transparent to line 1: its columns count from after the BOM.
            if (bom_skip) line_starts[0] = bom_skip;

            // Diagnostic columns: 1-based codepoints, recomputed here because the scan-loop col counts bytes.
            auto real_col = [&](uint_32 index, uint_32 r) -> uint_32 {
                uint_32 ls = (r <= line_starts.size()) ? line_starts[r - 1] : index;
                return utf8_col(data, ls, index);
            };

            // Error recovery: consume through the next ';' so the parser resumes on a statement boundary.
            auto skip_to_sync = [&]() {
                while (pos < size && data[pos] != ';' && data[pos] != '\n') {

                    if ((data[pos] & 0xC0) != 0x80) col++;
                    pos++;
                }
                if (pos < size && data[pos] == ';') {
                    pos++;
                    col++;
                }
            };

            while (pos < size) {
                uint_8 c = data[pos];

                if (is_space(c)) {
                    while (pos < size && is_space(data[pos])) {
                        if (data[pos] == '\n') {
                            row++;
                            col = 1;
                        } else {
                            col++;
                        }
                        pos++;
                    }

                    continue;
                }

                if (c == '/' && pos + 1 < size && data[pos + 1] == '/') {
                    uint_32 start = pos;
                    uint_32 start_r = row;
                    uint_32 start_c = col;
                    pos += 2;
                    col += 2;
                    while (pos < size && data[pos] != '\n') {
                        pos++;
                        col++;
                    }
                    emit(T_COMMENT, start, static_cast<uint_32>(pos - start), start_r, start_c);
                    continue;
                }

                if (c == '/' && pos + 1 < size && data[pos + 1] == '*') {
                    uint_32 start = pos;
                    uint_32 start_r = row;
                    uint_32 start_c = col;
                    pos += 2;
                    col += 2;
                    bool closed = false;
                    while (pos + 1 < size) {
                        if (data[pos] == '*' && data[pos + 1] == '/') {
                            pos += 2;
                            col += 2;
                            closed = true;
                            break;
                        }
                        if (data[pos] == '\n') {
                            row++;
                            col = 1;
                        } else {
                            col++;
                        }
                        pos++;
                    }
                    if (!closed) {
                        if (_on_error)
                            _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "unterminated block comment"});
                        emit(T_ERROR, start, static_cast<uint_32>(pos - start), start_r, start_c);
                        ok = false;
                        skip_to_sync();
                    } else {
                        emit(T_COMMENT, start, static_cast<uint_32>(pos - start), start_r, start_c);
                    }
                    continue;
                }

                uint_32 start = pos;
                uint_32 start_r = row;
                uint_32 start_c = col;

                if (c == '"') {
                    pos++;
                    col++;
                    std::vector<uint_32> esc;
                    while (pos < size && data[pos] != '"' && data[pos] != '\n') {
                        if (data[pos] == '\\') {

                            // Offsets are relative to the token start; the parser decodes and validates them.
                            esc.push_back(pos - start);
                            if (pos + 1 < size) {
                                pos++;
                                col++;
                            }
                        }
                        col++;
                        pos++;
                    }
                    if (pos < size && data[pos] == '"') {
                        pos++;
                        col++;
                        emit(T_STRING_LITERAL, start, pos - start, start_r, start_c, std::move(esc));
                    } else {

                        if (_on_error)
                            _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "unterminated string literal"});
                        emit(T_ERROR, start, pos - start, start_r, start_c);
                        ok = false;
                        skip_to_sync();
                    }
                    continue;
                }

                if (c == '`') {
                    pos++;
                    col++;
                    // Raw body: no escapes recorded, newlines allowed; the parser takes it verbatim.
                    while (pos < size && data[pos] != '`') {
                        if (data[pos] == '\n') {
                            row++;
                            col = 1;
                        } else {
                            col++;
                        }
                        pos++;
                    }
                    if (pos < size && data[pos] == '`') {
                        pos++;
                        col++;
                        emit(T_BACKTICK_STRING, start, pos - start, start_r, start_c);
                    } else {
                        if (_on_error)
                            _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "unterminated backtick string"});
                        emit(T_ERROR, start, pos - start, start_r, start_c);
                        ok = false;
                        skip_to_sync();
                    }
                    continue;
                }

                if (c == '\'') {
                    pos++;
                    col++;
                    std::vector<uint_32> esc;
                    while (pos < size && data[pos] != '\'' && data[pos] != '\n') {
                        if (data[pos] == '\\') {
                            esc.push_back(pos - start);
                            if (pos + 1 < size) {
                                pos++;
                                col++;
                            }
                        }
                        col++;
                        pos++;
                    }
                    if (pos < size && data[pos] == '\'') {
                        pos++;
                        col++;
                        emit(T_STRING_LITERAL, start, pos - start, start_r, start_c, std::move(esc));
                    } else {
                        if (_on_error)
                            _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "unterminated string literal"});
                        emit(T_ERROR, start, pos - start, start_r, start_c);
                        ok = false;
                        skip_to_sync();
                    }
                    continue;
                }

                if (is_digit(c)) {
                    bool is_float = false;

                    // 0x / 0o / 0b return early: no fraction or exponent may follow them.
                    if (c == '0' && pos + 1 < size) {
                        uint_8 next = data[pos + 1];
                        if (next == 'x' || next == 'X') {
                            pos += 2;
                            col += 2;
                            uint_32 digit_start = pos;
                            while (pos < size && is_hex_digit(data[pos])) {
                                pos++;
                                col++;
                            }
                            if (pos == digit_start) {
                                if (_on_error)
                                    _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "expected digits after '0x'"});
                                emit(T_ERROR, start, pos - start, start_r, start_c);
                                ok = false;
                                continue;
                            }
                            emit(T_HEX_LITERAL, start, pos - start, start_r, start_c);
                            continue;
                        }
                        if (next == 'o' || next == 'O') {
                            pos += 2;
                            col += 2;
                            uint_32 digit_start = pos;
                            while (pos < size && is_oct_digit(data[pos])) {
                                pos++;
                                col++;
                            }
                            if (pos == digit_start) {
                                if (_on_error)
                                    _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "expected digits after '0o'"});
                                emit(T_ERROR, start, pos - start, start_r, start_c);
                                ok = false;
                                continue;
                            }
                            emit(T_OCT_LITERAL, start, pos - start, start_r, start_c);
                            continue;
                        }
                        if (next == 'b' || next == 'B') {
                            pos += 2;
                            col += 2;
                            uint_32 digit_start = pos;
                            while (pos < size && is_bin_digit(data[pos])) {
                                pos++;
                                col++;
                            }
                            if (pos == digit_start) {
                                if (_on_error)
                                    _on_error->exec({{_file_path, start_r, real_col(start, start_r), start}, "expected digits after '0b'"});
                                emit(T_ERROR, start, pos - start, start_r, start_c);
                                ok = false;
                                continue;
                            }
                            emit(T_BIN_LITERAL, start, pos - start, start_r, start_c);
                            continue;
                        }
                    }

                    while (pos < size && is_digit(data[pos])) {
                        pos++;
                        col++;
                    }

                    // The '.' joins the literal only when a digit follows: "1.foo" is INT DOT NAME.
                    if (pos < size && data[pos] == '.') {
                        uint_32 peek = pos + 1;
                        if (peek < size && is_digit(data[peek])) {
                            is_float = true;
                            pos++;
                            col++;
                            while (pos < size && is_digit(data[pos])) {
                                pos++;
                                col++;
                            }
                        }
                    }

                    if (pos < size && (data[pos] == 'e' || data[pos] == 'E')) {
                        uint_32 peek = pos + 1;
                        if (peek < size && (data[peek] == '+' || data[peek] == '-')) peek++;
                        if (peek < size && is_digit(data[peek])) {
                            is_float = true;
                            pos++;
                            col++;
                            if (pos < size && (data[pos] == '+' || data[pos] == '-')) {
                                pos++;
                                col++;
                            }
                            while (pos < size && is_digit(data[pos])) {
                                pos++;
                                col++;
                            }
                        }
                    }

                    emit(is_float ? T_FLOAT_LITERAL : T_INT_LITERAL,
                         start, pos - start, start_r, start_c);
                    continue;
                }

                if (is_alpha(c)) {
                    pos++;
                    col++;
                    while (pos < size && is_alnum(data[pos])) {
                        pos++;
                        col++;
                    }
                    std::string name(reinterpret_cast<const char*>(data + start), pos - start);

                    auto it = s_keywords.find(name);
                    if (it != s_keywords.end()) {
                        emit(it->second, start, pos - start, start_r, start_c);
                    }

                    else {
                        emit(T_NAME, start, pos - start, start_r, start_c);
                    }
                    continue;
                }

                pos++;
                col++;

                switch (c) {

                case '+':
                    if (pos < size && data[pos] == '+') {
                        pos++;
                        col++;
                        emit(T_AUTO_INC, start, 2, start_r, start_c);
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_ADD, start, 2, start_r, start_c);
                    } else {
                        emit(T_PLUS, start, 1, start_r, start_c);
                    }
                    break;

                case '-':
                    if (pos < size && data[pos] == '-') {
                        pos++;
                        col++;
                        emit(T_AUTO_DEC, start, 2, start_r, start_c);
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_MINUS, start, 2, start_r, start_c);
                    } else {
                        emit(T_MINUS, start, 1, start_r, start_c);
                    }
                    break;

                case '*':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_MUL, start, 2, start_r, start_c);
                    } else {
                        emit(T_STAR, start, 1, start_r, start_c);
                    }
                    break;

                case '/':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_DIV, start, 2, start_r, start_c);
                    } else {
                        emit(T_SLASH, start, 1, start_r, start_c);
                    }
                    break;

                case '%':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_MOD, start, 2, start_r, start_c);
                    } else {
                        emit(T_PERCENT, start, 1, start_r, start_c);
                    }
                    break;

                case '<':
                    if (pos < size && data[pos] == '<') {
                        pos++;
                        col++;
                        if (pos < size && data[pos] == '=') {
                            pos++;
                            col++;
                            emit(T_ASS_LSHIFT, start, 3, start_r, start_c);
                        } else {
                            emit(T_LSHIFT, start, 2, start_r, start_c);
                        }
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_LE, start, 2, start_r, start_c);
                    } else {
                        emit(T_LT, start, 1, start_r, start_c);
                    }
                    break;

                case '>':
                    if (pos < size && data[pos] == '>') {
                        pos++;
                        col++;
                        if (pos < size && data[pos] == '=') {
                            pos++;
                            col++;
                            emit(T_ASS_RSHIFT, start, 3, start_r, start_c);
                        } else {
                            emit(T_RSHIFT, start, 2, start_r, start_c);
                        }
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_GE, start, 2, start_r, start_c);
                    } else {
                        emit(T_GT, start, 1, start_r, start_c);
                    }
                    break;

                case '=':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_EQ, start, 2, start_r, start_c);
                    } else {
                        emit(T_ASS, start, 1, start_r, start_c);
                    }
                    break;

                case '!':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_NE, start, 2, start_r, start_c);
                    } else {
                        emit(T_NOT, start, 1, start_r, start_c);
                    }
                    break;

                case '&':
                    if (pos < size && data[pos] == '&') {
                        pos++;
                        col++;
                        emit(T_AND, start, 2, start_r, start_c);
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_BIT_AND, start, 2, start_r, start_c);
                    } else {
                        emit(T_BIT_AND, start, 1, start_r, start_c);
                    }
                    break;

                case '|':
                    if (pos < size && data[pos] == '|') {
                        pos++;
                        col++;
                        emit(T_OR, start, 2, start_r, start_c);
                    } else if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_BIT_OR, start, 2, start_r, start_c);
                    } else {
                        emit(T_BIT_OR, start, 1, start_r, start_c);
                    }
                    break;

                case '^':
                    if (pos < size && data[pos] == '=') {
                        pos++;
                        col++;
                        emit(T_ASS_BIT_XOR, start, 2, start_r, start_c);
                    } else {
                        emit(T_BIT_XOR, start, 1, start_r, start_c);
                    }
                    break;

                case '~': emit(T_BIT_NEG, start, 1, start_r, start_c); break;
                case '?': emit(T_QUESTION, start, 1, start_r, start_c); break;
                case ':':
                    if (pos < size && data[pos] == ':') {
                        pos++;
                        col++;
                        emit(T_COLONCOLON, start, 2, start_r, start_c);
                    } else {
                        emit(T_COLON, start, 1, start_r, start_c);
                    }
                    break;
                case ',': emit(T_COMMA, start, 1, start_r, start_c); break;
                case '.':
                    if (pos < size && data[pos] == '.') {

                        uint_32 dot_start = pos - 1;
                        uint_32 dot_count = 2;
                        pos++;
                        col++;
                        while (pos < size && data[pos] == '.') {
                            pos++;
                            col++;
                            dot_count++;
                        }
                        // Even run: dot_count/2 T_DOTDOT tokens, each of length 2 and anchored at the first dot.
                        if (dot_count % 2 == 0) {

                            for (uint_32 i = 0; i < dot_count / 2; i++)
                                emit(T_DOTDOT, dot_start, 2, start_r, start_c);
                        } else {

                            if (_on_error)
                                _on_error->exec({{_file_path, start_r, real_col(dot_start, start_r), dot_start},
                                                 "odd number of dots is illegal"});
                            emit(T_ERROR, dot_start, dot_count, start_r, start_c);
                            ok = false;
                        }
                    } else {
                        emit(T_DOT, start, 1, start_r, start_c);
                    }
                    break;
                case '@': emit(T_AT, start, 1, start_r, start_c); break;
                case '$': emit(T_DOLLAR, start, 1, start_r, start_c); break;

                case ';': emit(T_SEMICOLON, start, 1, start_r, start_c); break;
                case '(': emit(T_LP, start, 1, start_r, start_c); break;
                case ')': emit(T_RP, start, 1, start_r, start_c); break;
                case '{': emit(T_LC, start, 1, start_r, start_c); break;
                case '}': emit(T_RC, start, 1, start_r, start_c); break;
                case '[': emit(T_LB, start, 1, start_r, start_c); break;
                case ']': emit(T_RB, start, 1, start_r, start_c); break;

                default:
                    if (_on_error) {
                        _on_error->exec({{_file_path, start_r, real_col(start, start_r), start},
                                         strutil::format("invalid character: '%1'", {std::string(1, static_cast<char>(c))})});
                    }
                    emit(T_ERROR, start, 1, start_r, start_c);
                    ok = false;
                    skip_to_sync();
                    break;
                }
            }

            // Zero-length final token; the parser peeks it as the end-of-input sentinel.
            emit(T_END_OF_FILE, pos, 0, row, col);
            return ok;
        }

    }
}
