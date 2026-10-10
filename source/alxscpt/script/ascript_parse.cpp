/*****************************************************************/ /**
 * \file   ascript_parse.cpp
 * \brief  Script parser — recursive-descent LL with precedence climbing
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_parse.h"
#include "afile.h"
#include "avarsolid.h"
#include "averify.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace alx {
    namespace script {

        struct NestGuard {
            parser& p;
            bool ok;
            NestGuard(parser& _p) : p(_p), ok(true) {
                // overflow kills the rest of the file: m_fatal silences later errors, ok unwinds
                if (p.m_max_nest && ++p.m_nest_depth > p.m_max_nest) {
                    p.error("nesting too deep");
                    p.m_fatal = true;
                    p.synchronize();
                    p.m_nest_depth--;
                    ok = false;
                }
            }
            ~NestGuard() {
                if (ok) p.m_nest_depth--;
            }
        };

        parser::parser(const token_list& _tokens,
                       const alx::signal<const compile_error&>* _on_error,
                       const std::string& _file_path,
                       const std::list<std::string>& _search_paths,
                       size_t _max_nest,
                       const std::unordered_map<std::string, native_func>* _ext_table,
                       const std::unordered_map<std::string, variant>* _def_table,
                       compile_result* _embed_out,
                       const std::string& _embed_etype,
                       uint_64 _embed_vtype,
                       bool _debug)
            : m_tokens(_tokens), m_file_path(_file_path), m_on_error(_on_error),
              m_max_nest(_max_nest), m_debug(_debug), m_ext_table(_ext_table), m_def_table(_def_table),
              m_embed_out(_embed_out), m_embed_etype(_embed_etype), m_embed_vtype(_embed_vtype) {
            for (auto& p : _search_paths) m_search_paths.push_back(p);
        }

        // m_pos may sit on a skipped ERROR/COMMENT token; resolve the index before touching tokens
        size_t parser::index() const {
            size_t idx = m_pos;
            while (idx < m_tokens.count() &&
                   (m_tokens.type(idx) == T_ERROR || m_tokens.type(idx) == T_COMMENT))
                idx++;
            return idx;
        }

        tk_enum parser::peek(size_t _ahead) const {
            size_t idx = index();
            idx += _ahead;
            while (idx < m_tokens.count() &&
                   (m_tokens.type(idx) == T_ERROR || m_tokens.type(idx) == T_COMMENT))
                idx++;
            if (idx >= m_tokens.count()) return T_END_OF_FILE;
            return m_tokens.type(idx);
        }

        tk_enum parser::advance() {
            while (m_pos < m_tokens.count() &&
                   (m_tokens.type(m_pos) == T_ERROR || m_tokens.type(m_pos) == T_COMMENT))
                m_pos++;
            if (m_pos >= m_tokens.count()) return T_END_OF_FILE;
            return m_tokens.type(m_pos++);
        }

        bool parser::check(tk_enum _t) const {
            return peek() == _t;
        }

        bool parser::match(tk_enum _t) {
            if (check(_t)) {
                advance();
                return true;
            }
            return false;
        }

        bool parser::consume(tk_enum _t, const char* _expected) {
            if (check(_t)) {
                advance();
                return true;
            }
            error(std::string("Expected ") + _expected);
            return false;
        }

        bytes_view parser::text(size_t _ahead) const {
            size_t idx = index();
            idx += _ahead;
            while (idx < m_tokens.count() &&
                   (m_tokens.type(idx) == T_ERROR || m_tokens.type(idx) == T_COMMENT))
                idx++;
            if (idx >= m_tokens.count()) return bytes_view();
            return m_tokens.text(idx);
        }

        uint_32 parser::row() const {
            size_t idx = index();
            if (idx >= m_tokens.count()) return 0;
            return m_tokens.row(idx);
        }

        uint_32 parser::col() const {
            size_t idx = index();
            if (idx >= m_tokens.count()) return 0;
            return m_tokens.col(idx);
        }

        // [O_DEBUG, row, col, ofst]; take it before parsing the stmt, push it ahead of its node
        varvec parser::pos_node() const {
            size_t idx = index();
            varvec v;
            v.push_back(variant(OPTYPE(O_DEBUG)));
            v.push_back(variant(static_cast<int_64>(row())));
            v.push_back(variant(static_cast<int_64>(col())));
            v.push_back(variant(static_cast<int_64>((idx < m_tokens.count()) ? m_tokens.ofst(idx) : 0)));
            return v;
        }

        void parser::error(const std::string& _msg) {
            m_has_error = true;
            if (m_fatal) return;
            size_t idx = index();
            if (m_on_error)
                m_on_error->exec({{m_file_path, row(), col(), (idx < m_tokens.count()) ? m_tokens.ofst(idx) : 0},
                                  _msg});
        }

        void parser::synchronize() {
            while (!check(T_END_OF_FILE) && !check(T_SEMICOLON) && !check(T_RC)) {
                m_pos++;
            }
            if (check(T_SEMICOLON) || check(T_RC)) m_pos++;
        }

        bool parser::has_comma_in_brackets() {
            size_t saved = m_pos;
            int depth = 1;
            while (depth > 0 && !check(T_END_OF_FILE)) {
                tk_enum t = advance();
                if (t == T_LB || t == T_LP || t == T_LC) depth++;
                else if (t == T_RB || t == T_RP || t == T_RC) depth--;
                else if (t == T_COMMA && depth == 1) {
                    m_pos = saved;
                    return true;
                }
            }
            m_pos = saved;
            return false;
        }

        static varvec make_node(op_enum _op, std::initializer_list<variant> _args = {}) {
            varvec v;
            v.push_back(variant(OPTYPE(_op)));
            for (auto& a : _args) v.push_back(a);
            return v;
        }

        static varvec make_node(op_enum _op, const varvec& args) {
            varvec v;
            v.push_back(variant(OPTYPE(_op)));
            v.insert(v.end(), args.begin(), args.end());
            return v;
        }

        static variant type_default_value(tk_enum _tok) {
            switch (_tok) {
            case T_INT: return variant(int_64(0));
            case T_FLOAT: return variant(double(0));
            case T_STRING: return variant(std::string(""));
            case T_BOOL: return variant(bool(false));
            case T_BYTES: return variant(bytes());
            case T_VEC: return variant(varvec());
            case T_MAP: return variant(varmap());
            case T_LST: return variant(varlst());
            default: return variant();
            }
        }

        varvec parser::make_binary(op_enum _op, varvec& _left, varvec& _right) {
            varvec v;
            v.push_back(variant(OPTYPE(_op)));
            v.push_back(variant(std::move(_left)));
            v.push_back(variant(std::move(_right)));
            return v;
        }

        varvec parser::make_unary(op_enum _op, varvec& _operand) {
            varvec v;
            v.push_back(variant(OPTYPE(_op)));
            v.push_back(variant(std::move(_operand)));
            return v;
        }

        varvec parser::make_name_ref(const bytes_view& _name) {
            varvec v;
            v.push_back(variant(OPTYPE(O_LOAD)));
            v.push_back(variant(std::string(_name.to_string())));
            return v;
        }

        varvec parser::parse() {

            // a lexer error anywhere kills the parse; the skippers would otherwise walk past it
            for (size_t i = 0; i < m_tokens.count(); i++) {
                if (m_tokens.type(i) == T_ERROR) {
                    m_has_error = true;
                    return {};
                }
            }
            return parse_program();
        }

        varvec parser::parse(const bytes_view& _source, const std::string& _file_path,
                             const std::list<std::string>& _search_paths,
                             const std::unordered_map<std::string, native_func>* _ext_table,
                             const std::unordered_map<std::string, variant>* _def_table,
                             const size_t _max_nest, bool _debug,
                             const alx::signal<const compile_error&>* _on_error) {
            token_list tl;
            tl.tokenize(_source, _on_error, _file_path);
            parser p(tl, _on_error, _file_path, _search_paths, _max_nest, _ext_table, _def_table,
                     nullptr, std::string(), 0, _debug);
            return p.parse();
        }

        varvec parser::parse_body(const bytes_view& _source, const std::string& _file_path,
                                  const std::list<std::string>& _search_paths,
                                  const std::unordered_map<std::string, native_func>* _ext_table,
                                  const std::unordered_map<std::string, variant>* _def_table,
                                  bool _debug) {
            token_list tl;
            tl.tokenize(_source, nullptr, _file_path);
            parser p(tl, nullptr, _file_path, _search_paths, 0, _ext_table, _def_table,
                     nullptr, std::string(), 0, _debug);
            return p.parse_body();
        }

        varvec parser::parse_body() {
            varvec stmts;
            while (!check(T_END_OF_FILE)) {
                varvec pos = m_debug ? pos_node() : varvec();
                varvec s = parse_top_stmt();
                // an empty node is the recovery sentinel (stray ';' or a failed stmt), so skip it
                if (s.empty()) continue;
                if (m_debug) stmts.push_back(variant(std::move(pos)));
                stmts.push_back(variant(std::move(s)));
            }
            return stmts;
        }

        varvec parser::parse_program() {
            return make_node(O_PROGRAM, parse_body());
        }

        varvec parser::parse_top_stmt() {
            switch (peek()) {
            case T_VAR: return parse_var_decl();

            case T_INT:
            case T_FLOAT:
            case T_STRING:
            case T_BOOL:
            case T_BYTES:
            case T_VEC:
            case T_MAP:
            case T_LST:
                if (peek(1) == T_NAME) return parse_typed_var_decl();
                return parse_assign_or_expr_stmt();
            case T_IF: return parse_if_stmt();
            case T_WHILE: return parse_while_stmt();
            case T_FOR: return parse_for_stmt();
            case T_SWITCH: return parse_switch_stmt();
            case T_TRY: return parse_try_stmt();
            case T_RETURN: return parse_return_stmt();
            case T_BREAK: return parse_break_stmt();
            case T_CONTINUE: return parse_continue_stmt();
            case T_THROW: return parse_throw_stmt();
            case T_LC: return parse_block();
            case T_IMPORT: return parse_import_stmt();
            case T_LINK: return parse_link_stmt();
            case T_DEF: return parse_def_stmt();
            case T_SEMICOLON:
                advance();
                return varvec();
            default:
                return parse_assign_or_expr_stmt();
            }
        }

        varvec parser::parse_stmt() {
            if (check(T_IMPORT) || check(T_LINK)) {
                error("import/link can only appear at top level");
                advance();
                synchronize();
                return varvec();
            }
            switch (peek()) {
            case T_VAR: return parse_var_decl();

            case T_INT:
            case T_FLOAT:
            case T_STRING:
            case T_BOOL:
            case T_BYTES:
            case T_VEC:
            case T_MAP:
            case T_LST:
                if (peek(1) == T_NAME) return parse_typed_var_decl();
                return parse_assign_or_expr_stmt();
            case T_IF: return parse_if_stmt();
            case T_WHILE: return parse_while_stmt();
            case T_FOR: return parse_for_stmt();
            case T_SWITCH: return parse_switch_stmt();
            case T_TRY: return parse_try_stmt();
            case T_RETURN: return parse_return_stmt();
            case T_BREAK: return parse_break_stmt();
            case T_CONTINUE: return parse_continue_stmt();
            case T_THROW: return parse_throw_stmt();
            case T_LC: return parse_block();
            case T_DEF: return parse_def_stmt();
            case T_SEMICOLON:
                advance();
                return varvec();
            default:
                return parse_assign_or_expr_stmt();
            }
        }

        varvec parser::parse_block() {
            NestGuard ng(*this);
            if (!ng.ok) return make_node(O_BLOCK);
            consume(T_LC, "{");
            varvec stmts;

            while (!check(T_RC) && !check(T_END_OF_FILE)) {
                varvec pos = m_debug ? pos_node() : varvec();
                varvec s = parse_stmt();
                if (s.empty()) continue;
                if (m_debug) stmts.push_back(variant(std::move(pos)));
                stmts.push_back(variant(std::move(s)));
            }
            consume(T_RC, "}");
            return make_node(O_BLOCK, stmts);
        }

        varvec parser::parse_stmt_or_block() {
            if (check(T_LC))
                return parse_block();
            varvec s = parse_stmt();
            // the caller needs a body node: a failed statement still yields an empty block
            if (s.empty()) return make_node(O_BLOCK);
            return s;
        }

        std::string parser::parse_target_name() {
            if (match(T_AT)) {

                if (check(T_LP)) {
                    advance();
                    parse_expr();
                    consume(T_RP, ")");
                } else {
                    consume(T_NAME, "name after @");
                }
                error("indirect name (@) not allowed in declaration; use a plain identifier");
                return {};
            }
            bytes_view name = text();
            consume(T_NAME, "identifier");
            return name.to_string();
        }

        varvec parser::parse_var_names() {

            varvec result;
            result.push_back(variant(OPTYPE(O_VAR)));
            do {
                std::string name = parse_target_name();
                result.push_back(variant(std::move(name)));
                if (match(T_ASS)) {
                    result.push_back(variant(parse_assign()));
                } else {
                    result.push_back(variant());
                }
                if (!match(T_COMMA)) break;
            } while (true);
            return result;
        }

        varvec parser::parse_var_decl() {
            advance();
            auto result = parse_var_names();
            consume(T_SEMICOLON, ";");
            return result;
        }

        varvec parser::parse_typed_var_decl() {

            op_enum type_op = O_NOP;
            switch (peek()) {
            case T_INT: type_op = O_INT; break;
            case T_FLOAT: type_op = O_FLOAT; break;
            case T_STRING: type_op = O_STRING; break;
            case T_BOOL: type_op = O_BOOL; break;
            case T_BYTES: type_op = O_BYTES; break;
            case T_VEC: type_op = O_VEC; break;
            case T_MAP: type_op = O_MAP; break;
            case T_LST: type_op = O_LST; break;
            default: break;
            }
            tk_enum tok = peek();
            advance();
            std::string name = parse_target_name();

            // int a = e desugars to var a = int(e); the bare form takes type_default_value(tok)
            if (match(T_ASS)) {

                varvec init;
                init.push_back(variant(OPTYPE(type_op)));
                init.push_back(variant(parse_assign()));
                return make_node(O_VAR, {variant(std::move(name)), variant(std::move(init))});
            }

            return make_node(O_VAR, {variant(std::move(name)), variant(varvec{type_default_value(tok)})});
        }

        varvec parser::parse_assign_or_expr_stmt() {
            varvec expr = parse_expr();
            consume(T_SEMICOLON, ";");
            return expr;
        }

        varvec parser::parse_if_stmt() {
            advance();
            consume(T_LP, "(");
            varvec test = parse_expr();
            consume(T_RP, ")");
            varvec true_body = parse_stmt_or_block();

            varvec false_body;
            if (match(T_ELSE)) {
                if (check(T_IF)) {
                    false_body = parse_if_stmt();
                } else {
                    false_body = parse_stmt_or_block();
                }
            } else {
                false_body = make_node(O_BLOCK);
            }

            return make_node(O_IF, {variant(std::move(test)),
                                    variant(std::move(true_body)),
                                    variant(std::move(false_body))});
        }

        varvec parser::parse_while_stmt() {
            advance();
            consume(T_LP, "(");
            varvec test = parse_expr();
            consume(T_RP, ")");
            m_loop_depth++;
            varvec body = parse_stmt_or_block();
            m_loop_depth--;
            return make_node(O_WHILE, {variant(std::move(test)), variant(std::move(body))});
        }

        varvec parser::parse_for_stmt() {
            advance();
            consume(T_LP, "(");
            varvec head = parse_for_head();
            consume(T_RP, ")");
            m_loop_depth++;
            varvec body = parse_stmt_or_block();
            m_loop_depth--;

            varvec result;
            // the head arity picks the node: 2 = for-each [target, iter], 3 = for-c [init, test, update]
            if (head.size() <= 2) {

                result.push_back(variant(OPTYPE(O_FOREACH)));
                result.push_back(variant(std::move(head[0])));
                result.push_back(variant(std::move(head[1])));
                result.push_back(variant(std::move(body)));
            } else {

                result.push_back(variant(OPTYPE(O_FOR)));
                result.push_back(variant(std::move(head[0])));
                result.push_back(variant(std::move(head[1])));
                result.push_back(variant(std::move(head[2])));
                result.push_back(variant(std::move(body)));
            }
            return result;
        }

        varvec parser::parse_for_head() {
            if (match(T_VAR)) {

                std::string firstName = parse_target_name();

                if (match(T_COLON)) {

                    varvec iter = parse_expr();
                    varvec head;
                    varvec target = make_node(O_VAR, {variant(std::move(firstName)), variant()});
                    head.push_back(variant(std::move(target)));
                    head.push_back(variant(std::move(iter)));
                    return head;
                }

                varvec init = make_node(O_VAR);
                init.push_back(variant(std::move(firstName)));
                if (match(T_ASS)) init.push_back(variant(parse_assign()));
                else init.push_back(variant());
                while (match(T_COMMA)) {
                    std::string n = parse_target_name();
                    init.push_back(variant(std::move(n)));
                    if (match(T_ASS)) init.push_back(variant(parse_assign()));
                    else init.push_back(variant());
                }
                consume(T_SEMICOLON, ";");
                varvec test = check(T_SEMICOLON) ? varvec() : parse_expr();
                consume(T_SEMICOLON, ";");
                varvec update = check(T_RP) ? varvec() : parse_expr();
                varvec head;
                head.push_back(variant(std::move(init)));
                head.push_back(variant(std::move(test)));
                head.push_back(variant(std::move(update)));
                return head;
            }

            {
                tk_enum tok = peek();
                bool is_type = (tok == T_INT || tok == T_FLOAT || tok == T_STRING || tok == T_BOOL || tok == T_BYTES || tok == T_VEC || tok == T_MAP || tok == T_LST);
                if (is_type && peek(1) == T_NAME) {

                    op_enum type_op = O_NOP;
                    switch (tok) {
                    case T_INT: type_op = O_INT; break;
                    case T_FLOAT: type_op = O_FLOAT; break;
                    case T_STRING: type_op = O_STRING; break;
                    case T_BOOL: type_op = O_BOOL; break;
                    case T_BYTES: type_op = O_BYTES; break;
                    case T_VEC: type_op = O_VEC; break;
                    case T_MAP: type_op = O_MAP; break;
                    case T_LST: type_op = O_LST; break;
                    default: break;
                    }
                    advance();
                    std::string firstName = parse_target_name();

                    if (match(T_COLON)) {

                        varvec iter = parse_expr();
                        varvec head;
                        varvec init_node;
                        init_node.push_back(variant(OPTYPE(O_VAR)));
                        init_node.push_back(variant(std::move(firstName)));
                        init_node.push_back(varvec{type_default_value(tok)});
                        head.push_back(variant(std::move(init_node)));
                        head.push_back(variant(std::move(iter)));
                        return head;
                    }

                    varvec init = make_node(O_VAR);
                    init.push_back(variant(std::move(firstName)));
                    if (match(T_ASS)) {

                        varvec type_conv;
                        type_conv.push_back(variant(OPTYPE(type_op)));
                        type_conv.push_back(variant(parse_assign()));
                        init.push_back(variant(std::move(type_conv)));
                    } else {
                        init.push_back(varvec{type_default_value(tok)});
                    }
                    while (match(T_COMMA)) {
                        std::string n = parse_target_name();
                        init.push_back(variant(std::move(n)));
                        if (match(T_ASS)) {
                            varvec type_conv;
                            type_conv.push_back(variant(OPTYPE(type_op)));
                            type_conv.push_back(variant(parse_assign()));
                            init.push_back(variant(std::move(type_conv)));
                        } else {
                            init.push_back(varvec{type_default_value(tok)});
                        }
                    }
                    consume(T_SEMICOLON, ";");
                    varvec test = check(T_SEMICOLON) ? varvec() : parse_expr();
                    consume(T_SEMICOLON, ";");
                    varvec update = check(T_RP) ? varvec() : parse_expr();
                    varvec head;
                    head.push_back(variant(std::move(init)));
                    head.push_back(variant(std::move(test)));
                    head.push_back(variant(std::move(update)));
                    return head;
                }
            }

            if (match(T_SEMICOLON)) {

                varvec init;
                varvec test = check(T_SEMICOLON) ? varvec() : parse_expr();
                consume(T_SEMICOLON, ";");
                varvec update = check(T_RP) ? varvec() : parse_expr();

                varvec head;
                head.push_back(variant(std::move(init)));
                head.push_back(variant(std::move(test)));
                head.push_back(variant(std::move(update)));
                return head;
            }

            if (check(T_NAME) && peek(1) == T_COLON) {

                bytes_view target_name = text();
                advance();
                advance();
                varvec iter = parse_expr();

                varvec head;
                head.push_back(variant(make_name_ref(target_name)));
                head.push_back(variant(std::move(iter)));
                return head;
            }

            varvec init_expr = parse_expr();

            if (match(T_COLON)) {

                error("for-each target must be a variable name");
            }

            consume(T_SEMICOLON, ";");
            varvec test = check(T_SEMICOLON) ? varvec() : parse_expr();
            consume(T_SEMICOLON, ";");
            varvec update = check(T_RP) ? varvec() : parse_expr();

            varvec head;
            head.push_back(variant(std::move(init_expr)));
            head.push_back(variant(std::move(test)));
            head.push_back(variant(std::move(update)));
            return head;
        }

        varvec parser::parse_switch_stmt() {
            advance();
            consume(T_LP, "(");
            varvec test = parse_expr();
            consume(T_RP, ")");
            consume(T_LC, "{");
            m_switch_depth++;

            auto make_body = [&](varvec& _stmts, std::vector<varvec>& _pos) -> varvec {
                // one stmt stays bare: an O_BLOCK wrapper is a frame, i.e. a scope the case lacks
                if (_stmts.size() == 1) return std::move(_stmts[0].to<varvec>());
                varvec blk;
                for (size_t i = 0; i < _stmts.size(); i++) {
                    if (m_debug) blk.push_back(variant(std::move(_pos[i])));
                    blk.push_back(std::move(_stmts[i]));
                }
                return make_node(O_BLOCK, blk);
            };

            varvec cases;
            while (check(T_CASE)) {
                advance();
                varvec val = parse_expr();
                consume(T_COLON, ":");
                varvec body_stmts;
                std::vector<varvec> body_pos;
                while (!check(T_CASE) && !check(T_DEFAULT) && !check(T_RC) && !check(T_END_OF_FILE)) {
                    varvec pos = m_debug ? pos_node() : varvec();
                    varvec s = parse_stmt();
                    if (s.empty()) continue;
                    if (m_debug) body_pos.push_back(std::move(pos));
                    body_stmts.push_back(variant(std::move(s)));
                }
                cases.push_back(variant(make_node(O_CASE,
                                                  {variant(std::move(val)),
                                                   variant(make_body(body_stmts, body_pos))})));
            }

            varvec default_body;
            if (match(T_DEFAULT)) {
                consume(T_COLON, ":");
                varvec body_stmts;
                std::vector<varvec> body_pos;
                while (!check(T_RC) && !check(T_END_OF_FILE)) {
                    varvec pos = m_debug ? pos_node() : varvec();
                    varvec s = parse_stmt();
                    if (s.empty()) continue;
                    if (m_debug) body_pos.push_back(std::move(pos));
                    body_stmts.push_back(variant(std::move(s)));
                }
                default_body = make_body(body_stmts, body_pos);
            }

            consume(T_RC, "}");

            varvec args;
            args.push_back(variant(std::move(test)));
            for (auto& c : cases) args.push_back(variant(std::move(c)));
            if (!default_body.empty())
                args.push_back(variant(make_node(O_DEFAULT, {variant(std::move(default_body))})));

            m_switch_depth--;
            return make_node(O_SWITCH, args);
        }

        varvec parser::parse_try_stmt() {
            advance();
            varvec try_body = parse_block();
            consume(T_CATCH, "catch");
            consume(T_LP, "(");
            bytes_view catch_var = text();
            consume(T_NAME, "exception variable name");
            consume(T_RP, ")");
            varvec catch_body = parse_block();

            return make_node(O_TRY, {variant(std::move(try_body)),
                                     variant(make_node(O_CATCH, {variant(catch_var.to_string()),
                                                                 variant(std::move(catch_body))}))});
        }

        varvec parser::parse_return_stmt() {
            advance();
            if (match(T_SEMICOLON)) {
                return make_node(O_RETURN);
            }
            varvec val = parse_expr();
            consume(T_SEMICOLON, ";");

            // tail call: a self-call in return position becomes O_TCALL, which rebinds the def frame
            if (!m_func_stack.empty() && val.size() >= 2 &&
                val[0].is<OPTYPE>() && val[1].is<std::string>() &&
                val[1].to<std::string>() == m_func_stack.back() &&
                static_cast<op_enum>(val[0].to<OPTYPE>()) == O_CALL) {
                val[0] = variant(OPTYPE(O_TCALL));
                return val;
            }
            return make_node(O_RETURN, {variant(std::move(val))});
        }

        varvec parser::parse_break_stmt() {
            advance();
            if (m_loop_depth <= 0 && m_switch_depth <= 0) error("break outside loop or switch");
            consume(T_SEMICOLON, ";");
            return make_node(O_BREAK);
        }

        varvec parser::parse_continue_stmt() {
            advance();
            if (m_loop_depth <= 0) error("continue outside loop");
            consume(T_SEMICOLON, ";");
            return make_node(O_CONTINUE);
        }

        varvec parser::parse_throw_stmt() {
            advance();
            varvec val = parse_expr();
            consume(T_SEMICOLON, ";");
            return make_node(O_THROW, {variant(std::move(val))});
        }

        varvec parser::parse_import_stmt() {
            advance();
            bytes_view path_tok = text();
            consume(T_STRING_LITERAL, "string literal");
            consume(T_AS, "as");
            std::string alias = parse_target_name();
            consume(T_SEMICOLON, ";");

            std::string path_str = path_tok.to_string();

            std::string rel = path_str.size() >= 2 ? path_str.substr(1, path_str.size() - 2) : std::string();
            if (m_embed_out) {

                std::string key = embed_resolve(rel);
                return make_node(O_IMPORT, {variant(key), variant(std::move(alias))});
            }
            return make_node(O_IMPORT, {variant(rel), variant(std::move(alias))});
        }

        varvec parser::parse_link_stmt() {
            if (m_embed_out) {
                error("link is not allowed in embed mode");
                advance();
            } else {
                advance();
            }
            bytes_view path_tok = text();
            consume(T_STRING_LITERAL, "string literal");
            consume(T_AS, "as");
            std::string alias = parse_target_name();
            consume(T_SEMICOLON, ";");

            std::string path_str = path_tok.to_string();

            std::string rel = path_str.size() >= 2 ? path_str.substr(1, path_str.size() - 2) : std::string();
            return make_node(O_LINK, {variant(rel), variant(std::move(alias))});
        }

        bool parser::check_embedded_axp(const varmap& _vm, const std::string& _rel) {
            std::string axp_type = _vm.value("etype").to<std::string>("");
            uint_64 axp_ver = _vm.value("vtype").to<uint_64>(0);
            if (axp_type != m_embed_etype) {
                error("engine type mismatch: expected '" + m_embed_etype +
                      "', got '" + axp_type + "'");
                return false;
            }
            if (axp_ver > m_embed_vtype) {
                error("version too new: axp v" + std::to_string(axp_ver) +
                      ", engine v" + std::to_string(m_embed_vtype));
                return false;
            }
            variant iv = _vm.value("imports");
            variant lv = _vm.value("links");
            if ((iv.is_vec() && !iv.to<varvec>().empty()) ||
                (lv.is_vec() && !lv.to<varvec>().empty())) {
                error("Compiled module has unresolved dependencies: " + _rel);
                return false;
            }
            return true;
        }

        std::string parser::embed_resolve(const std::string& _rel) {
            std::vector<std::string> local_stack;
            std::vector<std::string>* stack = m_embed_stack ? m_embed_stack : &local_stack;

            std::string full;
            if (file_info::is_absolute(_rel)) {
                file_info fi(_rel);
                if (fi.is_exist()) full = fi.path();
            } else if (!m_file_path.empty()) {
                file_info fi(m_file_path);
                std::string base = fi.is_dir() ? fi.path() : fi.get_parent().path();
                file_info fj(base + "/" + _rel);
                if (fj.is_exist()) full = fj.path();
            }
            if (full.empty()) {
                error("Cannot resolve import: " + _rel);
                return _rel;
            }
            for (auto& p : *stack) {
                if (p == full) {
                    error("circular import: " + _rel);
                    return _rel;
                }
            }
            if (m_embed_out) {
                for (auto& kv : m_embed_out->modules) {
                    if (kv.second.is<varmap>() &&
                        kv.second.to<varmap>().value("resolved").to<std::string>() == full)
                        return kv.first;
                }
            }
            stack->push_back(full);
            bytes content = file::read_all(full);
            if (content.empty()) {
                stack->pop_back();
                error("Cannot read import: " + _rel);
                return _rel;
            }
            auto* hasher = verify::create(verify::SHA_256);
            hasher->update(full);
            hasher->update(bytes_view(content));
            // content-addressed module key: 16 hex of sha256(absolute path + file contents)
            std::string key = "@" + hasher->hexdigest().substr(0, 16);
            delete hasher;
            varmap entry;
            entry["path"] = variant(_rel);
            entry["resolved"] = variant(full);
            if (varsolid::is_valid(bytes_view(content))) {
                varmap vm;
                bool ok = varsolid::to_varmap(bytes_view(content), vm) &&
                          check_embedded_axp(vm, _rel);
                varmap inner_modules;
                if (ok) ok = decompress_inner(vm, entry["ast"].as<varvec>(), inner_modules);
                if (ok) {
                    m_embed_out->modules[key] = variant(std::move(entry));
                    for (auto it = inner_modules.cbegin(); it != inner_modules.cend(); ++it)
                        if (m_embed_out->modules.find(it.key()) == m_embed_out->modules.end())
                            m_embed_out->modules[it.key()] = it.value();
                } else if (!entry["ast"].is<varvec>()) {
                    error("Corrupt compiled module: " + _rel);
                }
            } else {
                token_list tl;
                tl.tokenize(bytes_view(content), m_on_error, full);
                parser sub(tl, m_on_error, full, m_search_paths, m_max_nest, m_ext_table, m_def_table,
                           m_embed_out, m_embed_etype, m_embed_vtype);
                sub.m_embed_stack = stack;
                entry["ast"] = variant(sub.parse());
                if (sub.has_error()) m_has_error = true;
                m_embed_out->modules[key] = variant(std::move(entry));
            }
            stack->pop_back();
            return key;
        }

        varvec parser::parse_def_stmt() {
            advance();
            std::string name_target = parse_target_name();
            consume(T_LP, "(");

            varvec params = parse_paramlist();

            consume(T_RP, ")");
            m_func_stack.push_back(name_target);
            bool saved_in_def_body = m_in_def_body;
            m_in_def_body = true;
            varvec body = parse_block();
            m_in_def_body = saved_in_def_body;
            m_func_stack.pop_back();

            varvec param_names;
            varmap defaults_map;
            for (size_t i = 0; i < params.size(); i++) {
                auto& p = params[i].to<varvec>();

                std::string pname = p[0].to<std::string>();
                param_names.push_back(variant(pname));

                if (!p[1].null()) defaults_map[pname] = p[1];
            }

            return make_node(O_DEF, {std::move(name_target),
                                     variant(std::move(param_names)),
                                     variant(std::move(defaults_map)),
                                     variant(std::move(body))});
        }

        varvec parser::parse_paramlist() {
            varvec params;
            if (check(T_RP)) return params;

            do {
                bytes_view pname = text();
                consume(T_NAME, "parameter name");
                varvec param;
                param.push_back(variant(pname.to_string()));
                if (match(T_ASS)) {
                    param.push_back(variant(parse_ternary()));
                } else {
                    param.push_back(variant());
                }
                params.push_back(variant(std::move(param)));
            } while (match(T_COMMA));

            return params;
        }

        varvec parser::parse_expr() {
            varvec left = parse_assign();
            while (match(T_COMMA)) {
                varvec right = parse_assign();
                left = make_binary(O_COMMA, left, right);
            }
            return left;
        }

        // returns bytes consumed; 0 = invalid escape, which the caller reports and skips by one
        static size_t unescape_one(const char* _p, size_t _len, std::string& _out) {
            if (_len < 2) {
                _out += '\\';
                return 1;
            }
            auto hexval = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                return -1;
            };
            switch (_p[1]) {
            case 'n': _out += '\n'; return 2;
            case 't': _out += '\t'; return 2;
            case 'r': _out += '\r'; return 2;
            case '\\': _out += '\\'; return 2;
            case '"': _out += '"'; return 2;
            case '0': _out += '\0'; return 2;
            case 'x': {
                if (_len < 4) return 0;
                int v1 = hexval(_p[2]), v2 = hexval(_p[3]);
                if (v1 < 0 || v2 < 0) return 0;
                _out += static_cast<char>((v1 << 4) | v2);
                return 4;
            }
            default:
                // an unknown escape keeps the character and swallows the backslash
                _out += _p[1];
                return 2;
            }
        }

        varvec parser::parse_assign() {
            NestGuard ng(*this);
            if (!ng.ok) return {};
            varvec left = parse_ternary();

            if (match(T_ASS)) {
                varvec rhs = parse_assign();
                return make_node(O_STORE, {variant(std::move(left)), variant(std::move(rhs))});
            }

            static const struct {
                tk_enum tok;
                op_enum op;
                const char* txt;
            } assign_ops[] = {
                {T_ASS_ADD, O_ASS_ADD, "+="},
                {T_ASS_MINUS, O_ASS_SUB, "-="},
                {T_ASS_MUL, O_ASS_MUL, "*="},
                {T_ASS_DIV, O_ASS_DIV, "/="},
                {T_ASS_MOD, O_ASS_MOD, "%="},
                {T_ASS_LSHIFT, O_ASS_LSHIFT, "<<="},
                {T_ASS_RSHIFT, O_ASS_RSHIFT, ">>="},
                {T_ASS_BIT_AND, O_ASS_BIT_AND, "&="},
                {T_ASS_BIT_OR, O_ASS_BIT_OR, "|="},
                {T_ASS_BIT_XOR, O_ASS_BIT_XOR, "^="},
            };

            for (auto& op : assign_ops) {
                if (match(op.tok)) {
                    // these tokens cannot start an expression: report here, do not let it cascade
                    switch (peek()) {
                    case T_SEMICOLON:
                    case T_RC:
                    case T_RP:
                    case T_RB:
                    case T_COMMA:
                    case T_COLON:
                    case T_END_OF_FILE:
                        error(std::string("Expected expression after '") + op.txt + "'");
                        return left;
                    default: break;
                    }
                    varvec rhs = parse_assign();
                    return make_node(op.op, {variant(std::move(left)), variant(std::move(rhs))});
                }
            }

            return left;
        }

        varvec parser::parse_ternary() {
            NestGuard ng(*this);
            if (!ng.ok) return {};
            varvec left = parse_or();
            if (match(T_QUESTION)) {
                varvec conseq = parse_assign();
                consume(T_COLON, ":");
                varvec altern = parse_ternary();
                varvec v;
                v.push_back(variant(OPTYPE(O_TERNARY)));
                v.push_back(variant(std::move(left)));
                v.push_back(variant(std::move(conseq)));
                v.push_back(variant(std::move(altern)));
                return v;
            }
            return left;
        }

        varvec parser::parse_or() {
            varvec left = parse_and();
            while (match(T_OR)) {
                varvec right = parse_and();
                left = make_binary(O_OR, left, right);
            }
            return left;
        }

        varvec parser::parse_and() {
            varvec left = parse_bit_or();
            while (match(T_AND)) {
                varvec right = parse_bit_or();
                left = make_binary(O_AND, left, right);
            }
            return left;
        }

        varvec parser::parse_bit_or() {
            varvec left = parse_bit_xor();
            while (match(T_BIT_OR)) {
                varvec right = parse_bit_xor();
                left = make_binary(O_BIT_OR, left, right);
            }
            return left;
        }

        varvec parser::parse_bit_xor() {
            varvec left = parse_bit_and();
            while (match(T_BIT_XOR)) {
                varvec right = parse_bit_and();
                left = make_binary(O_BIT_XOR, left, right);
            }
            return left;
        }

        varvec parser::parse_bit_and() {
            varvec left = parse_equality();
            while (match(T_BIT_AND)) {
                varvec right = parse_equality();
                left = make_binary(O_BIT_AND, left, right);
            }
            return left;
        }

        varvec parser::parse_equality() {
            varvec left = parse_compare();
            while (check(T_EQ) || check(T_NE)) {
                tk_enum op = advance();
                varvec right = parse_compare();
                left = make_binary(op == T_EQ ? O_EQ : O_NE, left, right);
            }
            return left;
        }

        varvec parser::parse_compare() {
            varvec left = parse_shift();
            while (check(T_LT) || check(T_GT) || check(T_LE) || check(T_GE)) {
                tk_enum op = advance();
                varvec right = parse_shift();
                op_enum name = O_NOP;
                switch (op) {
                case T_LT: name = O_LT; break;
                case T_GT: name = O_GT; break;
                case T_LE: name = O_LE; break;
                case T_GE: name = O_GE; break;
                default: break;
                }
                left = make_binary(name, left, right);
            }
            return left;
        }

        varvec parser::parse_shift() {
            varvec left = parse_add();
            while (check(T_LSHIFT) || check(T_RSHIFT)) {
                tk_enum op = advance();
                varvec right = parse_add();
                left = make_binary(op == T_LSHIFT ? O_LSHIFT : O_RSHIFT, left, right);
            }
            return left;
        }

        varvec parser::parse_add() {
            varvec left = parse_mul();
            while (check(T_PLUS) || check(T_MINUS)) {
                tk_enum op = advance();
                varvec right = parse_mul();
                left = make_binary(op == T_PLUS ? O_ADD : O_SUB, left, right);
            }
            return left;
        }

        varvec parser::parse_mul() {
            varvec left = parse_unary();
            while (check(T_STAR) || check(T_SLASH) || check(T_PERCENT)) {
                tk_enum op = advance();
                varvec right = parse_unary();
                op_enum name = O_NOP;
                switch (op) {
                case T_STAR: name = O_MUL; break;
                case T_SLASH: name = O_DIV; break;
                case T_PERCENT: name = O_MOD; break;
                default: break;
                }
                left = make_binary(name, left, right);
            }
            return left;
        }

        varvec parser::parse_unary() {
            NestGuard ng(*this);
            if (!ng.ok) return {};
            if (check(T_AUTO_INC) || check(T_AUTO_DEC) || check(T_NOT) || check(T_BIT_NEG) || check(T_PLUS) || check(T_MINUS)) {
                tk_enum op = advance();
                varvec operand = parse_unary();
                op_enum name = O_NOP;
                switch (op) {
                case T_AUTO_INC: name = O_PRE_INC; break;
                case T_AUTO_DEC: name = O_PRE_DEC; break;
                case T_NOT: name = O_NOT; break;
                case T_BIT_NEG: name = O_BIT_NEG; break;
                case T_PLUS: name = O_UPLUS; break;
                case T_MINUS: name = O_UMINUS; break;
                default: break;
                }
                return make_unary(name, operand);
            }
            return parse_suffix();
        }

        varvec parser::parse_suffix() {
            NestGuard ng(*this);
            if (!ng.ok) return {};

            bool indirect = match(T_AT);

            if (check(T_DELETE)) {
                advance();
                varvec target = parse_suffix();
                if (target.size() >= 1 && target[0].is<OPTYPE>() &&
                    static_cast<op_enum>(target[0].to<OPTYPE>()) == O_LOAD)
                    return make_node(O_DEL, {variant(target[1].to<std::string>())});
                return make_node(O_DEL, {variant(std::move(target))});
            }

            varvec left = parse_primary();

            while (true) {
                if (check(T_LP)) {

                    bool is_op_call = !indirect && left.size() == 1 && left[0].is<OPTYPE>();
                    bool is_ncall = left.size() >= 2 &&
                                    left[0].is<OPTYPE>() &&
                                    static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT;
                    advance();
                    varvec args;
                    if (!check(T_RP)) {
                        do {
                            variant arg = parse_assign();

                            if (arg.is_vec() && !arg.to<varvec>().empty() &&
                                arg.to<varvec>()[0].is<OPTYPE>() &&
                                static_cast<op_enum>(arg.to<varvec>()[0].to<OPTYPE>()) == O_UNPACK &&
                                !check(T_RP))
                                error("spread [] only allowed at last position, expected ')'");
                            args.push_back(variant(std::move(arg)));
                        } while (match(T_COMMA));
                    }
                    consume(T_RP, ")");
                    if (indirect) {
                        if (is_ncall) {
                            error("@ requires a simple variable name, not a dot chain");
                            consume(T_SEMICOLON, ";");
                            return varvec();
                        }

                        varvec v;
                        v.push_back(variant(OPTYPE(O_ICALL)));
                        v.push_back(variant(std::move(left)));
                        for (auto& a : args) v.push_back(std::move(a));
                        left = std::move(v);
                        indirect = false;
                    } else if (is_op_call) {

                        varvec v;
                        v.push_back(variant(left[0].to<OPTYPE>()));
                        for (auto& a : args) v.push_back(std::move(a));
                        left = std::move(v);
                    } else if (is_ncall) {

                        varvec v;
                        v.push_back(variant(OPTYPE(O_NCALL)));
                        varvec keys;
                        for (size_t i = 1; i < left.size(); i++)
                            keys.push_back(std::move(left[i]));
                        v.push_back(variant(std::move(keys)));
                        for (auto& a : args) v.push_back(std::move(a));
                        left = std::move(v);
                    } else {

                        varvec v;
                        v.push_back(variant(OPTYPE(O_CALL)));
                        if (left.size() == 2 && left[0].is<OPTYPE>() &&
                            static_cast<op_enum>(left[0].to<OPTYPE>()) == O_LOAD &&
                            left[1].is<std::string>()) {
                            v.push_back(left[1]);
                        } else {
                            v.push_back(variant(std::move(left)));
                        }
                        for (auto& a : args) v.push_back(std::move(a));
                        left = std::move(v);
                    }
                } else if (match(T_LB)) {

                    if (match(T_RB)) {
                        if (indirect) {
                            left = make_node(O_ILOAD, {variant(std::move(left))});
                            indirect = false;
                        }
                        left = make_node(O_UNPACK, {variant(std::move(left))});
                        continue;
                    }
                    if (indirect) {
                        left = make_node(O_ILOAD, {variant(std::move(left))});
                        indirect = false;
                    }
                    bool in_dot = left.size() >= 2 && left[0].is<OPTYPE>() &&
                                  static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT;

                    // a comma form is a slice whatever the base is: the chain becomes its base and
                    // the result is a value, so the flat-chain scalar key rule does not apply
                    auto parse_slice_suffix = [&]() {
                        varvec from = parse_assign();
                        consume(T_COMMA, ",");
                        varvec to = parse_assign();
                        variant step;
                        if (match(T_COMMA)) {
                            varvec step_expr = parse_assign();
                            if (step_expr.size() == 1 && step_expr[0].is<int_64>())
                                step = step_expr[0];
                            else if (step_expr.size() == 1 && step_expr[0].null())
                                step = variant();

                            else if (step_expr.size() == 2 && step_expr[0].is<OPTYPE>() &&
                                     static_cast<op_enum>(step_expr[0].to<OPTYPE>()) == O_UMINUS &&
                                     step_expr[1].is_vec() && step_expr[1].to<varvec>().size() == 1 &&
                                     step_expr[1].to<varvec>()[0].is<int_64>())
                                step = variant(-step_expr[1].to<varvec>()[0].to<int_64>());
                            else
                                step = variant(std::move(step_expr));
                        } else {
                            step = variant(static_cast<int_64>(1));
                        }
                        consume(T_RB, "]");
                        // [O_SLICE, base, from, to, step]; literal int/null bounds are inlined
                        varvec v;
                        v.push_back(variant(OPTYPE(O_SLICE)));
                        v.push_back(variant(std::move(left)));
                        auto store_slice_arg = [](varvec& _v, varvec& _expr) {
                            if (_expr.size() == 1 && _expr[0].is<int_64>())
                                _v.push_back(_expr[0]);
                            else if (_expr.size() == 1 && _expr[0].null())
                                _v.push_back(variant());

                            else if (_expr.size() == 2 && _expr[0].is<OPTYPE>() &&
                                     static_cast<op_enum>(_expr[0].to<OPTYPE>()) == O_UMINUS &&
                                     _expr[1].is_vec() && _expr[1].to<varvec>().size() == 1 &&
                                     _expr[1].to<varvec>()[0].is<int_64>())
                                _v.push_back(variant(-_expr[1].to<varvec>()[0].to<int_64>()));
                            else
                                _v.push_back(variant(std::move(_expr)));
                        };
                        store_slice_arg(v, from);
                        store_slice_arg(v, to);
                        v.push_back(std::move(step));
                        left = std::move(v);
                    };

                    if (in_dot && has_comma_in_brackets()) {
                        parse_slice_suffix();
                    } else if (in_dot) {
                        varvec idx = parse_expr();
                        consume(T_RB, "]");

                        // a flat chain holds scalar keys only: int, string, null, with -N folded in
                        if (idx.size() == 1 && idx[0].is<int_64>())
                            left.push_back(variant(idx[0].to<int_64>()));
                        else if (idx.size() == 1 && idx[0].is<std::string>())
                            left.push_back(idx[0]);
                        else if (idx.size() == 1 && idx[0].null())
                            left.push_back(variant());

                        else if (idx.size() == 2 && idx[0].is<OPTYPE>() &&
                                 static_cast<op_enum>(idx[0].to<OPTYPE>()) == O_UMINUS &&
                                 idx[1].is_vec() && idx[1].to<varvec>().size() == 1 &&
                                 idx[1].to<varvec>()[0].is<int_64>())
                            left.push_back(variant(-idx[1].to<varvec>()[0].to<int_64>()));
                        else
                            error("[] index in dot chain must be a literal integer, string, or null");
                    } else if (has_comma_in_brackets()) {
                        parse_slice_suffix();
                    } else {
                        varvec idx = parse_expr();
                        consume(T_RB, "]");
                        left = make_binary(O_INDEX, left, idx);
                    }
                } else if (match(T_DOT)) {

                    if (check(T_LP)) {

                        bool has_nav = left.size() >= 2 && left[0].is<OPTYPE>() &&
                                       static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT &&
                                       left[1].is<OPTYPE>() &&
                                       (static_cast<op_enum>(left[1].to<OPTYPE>()) == O_CURRENT ||
                                        static_cast<op_enum>(left[1].to<OPTYPE>()) == O_PARENT ||
                                        static_cast<op_enum>(left[1].to<OPTYPE>()) == O_ROOT);
                        if (!has_nav)
                            error("bridge .(...) requires a nav prefix (. .. ::)");
                        advance();
                        while (!check(T_RP) && !check(T_END_OF_FILE)) {
                            if (match(T_DOT)) {
                                continue;
                            } else if (match(T_LB)) {
                                varvec idx = parse_expr();
                                consume(T_RB, "]");

                                if (idx.size() == 1 && idx[0].is<int_64>())
                                    left.push_back(variant(idx[0].to<int_64>()));
                                else if (idx.size() == 1 && idx[0].is<std::string>())
                                    left.push_back(idx[0]);
                                else if (idx.size() == 1 && idx[0].null())
                                    left.push_back(variant());
                                else if (idx.size() == 2 && idx[0].is<OPTYPE>() &&
                                         static_cast<op_enum>(idx[0].to<OPTYPE>()) == O_UMINUS &&
                                         idx[1].is_vec() && idx[1].to<varvec>().size() == 1 &&
                                         idx[1].to<varvec>()[0].is<int_64>())
                                    left.push_back(variant(-idx[1].to<varvec>()[0].to<int_64>()));
                                else
                                    error("[] index in .() bridge must be a literal integer, string, or null");
                            } else if (check(T_NAME)) {
                                bytes_view name = text();
                                advance();
                                left.push_back(variant(std::string(name.to_string())));
                            } else {
                                break;
                            }
                        }
                        consume(T_RP, ")");
                        break;
                    }

                    bytes_view member = text();
                    consume(T_NAME, "member name");
                    std::string mem_str = member.to_string();
                    // flat chain [O_DOT, key0, key1, ...]: a leading O_LOAD is unwrapped to its name
                    if (left.size() >= 2 && left[0].is<OPTYPE>() &&
                        static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT) {

                        left.push_back(variant(std::move(mem_str)));
                    } else if (left.size() >= 2 && left[0].is<OPTYPE>() &&
                               static_cast<op_enum>(left[0].to<OPTYPE>()) == O_INDEX) {

                        varvec v;
                        v.push_back(variant(OPTYPE(O_DOT)));

                        auto& base = left[1].to<varvec>();
                        if (base.size() == 2 && base[0].is<OPTYPE>() &&
                            static_cast<op_enum>(base[0].to<OPTYPE>()) == O_LOAD &&
                            base[1].is<std::string>())
                            v.push_back(base[1]);
                        else
                            v.push_back(left[1]);

                        for (size_t k = 2; k < left.size(); k++) {
                            auto& idx = left[k].to<varvec>();
                            if (idx.size() == 1 && idx[0].is<int_64>())
                                v.push_back(variant(idx[0].to<int_64>()));
                            else if (idx.size() == 1 && idx[0].is<std::string>())
                                v.push_back(idx[0]);
                            else if (idx.size() == 1 && idx[0].null())
                                v.push_back(variant());
                            else
                                v.push_back(variant(std::move(idx)));
                        }
                        v.push_back(variant(std::move(mem_str)));
                        left = std::move(v);
                    } else {

                        varvec v;
                        v.push_back(variant(OPTYPE(O_DOT)));
                        if (left.size() == 2 && left[0].is<OPTYPE>() &&
                            static_cast<op_enum>(left[0].to<OPTYPE>()) == O_LOAD &&
                            left[1].is<std::string>()) {
                            v.push_back(left[1]);
                        } else {
                            v.push_back(variant(std::move(left)));
                        }
                        v.push_back(variant(std::move(mem_str)));
                        left = std::move(v);
                    }
                } else if (match(T_DOTDOT)) {

                    if (left.size() >= 2 && left[0].is<OPTYPE>() &&
                        static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT) {

                        left.push_back(variant(OPTYPE(O_PARENT)));
                    } else {

                        varvec v;
                        v.push_back(variant(OPTYPE(O_DOT)));

                        if (left.size() == 2 && left[0].is<OPTYPE>() &&
                            static_cast<op_enum>(left[0].to<OPTYPE>()) == O_LOAD &&
                            left[1].is<std::string>()) {
                            v.push_back(left[1]);
                        } else {
                            v.push_back(variant(std::move(left)));
                        }
                        v.push_back(variant(OPTYPE(O_PARENT)));
                        left = std::move(v);
                    }

                    while (match(T_DOTDOT))
                        left.push_back(variant(OPTYPE(O_PARENT)));
                } else if (match(T_AUTO_INC)) {
                    if (indirect) {
                        left = make_node(O_ILOAD, {variant(std::move(left))});
                        indirect = false;
                    }
                    left = make_unary(O_POST_INC, left);
                } else if (match(T_AUTO_DEC)) {
                    if (indirect) {
                        left = make_node(O_ILOAD, {variant(std::move(left))});
                        indirect = false;
                    }
                    left = make_unary(O_POST_DEC, left);
                } else if (check(T_NAME) && left.size() >= 2 &&
                           left[0].is<OPTYPE>() &&
                           static_cast<op_enum>(left[0].to<OPTYPE>()) == O_DOT) {

                    bytes_view name = text();
                    advance();
                    left.push_back(variant(std::string(name.to_string())));
                } else {
                    break;
                }
            }
            if (indirect) {

                varvec v;
                v.push_back(variant(OPTYPE(O_ILOAD)));
                v.push_back(variant(std::move(left)));
                return v;
            }
            return left;
        }

        varvec parser::parse_primary() {
            switch (peek()) {
            case T_INT_LITERAL: {
                bytes_view tv = text();
                advance();
                int_64 val = 0;
                try {
                    val = std::stoll(tv.to_string());
                } catch (const std::out_of_range&) {
                    error("integer literal overflow");
                    val = max_int_64;
                } catch (const std::invalid_argument&) {
                    error("invalid integer literal");
                }
                varvec v;
                v.push_back(variant(val));
                return v;
            }
            case T_HEX_LITERAL: {
                bytes_view tv = text();
                advance();
                int_64 val = 0;
                try {
                    val = std::stoll(tv.to_string().substr(2), nullptr, 16);
                } catch (const std::out_of_range&) {
                    error("hex literal overflow");
                    val = max_int_64;
                } catch (const std::invalid_argument&) {
                    error("invalid hex literal");
                }
                varvec v;
                v.push_back(variant(val));
                return v;
            }
            case T_OCT_LITERAL: {
                bytes_view tv = text();
                advance();
                int_64 val = 0;
                try {
                    val = std::stoll(tv.to_string().substr(2), nullptr, 8);
                } catch (const std::out_of_range&) {
                    error("octal literal overflow");
                    val = max_int_64;
                } catch (const std::invalid_argument&) {
                    error("invalid octal literal");
                }
                varvec v;
                v.push_back(variant(val));
                return v;
            }
            case T_BIN_LITERAL: {
                bytes_view tv = text();
                advance();
                int_64 val = 0;
                try {
                    val = std::stoll(tv.to_string().substr(2), nullptr, 2);
                } catch (const std::out_of_range&) {
                    error("binary literal overflow");
                    val = max_int_64;
                } catch (const std::invalid_argument&) {
                    error("invalid binary literal");
                }
                varvec v;
                v.push_back(variant(val));
                return v;
            }
            case T_FLOAT_LITERAL: {
                bytes_view tv = text();
                advance();
                std::string s = tv.to_string();
                double val = 0.0;
                try {
                    size_t pos = 0;
                    val = std::stod(s, &pos);
                    if (pos != s.size())
                        throw std::invalid_argument("partial parse");
                } catch (const std::out_of_range&) {
                    error("float literal overflow");
                } catch (const std::invalid_argument&) {
                    error("invalid float literal");
                }
                varvec v;
                v.push_back(variant(val));
                return v;
            }
            case T_STRING_LITERAL: {
                size_t t_idx = index();
                bytes_view tv = text();
                advance();
                std::string s = tv.to_string();
                s = s.substr(1, s.size() - 2);

                const auto& escs = m_tokens.esc(t_idx);
                std::string result;
                size_t mark = 0;
                for (size_t i = 0; i < s.size();) {
                    if (mark < escs.size() && i + 1 == escs[mark]) {
                        size_t used = unescape_one(s.data() + i, s.size() - i, result);
                        if (used == 0) {
                            error("invalid escape sequence");
                            used = 1;
                        }
                        i += used;
                        mark++;
                    } else {
                        result += s[i++];
                    }
                }
                varvec v;
                v.push_back(variant(result));
                return v;
            }
            case T_BACKTICK_STRING: {
                bytes_view tv = text();
                advance();
                std::string s = tv.to_string();
                s = s.substr(1, s.size() - 2);
                varvec v;
                v.push_back(variant(s));
                return v;
            }
            case T_TRUE:
                advance();
                {
                    varvec v;
                    v.push_back(variant(true));
                    return v;
                }
            case T_FALSE:
                advance();
                {
                    varvec v;
                    v.push_back(variant(false));
                    return v;
                }
            case T_NULL_:
                advance();
                {
                    varvec v;
                    v.push_back(variant());
                    return v;
                }
            case T_NAN:
                advance();
                {
                    varvec v;
                    v.push_back(variant(std::numeric_limits<double>::quiet_NaN()));
                    return v;
                }
            case T_INF:
                advance();
                {
                    varvec v;
                    v.push_back(variant(std::numeric_limits<double>::infinity()));
                    return v;
                }
            case T_INT:
            case T_FLOAT:
            case T_STRING:
            case T_BOOL:
            case T_BYTES:
            case T_TYPE:
            case T_ENV: {

                std::string name = text().to_string();
                advance();
                if (!check(T_LP))
                    error("'" + name + "' is a op function, cannot be used as a variable or value");
                varvec v;

                op_enum o = O_NOP;
                if (name == "int") o = O_INT;
                else if (name == "float") o = O_FLOAT;
                else if (name == "string") o = O_STRING;
                else if (name == "bool") o = O_BOOL;
                else if (name == "bytes") o = O_BYTES;
                else if (name == "type") o = O_TYPE;
                else if (name == "env") o = O_ENV;
                if (o == O_ENV && m_embed_out)
                    error("env() is not allowed in embed mode");
                v.push_back(variant(OPTYPE(o)));
                return v;
            }
            case T_HERE: {

                uint_32 r = row();
                uint_32 c = col();
                size_t idx = index();
                uint_32 o = (idx < m_tokens.count()) ? m_tokens.ofst(idx) : 0;
                advance();
                if (!check(T_LP)) {
                    error("'here' is a op function, cannot be used as a variable or value");

                    varvec v;
                    v.push_back(variant(OPTYPE(O_HERE)));
                    v.push_back(variant(varmap()));
                    return v;
                }
                advance();
                consume(T_RP, ")");
                varmap m;
                m["row"] = variant(static_cast<int_64>(r));
                m["col"] = variant(static_cast<int_64>(c));
                m["ofst"] = variant(static_cast<int_64>(o));

                m["file"] = variant(m_file_path.empty() ? std::string()
                                                        : file_info(m_file_path).path());
                varvec v;
                v.push_back(variant(OPTYPE(O_HERE)));
                v.push_back(variant(std::move(m)));
                return v;
            }
            case T_TRAP: {

                uint_32 r = row();
                uint_32 c = col();
                size_t idx = index();
                uint_32 o = (idx < m_tokens.count()) ? m_tokens.ofst(idx) : 0;
                advance();
                if (!check(T_LP)) {
                    error("'trap' is a op function, cannot be used as a variable or value");
                    varvec v;
                    v.push_back(variant(OPTYPE(O_TRAP)));
                    v.push_back(variant(varmap()));
                    return v;
                }
                advance();
                varmap m;
                m["row"] = variant(static_cast<int_64>(r));
                m["col"] = variant(static_cast<int_64>(c));
                m["ofst"] = variant(static_cast<int_64>(o));
                m["file"] = variant(m_file_path.empty() ? std::string()
                                                        : file_info(m_file_path).path());
                varvec v;
                v.push_back(variant(OPTYPE(O_TRAP)));
                v.push_back(variant(std::move(m)));
                if (!check(T_RP)) {
                    v.push_back(parse_assign());
                    if (match(T_COMMA)) {
                        v.push_back(parse_assign());
                        if (!check(T_RP)) {
                            error("trap: at most 2 arguments");
                            while (!check(T_RP) && !check(T_END_OF_FILE)) advance();
                        }
                    }
                }
                consume(T_RP, ")");
                return v;
            }
            case T_EVAL: {

                advance();
                if (!check(T_LP)) {
                    error("'eval' is a op function, cannot be used as a variable or value");
                    varvec v;
                    v.push_back(variant(OPTYPE(O_EVAL)));
                    return v;
                }
                varvec v;
                v.push_back(variant(OPTYPE(O_EVAL)));
                return v;
            }
            case T_VEC: {
                if (peek(1) == T_LB) {
                    advance();
                    return parse_seq_lit(O_VEC);
                }
                goto treat_as_op_call;
            }
            case T_MAP: {
                if (peek(1) == T_LC) {
                    advance();
                    return parse_dict_lit();
                }
                goto treat_as_op_call;
            }
            case T_LST: {
                if (peek(1) == T_LB) {
                    advance();
                    return parse_seq_lit(O_LST);
                }
                goto treat_as_op_call;
            }
            treat_as_op_call:
                {

                    std::string name = text().to_string();
                    advance();
                    if (!check(T_LP))
                        error("'" + name + "' is a op function, cannot be used as a variable or value");
                    varvec v;
                    op_enum o = O_NOP;
                    if (name == "vec") o = O_VEC;
                    else if (name == "map") o = O_MAP;
                    else if (name == "lst") o = O_LST;
                    v.push_back(variant(OPTYPE(o)));
                    return v;
                }
            case T_NAME: {
                bytes_view name = text();
                advance();
                return make_name_ref(name);
            }
            case T_DOLLAR: {

                advance();
                tk_enum nt = peek();
                if (nt != T_NAME && !(nt >= T_VAR && nt <= T_EVAL)) {
                    error("extension function name after $");
                    synchronize();
                    return varvec();
                }
                bytes_view name_bv = text();
                std::string func_name = name_bv.to_string();
                advance();

                if (!check(T_LP)) {
                    if (m_def_table) {
                        auto it = m_def_table->find(func_name);
                        if (it != m_def_table->end()) {
                            varvec node;
                            node.push_back(it->second);
                            return node;
                        }
                        if (m_ext_table && m_ext_table->find(func_name) != m_ext_table->end()) {
                            error("extension function cannot be read as value: $" + func_name);
                            synchronize();
                            return varvec();
                        }
                        error("undefined static definition: $" + func_name);
                        synchronize();
                        return varvec();
                    }

                    // null def_table means host state is unknown (AST dump): emit it as O_ECONST
                    varvec node;
                    node.push_back(variant(OPTYPE(O_ECONST)));
                    node.push_back(variant(func_name));
                    return node;
                }

                if (m_ext_table && m_ext_table->find(func_name) == m_ext_table->end()) {
                    error("undefined extension function: $" + func_name);
                    synchronize();
                    return varvec();
                }
                consume(T_LP, "(");

                varvec node;
                node.push_back(variant(OPTYPE(O_EXCALL)));
                node.push_back(variant(std::move(func_name)));

                if (!check(T_RP)) {
                    do {
                        node.push_back(variant(parse_assign()));
                    } while (match(T_COMMA));
                }
                consume(T_RP, ")");
                return node;
            }

            case T_DOTDOT: {
                varvec dot;
                dot.push_back(variant(OPTYPE(O_DOT)));
                advance();
                dot.push_back(variant(OPTYPE(O_PARENT)));
                while (match(T_DOTDOT))
                    dot.push_back(variant(OPTYPE(O_PARENT)));
                if (!check(T_NAME) && !check(T_DOT) && !check(T_LP))
                    error(".. must be followed by a key or call");
                return dot;
            }
            case T_COLONCOLON: {
                advance();
                varvec dot;
                dot.push_back(variant(OPTYPE(O_DOT)));
                dot.push_back(variant(OPTYPE(O_ROOT)));
                if (!check(T_NAME) && !check(T_DOT) && !check(T_LP))
                    error(":: must be followed by a key or call");
                return dot;
            }
            case T_DOT: {

                if (peek(1) != T_NAME && peek(1) != T_LP) {
                    error(". must be followed by a key or call");
                    advance();
                    return varvec();
                }
                advance();
                varvec dot;
                dot.push_back(variant(OPTYPE(O_DOT)));
                dot.push_back(variant(OPTYPE(O_CURRENT)));

                return dot;
            }
            case T_LB:
                return parse_seq_lit(O_VEC);
            case T_LP: {
                NestGuard ng(*this);
                if (!ng.ok) return varvec();
                advance();
                varvec e = parse_expr();
                consume(T_RP, ")");
                return e;
            }
            default:
                error("Unexpected token");
                synchronize();
                {
                    varvec v;
                    return v;
                }
            }
        }

        varvec parser::parse_seq_lit(op_enum _node_type) {
            NestGuard ng(*this);
            if (!ng.ok) return make_node(_node_type);
            advance();
            varvec elems;
            variant fill_val;
            variant cnt;
            if (!check(T_RB)) {
                if (!check(T_COLON)) {
                    varvec first = parse_ternary();
                    if (match(T_COLON)) {

                        fill_val = variant(std::move(first));
                        cnt = variant(parse_expr());
                    } else {
                        elems.push_back(variant(std::move(first)));
                        while (match(T_COMMA)) {
                            elems.push_back(variant(parse_ternary()));
                        }
                        if (match(T_COLON)) {
                            cnt = variant(parse_expr());
                        }
                    }
                } else {

                    advance();
                    cnt = variant(parse_expr());
                }
            }

            consume(T_RB, "]");

            // [tag, elems, fill_val, cnt] - four slots, read positionally by the walker
            // elems empty: fill_val repeated cnt times; elems present with cnt: last elem cnt times
            varvec result;
            result.push_back(variant(OPTYPE(_node_type)));
            result.push_back(variant(std::move(elems)));
            result.push_back(std::move(fill_val));
            result.push_back(std::move(cnt));
            return result;
        }

        varvec parser::parse_dict_lit() {
            NestGuard ng(*this);
            if (!ng.ok) return make_node(O_MAP);
            advance();

            if (check(T_RC)) {
                advance();
                varvec v;
                v.push_back(variant(OPTYPE(O_MAP)));
                return v;
            }

            varvec items;
            do {
                varvec key = parse_ternary();
                consume(T_COLON, ":");
                varvec val = parse_ternary();
                items.push_back(variant(std::move(key)));
                items.push_back(variant(std::move(val)));
            } while (match(T_COMMA));

            consume(T_RC, "}");

            varvec v;
            v.push_back(variant(OPTYPE(O_MAP)));
            for (auto& item : items) v.push_back(std::move(item));
            return v;
        }

    }
}
