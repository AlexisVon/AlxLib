/*****************************************************************/ /**
 * \file   ascript_compile.cpp
 * \brief  Script compile pipeline — import resolve, module embed, binary output
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_compile.h"
#include "acompress.h"
#include "afile.h"
#include "ascript_lex.h"
#include "ascript_parse.h"
#include "ascript_utils.h"
#include "avarsolid.h"
#include "averify.h"
#include <chrono>
#include <unordered_set>

namespace alx {
    namespace script {

        void collect_deps(const varvec& _ast, compile_result& _out) {
            for (auto& s : _ast) {
                if (!s.is_vec()) continue;
                const auto& n = s.to<varvec>();
                if (n.empty() || !n[0].is<OPTYPE>()) continue;
                op_enum head = static_cast<op_enum>(n[0].to<OPTYPE>());
                if (n.size() > 1) {
                    if (head == O_IMPORT)
                        _out.imports.push_back(n[1].to<std::string>());
                    else if (head == O_LINK)
                        _out.links.push_back(n[1].to<std::string>());
                }
            }
        }

        // 512 MB cap on "size": not covered by sha256, and it must fit the int_32 lz4 decode below
        static constexpr uint_64 k_max_axp_inner = 512 * 1024 * 1024;

        bool decompress_inner(const varmap& _outer, varvec& _out_ast,
                              varmap& _out_modules) {
            variant av = _outer.value("ast");
            if (!av.is<bytes>()) return false;
            const bytes& stored = av.to<bytes>();

            variant iv = _outer.value("info");
            if (!iv.is<varmap>()) return false;
            const varmap& info = iv.to<varmap>();

            // the hash covers the stored bytes as written, the compressed form included
            if (info.value("sha256").to<std::string>() != verify::exec(verify::SHA_256, stored))
                return false;

            bool compressed = info.value("compressed").to<bool>();
            uint_64 size = info.value("size").to<uint_64>(0);
            if (size > k_max_axp_inner) return false;

            bytes inner;
            if (compressed) {
                inner = compress::decoder_lz4::sexec(stored, static_cast<int_32>(size));
                if (inner.empty() || inner.size() != size) return false;
            } else {
                inner = stored;
            }
            varmap inner_map;
            if (!varsolid::to_varmap(bytes_view(inner), inner_map)) return false;
            variant a = inner_map.value("::");
            if (!a.is_vec()) return false;
            _out_ast = a.to<varvec>();
            variant mods = inner_map.value("modules");
            if (mods.is<varmap>()) _out_modules = mods.to<varmap>();
            return true;
        }

        bytes make_compile_binary(const std::string& _file, const varvec& _ast,
                                  const std::string& _etype, uint_64 _vtype,
                                  const compile_result& _result, bool _cmps,
                                  const bytes_view& _hint) {
            varmap outer;
            outer["etype"] = variant(_etype);
            outer["vtype"] = variant(_vtype);
            if (!_file.empty()) {
                file_info fi(_file);
                outer["file"] = variant(fi.path());
                outer["name"] = variant(fi.name());
            }
            if (!_result.imports.empty()) {
                varvec iv;
                for (auto& p : _result.imports) iv.push_back(variant(p));
                outer["imports"] = variant(std::move(iv));
            }
            if (!_result.links.empty()) {
                varvec lv;
                for (auto& p : _result.links) lv.push_back(variant(p));
                outer["links"] = variant(std::move(lv));
            }
            auto now = std::chrono::system_clock::now();
            outer["time"] = variant(static_cast<int_64>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch())
                    .count()));

            varmap inner;
            // "::" is the program root key of the inner map; modules sit beside it
            inner["::"] = variant(_ast);
            if (!_result.modules.empty()) {
                varmap mods;
                for (auto& kv : _result.modules) mods[kv.first] = kv.second;
                inner["modules"] = variant(std::move(mods));
            }
            bytes inner_bytes = varsolid::to_bytes(inner);
            uint_64 inner_size = inner_bytes.size();
            varmap info;
            if (_cmps) {
                info["compressed"] = variant(true);
                outer["ast"] = variant(compress::encoder_lz4::sexec(inner_bytes));
            } else {
                info["compressed"] = variant(false);
                outer["ast"] = variant(std::move(inner_bytes));
            }
            info["size"] = variant(inner_size);

            // "hintsize" presence marks a hint; its compression follows the ast's "compressed"
            if (!_hint.empty()) {
                info["hintsize"] = variant(static_cast<uint_64>(_hint.size()));
                outer["hint"] = _cmps ? variant(compress::encoder_lz4::sexec(_hint))
                                      : variant(_hint.to_bytes());
            }

            info["sha256"] = variant(verify::exec(verify::SHA_256, outer["ast"].to<bytes>()));
            outer["info"] = variant(std::move(info));
            return varsolid::to_bytes(outer);
        }

        bool get_dependencies(const bytes_view& _data, compile_result& _out) {
            if (!varsolid::is_valid(_data)) return false;
            varmap outer;
            if (!varsolid::to_varmap(_data, outer)) return false;

            variant iv = outer.value("imports");
            if (iv.is_vec()) {
                for (auto& v : iv.to<varvec>()) {
                    if (v.is<std::string>())
                        _out.imports.push_back(v.to<std::string>());
                }
            }

            variant lv = outer.value("links");
            if (lv.is_vec()) {
                for (auto& v : lv.to<varvec>()) {
                    if (v.is<std::string>())
                        _out.links.push_back(v.to<std::string>());
                }
            }
            return true;
        }

        namespace {

            void ast_indent(std::string& _s, int _level) {
                // net effect: '\n' followed by _level tabs
                size_t len = _s.length();
                _s.resize(len + static_cast<size_t>(_level) + 1, '\t');
                _s[len] = '\n';
            }

            const char* op_name(int _op) {
                if (_op >= 0 && _op < static_cast<int>(O_ENUMSIZE))
                    return s_op_names[static_cast<size_t>(_op)];
                return "???";
            }

            // -1: the vec does not start with an opcode (empty, or a plain value list)
            int try_get_op(const varvec& _vec) {
                if (_vec.empty()) return -1;
                return _vec[0].is<unsigned char>() ? static_cast<int>(_vec[0].to<unsigned char>()) : -1;
            }

            void format_variant(std::string& _s, const variant& _v, int _level);

            void format_vec(std::string& _s, const varvec& _vec, int _level) {
                int op = try_get_op(_vec);
                if (op >= 0) {
                    // O_LOAD holds a name (a slot), O_ILOAD a name computed at run time; both dump flat
                    if (op == O_LOAD) {
                        _s += "(SLOT";
                        for (size_t i = 1; i < _vec.size(); i++) {
                            _s += " ";
                            format_variant(_s, _vec[i], _level);
                        }
                        _s += ")";
                        return;
                    }
                    if (op == O_ILOAD) {
                        _s += "(ILOAD ";
                        format_variant(_s, _vec[1], _level);
                        _s += ")";
                        return;
                    }
                    _s += "(";
                    _s += op_name(op);
                    std::string flat;
                    for (size_t i = 1; i < _vec.size(); i++) {
                        if (i > 1) flat += " ";
                        format_variant(flat, _vec[i], 0);
                    }
                    // keep the one-line form only while the flattened operands stay under 60 chars
                    if (flat.length() < 60 && flat.find('\n') == std::string::npos) {
                        _s += " " + flat + ")";
                        return;
                    }
                    for (size_t i = 1; i < _vec.size(); i++) {
                        ast_indent(_s, _level + 1);
                        format_variant(_s, _vec[i], _level + 1);
                    }
                    ast_indent(_s, _level);
                    _s += ")";
                    return;
                }
                _s += "[";
                for (size_t i = 0; i < _vec.size(); i++) {
                    if (i > 0) _s += " ";
                    format_variant(_s, _vec[i], _level);
                }
                _s += "]";
            }

            void format_variant(std::string& _s, const variant& _v, int _level) {
                // a bare unsigned char leaf is an opcode (OPTYPE), so print its mnemonic
                if (_v.is<unsigned char>()) {
                    _s += op_name(static_cast<int>(_v.to<unsigned char>()));
                } else if (_v.is<long long>()) {
                    _s += std::to_string(_v.to<long long>());
                } else if (_v.is<unsigned long long>()) {
                    _s += std::to_string(_v.to<unsigned long long>());
                } else if (_v.is<double>()) {
                    _s += fmt_double(_v.to<double>());
                } else if (_v.is<bool>()) {
                    _s += _v.to<bool>() ? "true" : "false";
                } else if (_v.is<bytes>()) {
                    _s += "bytes(" + std::to_string(_v.to<bytes>().size()) + ")";
                } else if (_v.is<std::string>()) {
                    std::string str = _v.to<std::string>();
                    for (size_t i = 0; i < str.length(); i++) {
                        switch (str[i]) {
                        case '\n':
                            str.replace(i, 1, "\\n");
                            i++;
                            break;
                        case '\t':
                            str.replace(i, 1, "\\t");
                            i++;
                            break;
                        case '\r':
                            str.replace(i, 1, "\\r");
                            i++;
                            break;
                        case '\\':
                            str.replace(i, 1, "\\\\");
                            i++;
                            break;
                        case '\"':
                            str.replace(i, 1, "\\\"");
                            i++;
                            break;
                        default: break;
                        }
                    }
                    _s += "\"" + str + "\"";
                } else if (_v.is<varvec>()) {
                    format_vec(_s, _v.to<varvec>(), _level);
                } else if (_v.is<varmap>()) {
                    const varmap& m = _v.to<varmap>();
                    _s += "{";
                    bool first = true;
                    for (auto& kv : m) {
                        if (!first) _s += ", ";
                        first = false;
                        _s += kv.first + ": ";
                        format_variant(_s, *kv.second, _level);
                    }
                    _s += "}";
                } else {
                    _s += "null";
                }
            }

        }

        bool unpack(const bytes_view& _data, varmap& _out) {
            varmap outer;
            if (!varsolid::to_varmap(_data, outer)) return false;
            varvec ast;
            varmap modules;
            if (!decompress_inner(outer, ast, modules)) return false;

            std::string dump;
            format_vec(dump, ast, 0);
            _out["ast"] = variant(std::move(dump));

            if (!modules.empty()) {
                varmap& mods = _out["modules"].as<varmap>();
                for (auto& kv : modules) {
                    const variant* mv = kv.second;
                    // module entry {path, resolved, ast}: dump the ast; an entry without one is skipped
                    if (mv->is<varmap>()) {
                        const variant& av = mv->to<varmap>().value("ast");
                        mv = &av;
                    }
                    if (!mv->is<varvec>()) continue;
                    std::string md;
                    format_vec(md, mv->to<varvec>(), 0);
                    mods[kv.first] = variant(std::move(md));
                }
            }

            // outer scalars are hoisted to the top level, the "info" submap stays nested
            varmap info;
            for (const char* k : {"etype", "vtype", "file", "name", "imports", "links",
                                  "time"}) {
                if (outer.contain(k)) info[k] = outer.value(k);
            }
            variant iv = outer.value("info");
            if (iv.is<varmap>()) info["info"] = iv;
            _out["info"] = variant(std::move(info));

            variant hv = outer.value("hint");
            if (hv.is<bytes>() && iv.is<varmap>() && iv.to<varmap>().contain("hintsize")) {
                bytes plain = hv.to<bytes>();
                if (iv.to<varmap>().value("compressed").to<bool>()) {
                    uint_64 hsz = iv.to<varmap>().value("hintsize").to<uint_64>(0);
                    plain = compress::decoder_lz4::sexec(plain, static_cast<int_32>(hsz));
                    if (plain.empty() || plain.size() != hsz) return false;
                }
                _out["hint"] = variant(
                    std::string(reinterpret_cast<const char*>(plain.data()), plain.size()));
            }
            return true;
        }

        std::string prtast(const bytes_view& _data, engine* _eng) {
            varmap vm;
            std::string result;

            if (varsolid::to_varmap(_data, vm)) {

                varvec ast;
                varmap modules;
                if (decompress_inner(vm, ast, modules)) {
                    variant etype_var = vm.value("etype");
                    variant vtype_var = vm.value("vtype");
                    if (etype_var.is<std::string>() && !etype_var.to<std::string>().empty())
                        result += ";; etype: " + etype_var.to<std::string>() + "\n";
                    if (vtype_var.is<uint_64>() && vtype_var.to<uint_64>(0) > 0)
                        result += ";; vtype: " + std::to_string(vtype_var.to<uint_64>()) + "\n";
                    std::string out;
                    format_vec(out, ast, 0);
                    result += out + "\n";
                    return result;
                }
            }

            {
                token_list tl;
                tl.tokenize(_data, nullptr);
                // no ext table: host extensions are unknown here, so $f(...) dumps as a plain EXCALL
                parser p(tl, nullptr, "", {}, 0, nullptr);
                varvec ast = p.parse();
                if (!p.has_error() && !ast.empty()) {
                    std::string out;
                    format_vec(out, ast, 0);
                    return out + "\n";
                }
            }

            // last resort: the host engine compiles (strict ext check); a null engine stops the recursion
            if (_eng) {
                bytes compiled = _eng->compile(_data, "", false);
                if (!compiled.empty())
                    return prtast(bytes_view(compiled), nullptr);
            }

            return "Error: failed to parse or compile\n";
        }

        std::string prtinf(const bytes_view& _data) {
            varmap vm;
            if (!varsolid::to_varmap(_data, vm) || !vm.contain("info"))
                return "Error: not a compiled binary\n";

            std::string result;
            auto put_str = [&](const char* _k) {
                variant v = vm.value(_k);
                if (v.is<std::string>() && !v.to<std::string>().empty())
                    result += std::string(";; ") + _k + ": " + v.to<std::string>() + "\n";
            };
            auto put_int = [&](const char* _k) {
                variant v = vm.value(_k);
                if (v.is<uint_64>())
                    result += std::string(";; ") + _k + ": " + std::to_string(v.to<uint_64>()) + "\n";
                else if (v.is<int_64>())
                    result += std::string(";; ") + _k + ": " + std::to_string(v.to<int_64>()) + "\n";
            };
            auto put_list = [&](const char* _k) {
                variant v = vm.value(_k);
                if (!v.is_vec()) return;
                std::string joined;
                for (auto& e : v.to<varvec>()) {
                    if (!joined.empty()) joined += ", ";
                    joined += e.is<std::string>() ? e.to<std::string>() : "?";
                }
                if (!joined.empty()) result += std::string(";; ") + _k + ": " + joined + "\n";
            };

            put_str("etype");
            put_int("vtype");
            put_str("file");
            put_str("name");
            put_list("imports");
            put_list("links");
            put_int("time");

            variant iv = vm.value("info");
            if (iv.is<varmap>()) {
                const varmap& info = iv.to<varmap>();
                result += ";; info: size=" +
                          std::to_string(info.value("size").to<uint_64>()) +
                          " compressed=" +
                          (info.value("compressed").to<bool>() ? "true" : "false") +
                          " sha256=" + info.value("sha256").to<std::string>() + "\n";
            }

            return result;
        }

        bytes prtfmt(const bytes_view& _data) {
            token_list tl;
            tl.tokenize(_data, nullptr);
            bytes out;
            const char* src = reinterpret_cast<const char*>(_data.data());
            uint_32 src_pos = 0;
            int indent = 0;
            bool line_start = true;
            // for_paren: depth of the "(" opened by for(, whose ";" stay on one line; -1 = none open
            int paren = 0, for_paren = -1;
            // depth of map{...}/vec{} literals: no indent, no line break inside
            int inline_lit = 0;
            bool unary_stick = false;
            tk_enum prev = T_END_OF_FILE;

            auto count_newlines = [&](uint_32 _start, uint_32 _end) -> int {
                int cnt = 0;
                for (uint_32 p = _start; p < _end; p++)
                    if (src[p] == '\n') cnt++;
                return cnt;
            };

            auto tail_char = [&]() -> uint_8 {
                return out.empty() ? 0 : out[out.size() - 1];
            };
            auto trim_tail = [&] {
                while (!out.empty() && (tail_char() == ' ' || tail_char() == '\t'))
                    out.resize(out.size() - 1);
            };
            auto newline = [&] {
                trim_tail();
                if (out.empty() || tail_char() != '\n') out << '\n';
                line_start = true;
            };
            auto indent_pre = [&] {
                if (!line_start) return;
                for (int i = 0; i < indent; i++) out.append("    ", 4);
                line_start = false;
            };
            auto space = [&] {
                if (!out.empty() && tail_char() != ' ' && tail_char() != '\n')
                    out << ' ';
            };

            auto is_unary_op = [](tk_enum _t) {
                return _t == T_MINUS || _t == T_PLUS || _t == T_NOT || _t == T_BIT_NEG ||
                       _t == T_AUTO_INC || _t == T_AUTO_DEC;
            };
            auto prev_is_value = [](tk_enum _t) {
                return _t == T_NAME || _t == T_INT_LITERAL || _t == T_HEX_LITERAL ||
                       _t == T_OCT_LITERAL || _t == T_BIN_LITERAL || _t == T_FLOAT_LITERAL ||
                       _t == T_STRING_LITERAL || _t == T_CHAR_LITERAL || _t == T_BACKTICK_LITERAL || _t == T_RP ||
                       _t == T_RB || _t == T_TRUE || _t == T_FALSE || _t == T_NULL_ ||
                       _t == T_NAN || _t == T_INF ||
                       _t == T_INT || _t == T_FLOAT || _t == T_STRING || _t == T_BOOL ||
                       _t == T_BYTES || _t == T_VEC || _t == T_MAP || _t == T_LST;
            };

            auto prev_is_op = [&is_unary_op](tk_enum _t) {
                return is_unary_op(_t) || _t == T_STAR || _t == T_SLASH || _t == T_PERCENT ||
                       _t == T_ASS || _t == T_ASS_ADD || _t == T_ASS_MINUS ||
                       _t == T_ASS_MUL || _t == T_ASS_DIV || _t == T_ASS_MOD ||
                       _t == T_ASS_LSHIFT || _t == T_ASS_RSHIFT || _t == T_ASS_BIT_AND ||
                       _t == T_ASS_BIT_OR || _t == T_ASS_BIT_XOR ||
                       _t == T_LT || _t == T_GT || _t == T_LE || _t == T_GE || _t == T_EQ ||
                       _t == T_NE || _t == T_BIT_AND || _t == T_BIT_OR || _t == T_BIT_XOR ||
                       _t == T_BIT_NEG || _t == T_AND || _t == T_OR || _t == T_NOT ||
                       _t == T_LSHIFT || _t == T_RSHIFT || _t == T_QUESTION || _t == T_COLON ||
                       _t == T_LP || _t == T_LB || _t == T_COMMA;
            };
            auto prev_is_assign = [](tk_enum _t) {
                return _t == T_ASS || _t == T_ASS_ADD || _t == T_ASS_MINUS ||
                       _t == T_ASS_MUL || _t == T_ASS_DIV || _t == T_ASS_MOD ||
                       _t == T_ASS_LSHIFT || _t == T_ASS_RSHIFT ||
                       _t == T_ASS_BIT_AND || _t == T_ASS_BIT_OR || _t == T_ASS_BIT_XOR;
            };

            for (size_t i = 0; i < tl.count(); i++) {
                const tk_enum t = tl.type(i);
                const bytes_view v = tl.text(i);
                const char* c = reinterpret_cast<const char*>(v.data());
                const size_t n = v.size();
                const uint_32 tok_ofst = tl.ofst(i);

                // reproduce blank lines: newline() already ended the line, emit the remaining ones
                if (i > 0 && line_start) {
                    int nl_cnt = count_newlines(src_pos, tok_ofst);
                    if (nl_cnt >= 2) {
                        for (int k = 1; k < nl_cnt; k++) out << '\n';
                    }
                }

                switch (t) {
                case T_COMMENT: {
                    const bool block = n >= 2 && c[0] == '/' && c[1] == '*';
                    indent_pre();
                    out.append(c, n);
                    if (block && c[n - 1] == '\n')
                        line_start = true;
                    else
                        newline();
                    break;
                }
                case T_LC: {
                    const bool literal =
                        !line_start && (prev == T_NAME || (prev >= T_INT && prev <= T_EVAL));
                    if (line_start) indent_pre();
                    else if (!literal) space();
                    out << '{';
                    if (literal) {
                        inline_lit++;

                    } else {
                        indent++;
                        newline();
                    }
                    break;
                }
                case T_RC:
                    if (inline_lit > 0) {
                        out << '}';
                        inline_lit--;
                        break;
                    }
                    if (!line_start) newline();
                    if (indent > 0) indent--;
                    indent_pre();
                    out << '}';
                    newline();
                    break;
                case T_SEMICOLON:
                    if (for_paren >= 0 && paren > for_paren) {
                        out << ';';
                        space();
                    } else {
                        indent_pre();
                        out << ';';
                        newline();
                    }
                    break;
                case T_COMMA:
                    out << ',';
                    if (inline_lit == 0) space();
                    break;
                case T_LP:
                case T_LB:
                    // f(x) / a[i] stay tight; "if (x)" and operator contexts get the space
                    indent_pre();
                    if (prev_is_op(prev) || prev == T_RC) space();
                    else if (prev == T_NAME || prev == T_RP || prev == T_RB) {
                    } else if (prev == T_IF || prev == T_WHILE || prev == T_FOR ||
                               prev == T_SWITCH || prev == T_CATCH || prev == T_TRY)
                        space();
                    else if (t == T_LP) {
                    } else
                        space();
                    out.append(c, n);
                    paren++;
                    break;
                case T_RP:
                case T_RB:
                    indent_pre();
                    out.append(c, n);
                    if (paren > 0) paren--;
                    if (for_paren >= 0 && paren <= for_paren) for_paren = -1;
                    break;
                case T_DOT:
                case T_COLONCOLON: {
                    indent_pre();
                    bool stmt_kw = (prev == T_RETURN || prev == T_DELETE || prev == T_THROW ||
                                    prev == T_ELSE);
                    if (stmt_kw || (!prev_is_value(prev) && !(prev >= T_VAR && prev <= T_EVAL)))
                        space();
                    out.append(c, n);
                    break;
                }
                case T_DOLLAR:
                case T_AT: {
                    indent_pre();
                    bool no_space = (prev == T_LP || prev == T_LB || prev == T_DOT ||
                                     prev == T_COLONCOLON || prev_is_value(prev) ||
                                     (prev >= T_VAR && prev <= T_EVAL));
                    if (!no_space || prev == T_RETURN || prev == T_DELETE || prev == T_THROW ||
                        prev == T_ELSE)
                        space();
                    out.append(c, n);
                    break;
                }
                case T_ELSE:
                    if (prev == T_RC) {
                        if (!out.empty() && out[out.size() - 1] == '\n')
                            out.resize(out.size() - 1);
                        line_start = false;
                    }
                    indent_pre();
                    space();
                    out.append(c, n);
                    break;
                case T_CATCH:
                    if (prev == T_RC) {
                        if (!out.empty() && out[out.size() - 1] == '\n')
                            out.resize(out.size() - 1);
                        line_start = false;
                    }
                    indent_pre();
                    space();
                    out.append(c, n);
                    break;
                default: {
                    if (t == T_FOR && for_paren < 0) for_paren = paren;
                    const bool prefix =
                        is_unary_op(t) && !prev_is_value(prev) &&
                        (prev_is_op(prev) || prev == T_RC || prev == T_ELSE ||
                         prev == T_RETURN || prev == T_DELETE || prev == T_THROW ||
                         prev == T_QUESTION || prev == T_COLON || line_start);
                    indent_pre();
                    if (prefix) {
                        if (!line_start && prev != T_LP && prev != T_LB && prev != T_DOLLAR &&
                            prev != T_AT && prev != T_RC && prev != T_ELSE &&
                            (!prev_is_op(prev) || prev_is_assign(prev)))
                            space();
                        out.append(c, n);
                    } else if (prev == T_LP || prev == T_LB || prev == T_DOLLAR || prev == T_AT ||
                               prev == T_DOT || prev == T_COLONCOLON || unary_stick ||
                               ((t == T_AUTO_INC || t == T_AUTO_DEC) && prev_is_value(prev))) {
                        out.append(c, n);
                    } else {
                        if (inline_lit == 0) space();
                        out.append(c, n);
                    }
                    unary_stick = prefix;
                    break;
                }
                }
                // a token with a raw newline in it (block comment, backtick string) ends the line
                if (n > 0 && std::memchr(c, '\n', n)) line_start = true;
                if (t != T_COMMENT) prev = t;
                src_pos = tok_ofst + static_cast<uint_32>(n);
            }
            trim_tail();
            if (!out.empty() && out[out.size() - 1] != '\n')
                out << '\n';
            return out;
        }

    }
}
