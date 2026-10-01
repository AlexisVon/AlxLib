/*****************************************************************/ /**
 * \file   ascript_parse.h
 * \brief  Script parser — recursive-descent LL with precedence climbing
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_PARSE_H_
#define _ALEXIS_SCRIPT_PARSE_H_

#include "ascript.h"
#include "ascript_compile.h"
#include "ascript_lex.h"
#include "avariant.h"
#include <functional>
#include <list>
#include <string>
#include <unordered_set>
#include <vector>

namespace alx {
    namespace script {

        // A rule that fails returns an empty varvec (the error-recovery sentinel); callers skip it.
        class parser {
            friend struct NestGuard;

        public:

            // Non-null _ext_table/_def_table enable compile-time name checks (null = host extensions
            // unknown, so $name without parens stays a runtime O_ECONST); _max_nest 0 = no depth limit.
            // _embed_out non-null selects embed mode: import is resolved and recursed here, link/env error.
            parser(const token_list& _tokens,
                   const alx::signal<const compile_error&>* _on_error = nullptr,
                   const std::string& _file_path = std::string(),
                   const std::list<std::string>& _search_paths = {},
                   size_t _max_nest = 1024,
                   const std::unordered_set<std::string>* _ext_table = nullptr,
                   const std::unordered_map<std::string, variant>* _def_table = nullptr,
                   compile_result* _embed_out = nullptr,
                   const std::string& _embed_etype = std::string(),
                   uint_64 _embed_vtype = 0,
                   bool _debug = false);

            // Returns {} when the token list holds any lexer error; otherwise an O_PROGRAM-wrapped forest.
            varvec parse();

            // Bare statement list, no O_PROGRAM wrapper and no lexer-error scan: the eval runtime entry.
            varvec parse_body();

            static varvec parse(const bytes_view& _source, const std::string& _file_path = std::string(),
                                const std::list<std::string>& _search_paths = {},
                                const std::unordered_set<std::string>* _ext_table = nullptr,
                                const std::unordered_map<std::string, variant>* _def_table = nullptr,
                                size_t _max_nest = 1024,
                                bool _debug = false,
                                const alx::signal<const compile_error&>* _on_error = nullptr);

            static varvec parse_body(const bytes_view& _source, const std::string& _file_path = std::string(),
                                     const std::list<std::string>& _search_paths = {},
                                     const std::unordered_set<std::string>* _ext_table = nullptr,
                                     const std::unordered_map<std::string, variant>* _def_table = nullptr,
                                     bool _debug = false);

            bool has_error() const { return m_has_error; }

        private:
            const token_list& m_tokens;
            std::string m_file_path;
            std::list<std::string> m_search_paths;
            const alx::signal<const compile_error&>* m_on_error = nullptr;
            size_t m_pos = 0;
            bool m_has_error = false;
            // Set by NestGuard on the depth limit; error() then stops reporting (later ones are cascade).
            bool m_fatal = false;
            size_t m_nest_depth = 0;
            size_t m_max_nest = 1024;
            int m_loop_depth = 0;
            int m_switch_depth = 0;
            bool m_in_def_body = false;
            bool m_debug = false;

            // Enclosing def names; parse_return_stmt turns "return <this def>(...)" into O_TCALL (TCO).
            std::vector<std::string> m_func_stack;
            const std::unordered_set<std::string>* m_ext_table = nullptr;
            const std::unordered_map<std::string, variant>* m_def_table = nullptr;
            compile_result* m_embed_out = nullptr;
            // Circular-import chain: null on the root parser, shared by the nested parsers it spawns.
            std::vector<std::string>* m_embed_stack = nullptr;
            std::string m_embed_etype;
            uint_64 m_embed_vtype = 0;

            // m_pos is a raw token index; index() is the current token with T_ERROR/T_COMMENT skipped.
            size_t index() const;
            tk_enum peek(size_t _ahead = 0) const;
            tk_enum advance();
            bool check(tk_enum t) const;
            bool match(tk_enum t);
            bool consume(tk_enum t, const char* expected);
            bytes_view text(size_t _ahead = 0) const;
            uint_32 row() const;
            uint_32 col() const;
            varvec pos_node() const;

            void error(const std::string& _msg);
            void synchronize();

            // Lookahead only, '[' already consumed: true if a top-level ',' follows, cursor restored.
            bool has_comma_in_brackets();
            varvec parse_program();
            varvec parse_top_stmt();
            varvec parse_stmt();
            varvec parse_block();
            varvec parse_stmt_or_block();
            std::string parse_target_name();
            varvec parse_var_names();
            varvec parse_var_decl();
            varvec parse_typed_var_decl();
            varvec parse_if_stmt();
            varvec parse_while_stmt();
            varvec parse_for_stmt();
            varvec parse_for_head();
            varvec parse_switch_stmt();
            varvec parse_try_stmt();
            varvec parse_return_stmt();
            varvec parse_break_stmt();
            varvec parse_continue_stmt();
            varvec parse_throw_stmt();
            varvec parse_import_stmt();
            varvec parse_link_stmt();
            // Embed mode: parses _rel into m_embed_out under an "@<hash>" key and returns that key.
            std::string embed_resolve(const std::string& _rel);
            bool check_embedded_axp(const varmap& _vm, const std::string& _rel);
            varvec parse_def_stmt();
            varvec parse_expr_stmt();

            // Expression ladder: one function per precedence level, loosest binding first, as ordered below.
            varvec parse_expr();
            varvec parse_assign();
            varvec parse_ternary();
            varvec parse_or();
            varvec parse_and();
            varvec parse_bit_or();
            varvec parse_bit_xor();
            varvec parse_bit_and();
            varvec parse_equality();
            varvec parse_compare();
            varvec parse_shift();
            varvec parse_add();
            varvec parse_mul();
            varvec parse_unary();
            varvec parse_suffix();
            varvec parse_primary();

            varvec parse_arglist();
            varvec parse_paramlist();
            varvec parse_seq_lit(op_enum _node_type);
            varvec parse_dict_lit();
            varvec parse_assign_or_expr_stmt();
            varvec make_binary(op_enum _op, varvec& _left, varvec& _right);
            varvec make_unary(op_enum _op, varvec& _operand);
            varvec make_name_ref(const bytes_view& _name);
        };

    }
}

#endif
