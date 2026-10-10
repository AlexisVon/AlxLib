/*****************************************************************/ /**
 * \file   ascript_walk.cpp
 * \brief  Script walk executor — tree-walk interpreter + all ops
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_walk.h"

#include <new>
#include <stdexcept>
#include "afile.h"
#include "ascript.h"
#include "ascript_compile.h"
#include "ascript_dot.h"
#include "ascript_fwrap.h"
#include "ascript_modmng.h"
#include "ascript_parse.h"
#include "ascript_trace.h"
#include "ascript_utils.h"
#include "astring.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

namespace alx {
    namespace script {
        namespace {

            // Wrapping arithmetic: callers with overflow_check on throw first, plain signed ops would be UB
            int_64 wrap_add(int_64 _a, int_64 _b) { return (int_64) ((uint_64) _a + (uint_64) _b); }
            int_64 wrap_sub(int_64 _a, int_64 _b) { return (int_64) ((uint_64) _a - (uint_64) _b); }
            int_64 wrap_mul(int_64 _a, int_64 _b) { return (int_64) ((uint_64) _a * (uint_64) _b); }
            int_64 wrap_neg(int_64 _a) { return (int_64) (0 - (uint_64) _a); }
            int_64 wrap_shl(int_64 _a, int_64 _b) { return (int_64) ((uint_64) _a << (_b & 63)); }
            int_64 wrap_shr(int_64 _a, int_64 _b) { return _a >> (_b & 63); }

            // A byte container's element value: the ordinary integer conversion, then the 0-255
            // write-back policy -- wrap by default, an overflow error when the config asks
            int_64 byte_value(const variant& _val, bool _overflow_check) {
                int_64 v = cov_int(_val);
                if (!_overflow_check) return v & 0xFF;
                if (v < 0 || v > 255)
                    throw script_exception{error_type::OverflowError,
                                           std::string("Byte value out of range (0-255): ") + std::to_string(v)};
                return v;
            }

            // The position a byte write lands on: the index folds [-1] and is checked first, so an
            // out-of-range position reports before the value is looked at
            uint_64 byte_index(variant& _parent, const variant& _key) {
                bool is_str = _parent.is<std::string>();
                uint_64 size = is_str ? _parent.as<std::string>().size() : _parent.as<bytes>().size();
                int_64 i = cov_int(_key);
                if (i == -1) i = static_cast<int_64>(size) - 1;
                if (i < 0 || static_cast<uint_64>(i) >= size)
                    throw script_exception{error_type::IndexError,
                                           std::string(is_str ? "string index out of range" : "bytes index out of range")};
                return static_cast<uint_64>(i);
            }

            // Append one byte: the value is converted by the caller, so a refusal leaves no byte behind
            void byte_append(variant& _parent, int_64 _byte) {
                if (_parent.is<std::string>()) {
                    _parent.as<std::string>().push_back(static_cast<char>(_byte));
                    return;
                }
                uint_8 b = static_cast<uint_8>(_byte);
                _parent.as<bytes>().append(&b, 1);
            }

            void byte_put(variant& _parent, uint_64 _i, int_64 _byte) {
                if (_parent.is<std::string>()) {
                    _parent.as<std::string>()[_i] = static_cast<char>(_byte);
                    return;
                }
                _parent.as<bytes>()[_i] = static_cast<uint_8>(_byte);
            }

            // The number of positions a slice visits; the read walk and the write scatter share it
            int_64 slice_count(int_64 _from, int_64 _to, int_64 _step) {
                if (_step > 0) return _from >= _to ? 0 : (_to - _from + _step - 1) / _step;
                return _from <= _to ? 0 : (_from - _to - _step - 1) / (-_step);
            }

            // The slice-assignment messages, shared by every container branch
            std::string slice_length_error(int_64 _want, uint_64 _got) {
                return "slice assignment length mismatch: " + std::to_string(_want) + " positions, " +
                       std::to_string(_got) + " values";
            }

            std::string slice_type_error(const char* _want, const variant& _got) {
                return std::string("slice assignment expects ") + _want + ", got " + type_name_script(_got);
            }

            // Read-side element access, shared by every level of an index chain: [i] / [-1] / [null] /
            // key. A string or bytes element reads as its byte value (0-255)
            variant index_value(const variant& _obj, const variant& _idx) {
                if (_obj.is<varvec>()) {
                    auto& vec = _obj.to<varvec>();
                    if (_idx.null()) return variant(static_cast<int_64>(vec.size()));
                    int_64 i = cov_int(_idx);
                    if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
                    if (i >= 0 && static_cast<uint_64>(i) < vec.size())
                        return vec[static_cast<size_t>(i)];
                    throw script_exception{error_type::IndexError, std::string("vec index out of range")};
                }
                if (_obj.is<varlst>()) {
                    auto& lst = _obj.to<varlst>();
                    if (_idx.null()) return variant(static_cast<int_64>(lst.size()));
                    int_64 i = cov_int(_idx);
                    if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
                    if (i < 0 || static_cast<uint_64>(i) >= lst.size())
                        throw script_exception{error_type::IndexError, std::string("lst index out of range")};
                    auto it = lst.begin();
                    for (int_64 n = 0; n < i; ++n) ++it;
                    return *it;
                }
                if (_obj.is<varmap>()) {
                    auto& m = _obj.to<varmap>();
                    if (_idx.null()) return variant(static_cast<int_64>(m.size()));
                    if (!_idx.is<std::string>())
                        throw script_exception{error_type::TypeError, std::string("map key must be a string")};
                    std::string key = _idx.to<std::string>();
                    if (m.contain(key)) return m.value(key);
                    throw script_exception{error_type::KeyError, std::string("map key not found: " + key)};
                }
                if (_obj.is<std::string>()) {
                    auto& s = _obj.to<std::string>();
                    if (_idx.null()) return variant(static_cast<int_64>(s.size()));
                    int_64 i = cov_int(_idx);
                    if (i == -1) i = static_cast<int_64>(s.size()) - 1;
                    if (i >= 0 && static_cast<uint_64>(i) < s.size())
                        return variant(static_cast<int_64>(static_cast<uint_8>(s[static_cast<size_t>(i)])));
                    throw script_exception{error_type::IndexError, std::string("string index out of range")};
                }
                if (_obj.is<bytes>()) {
                    auto& b = _obj.to<bytes>();
                    if (_idx.null()) return variant(static_cast<int_64>(b.size()));
                    int_64 i = cov_int(_idx);
                    if (i == -1) i = static_cast<int_64>(b.size()) - 1;
                    if (i >= 0 && static_cast<uint_64>(i) < b.size())
                        return variant(static_cast<int_64>(b[static_cast<uint_64>(i)]));
                    throw script_exception{error_type::IndexError, std::string("bytes index out of range")};
                }
                throw script_exception{error_type::TypeError, std::string("Type does not support [i] index")};
            }
        }

        op_table walker::s_ops = {};

        walker::walker(const engine_config& _cfg) : m_cfg(_cfg) { init(); }
        walker::~walker() = default;

        void walker::init() {
            // magic static: the language makes this initialisation thread-safe, so a second walker
            // built on another thread cannot walk the table while it is half filled
            static const bool once = [] { fill_ops(); return true; }();
            (void) once;
        }

        void walker::fill_ops() {
            s_ops[O_NOP] = op_nop;
            s_ops[O_PROGRAM] = op_program;
            s_ops[O_BLOCK] = op_block;
            s_ops[O_LOAD] = op_load;
            s_ops[O_ILOAD] = op_iload;
            s_ops[O_VAR] = op_var;
            s_ops[O_STORE] = op_store;
            s_ops[O_RETURN] = op_return;
            s_ops[O_CALL] = op_call;
            s_ops[O_NCALL] = op_ncall;
            s_ops[O_ICALL] = op_icall;
            s_ops[O_TCALL] = op_tcall;
            s_ops[O_DEF] = op_def;
            s_ops[O_IF] = op_if;
            s_ops[O_WHILE] = op_while;
            s_ops[O_FOR] = op_for;
            s_ops[O_FOREACH] = op_foreach;
            s_ops[O_BREAK] = op_break;
            s_ops[O_CONTINUE] = op_continue;
            s_ops[O_SWITCH] = op_switch;
            s_ops[O_TRY] = op_try;
            s_ops[O_THROW] = op_throw;
            s_ops[O_INT] = op_int;
            s_ops[O_FLOAT] = op_float;
            s_ops[O_STRING] = op_string;
            s_ops[O_BOOL] = op_bool;
            s_ops[O_BYTES] = op_bytes;
            s_ops[O_VEC] = op_vec;
            s_ops[O_MAP] = op_map;

            s_ops[O_LST] = op_lst;
            s_ops[O_TYPE] = op_type;
            s_ops[O_ENV] = op_env;
            s_ops[O_IMPORT] = op_import;
            s_ops[O_LINK] = op_link;
            s_ops[O_UPLUS] = op_uplus;
            s_ops[O_UMINUS] = op_uminus;
            s_ops[O_ADD] = op_add;
            s_ops[O_SUB] = op_sub;
            s_ops[O_MUL] = op_mul;
            s_ops[O_DIV] = op_div;
            s_ops[O_MOD] = op_mod;
            s_ops[O_PRE_INC] = op_pre_inc;
            s_ops[O_PRE_DEC] = op_pre_dec;
            s_ops[O_POST_INC] = op_post_inc;
            s_ops[O_POST_DEC] = op_post_dec;
            s_ops[O_LSHIFT] = op_lshift;
            s_ops[O_RSHIFT] = op_rshift;
            s_ops[O_EQ] = op_eq;
            s_ops[O_NE] = op_ne;
            s_ops[O_LT] = op_lt;
            s_ops[O_GT] = op_gt;
            s_ops[O_LE] = op_le;
            s_ops[O_GE] = op_ge;
            s_ops[O_BIT_AND] = op_bit_and;
            s_ops[O_BIT_OR] = op_bit_or;
            s_ops[O_BIT_XOR] = op_bit_xor;
            s_ops[O_BIT_NEG] = op_bit_neg;
            s_ops[O_AND] = op_and;
            s_ops[O_OR] = op_or;
            s_ops[O_NOT] = op_not;
            s_ops[O_TERNARY] = op_ternary;
            s_ops[O_COMMA] = op_comma;
            s_ops[O_ASS_ADD] = op_ass_add;
            s_ops[O_ASS_SUB] = op_ass_sub;
            s_ops[O_ASS_MUL] = op_ass_mul;
            s_ops[O_ASS_DIV] = op_ass_div;
            s_ops[O_ASS_MOD] = op_ass_mod;
            s_ops[O_ASS_LSHIFT] = op_ass_lshift;
            s_ops[O_ASS_RSHIFT] = op_ass_rshift;
            s_ops[O_ASS_BIT_AND] = op_ass_bit_and;
            s_ops[O_ASS_BIT_OR] = op_ass_bit_or;
            s_ops[O_ASS_BIT_XOR] = op_ass_bit_xor;
            s_ops[O_DOT] = op_dot;
            s_ops[O_INDEX] = op_index;
            s_ops[O_SLICE] = op_slice;
            s_ops[O_DEL] = op_del;
            s_ops[O_EXCALL] = op_excall;

            s_ops[O_CASE] = op_nop;
            s_ops[O_CATCH] = op_nop;
            s_ops[O_CURRENT] = op_nop;
            s_ops[O_DEFAULT] = op_nop;
            s_ops[O_PARENT] = op_nop;
            s_ops[O_ROOT] = op_nop;
            s_ops[O_ECONST] = op_econst;
            s_ops[O_UNPACK] = op_nop;
            s_ops[O_HERE] = op_here;
            s_ops[O_TRAP] = op_trap;
            s_ops[O_EVAL] = op_eval;
        }

        void walker::reset() {
            state.clear();
            state.m_cfg = &m_cfg;
            m_insn = 0;
            m_insn_total = 0;
            m_interrupted = false;
            m_interrupt_desc.clear();

            m_root = impl_import();
            m_root.m_fly = &m_root_fly;
            m_root_fly.m_ast = nullptr;
            m_root_fly.m_root.m_store.clear();
            m_root_fly.m_path.clear();
            m_root_fly.m_root.m_env_paths.clear();
            m_root_asts.clear();
            state.root = &m_root;
            state.current = &m_root;
            state.root_entity = &m_root;
        }

        variant walker::walk_tree(const varvec& _tree) {
            if (m_interrupted) return variant();
            if (_tree.empty()) return variant();
            if (!_tree[0].is<OPTYPE>()) {
                return _tree[0];
            }

            op_enum op = static_cast<op_enum>(_tree[0].to<OPTYPE>());

            // Debug markers are not instructions: they skip both the dispatch table and the step budget
            if (op == O_DEBUG) return op_debug(_tree, *this);
            else if (!checkpoint()) return variant();

            if (op < O_ENUMSIZE && s_ops[op]) {
                return s_ops[op](_tree, *this);
            }

            if (state.on_cerr) state.on_cerr->exec("Undefined op_enum: " + std::to_string(op));
            return variant();
        }

        // The interrupt flag is tested first: interval 0 disables exec events but not set_interrupt()
        bool walker::checkpoint() {
            if (m_interrupted.load(std::memory_order_relaxed)) return false;

            uint_64 interval = m_hook_interval.load(std::memory_order_relaxed);
            if (!interval || ++m_insn < interval) return true;

            m_insn = 0;
            ++m_insn_total;
            return call_hook(*this, hook_event::exec, variant(static_cast<int_64>(m_insn_total)));
        }

        variant walker::walk_forest(const varvec& _forest) {
            variant result;
            for (size_t i = 0; i < _forest.size(); i++) {
                if (_forest[i].is_vec()) {
                    result = walk_tree(_forest[i].to<varvec>());
                    if (state.ret_flag || state.break_flag || state.cont_flag || state.tail_flag) break;
                }
            }
            return result;
        }

        variant walker::op_nop(const varvec&, walker&) { return variant(); }

        variant walker::op_here(const varvec& _tree, walker&) {
            return _tree.size() > 1 ? _tree[1] : variant();
        }

        variant walker::op_debug(const varvec& _tree, walker& _w) {
            if (!_w.m_cfg.debug_enable) return variant();
            auto val = [&](size_t _i) {
                return (_tree.size() > _i && _tree[_i].is<int_64>())
                           ? static_cast<uint_32>(_tree[_i].to<int_64>())
                           : 0;
            };
            src_pos& p = _w.state.pos_slot();
            p.row = val(1);
            p.col = val(2);
            p.ofst = val(3);
            // debug events leave the exec checkpoint counter alone (false = do not clear m_insn)
            call_hook(_w, hook_event::debug, variant(), false);
            return variant();
        }

        variant walker::op_trap(const varvec& _tree, walker& _w) {

            if (!_w.m_hook.load(std::memory_order_relaxed))
                return variant();
            varmap m;
            m["here"] = _tree[1];
            bool hit = true;
            if (_tree.size() == 3)
                m["args"] = eval_arg(_tree[2], _w);
            else if (_tree.size() == 4) {
                hit = to_bool_strict(eval_arg(_tree[2], _w));
                if (hit) m["args"] = eval_arg(_tree[3], _w);
            }
            if (hit)
                call_hook(_w, hook_event::trap, variant(std::move(m)));
            return variant();
        }

        variant walker::op_eval(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2)
                throw script_exception{error_type::ArgError,
                                       std::string("eval: missing code string")};
            variant code = walker::eval_arg(_tree[1], _w);
            if (!code.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("eval: code must be a string")};

            variant params;
            if (_tree.size() > 2) {
                params = walker::eval_arg(_tree[2], _w);
                if (!params.is<varmap>())
                    throw script_exception{error_type::TypeError,
                                           std::string("eval: params must be a map")};
            }

            bytes b(code.to<std::string>());
            token_list tl;
            signal<const compile_error&> cmpl;
            compile_error ce;
            bool got_err = false;
            cmpl.connect([&](const compile_error& _e) {
                if (got_err) return;
                ce = _e;
                got_err = true;
            });
            bool lex_ok = tl.tokenize(bytes_view(b), &cmpl);
            parser p(tl, &cmpl, "",
                     _w.state.m_search_paths ? *_w.state.m_search_paths
                                             : std::list<std::string>(),
                     _w.m_cfg.parse_depth, nullptr, nullptr, nullptr, std::string(), 0,
                     _w.m_cfg.debug_enable);

            varvec body = lex_ok ? p.parse_body() : varvec();
            if (!lex_ok || p.has_error()) {

                std::string at = ce.loc.row ? (" at " + std::to_string(ce.loc.row) + ":" + std::to_string(ce.loc.col))
                                            : std::string();
                throw script_exception{error_type::ParseError,
                                       std::string("eval: syntax error") + at +
                                           (ce.msg.empty() ? std::string() : ": " + ce.msg)};
            }

            _w.state.push_frame(nullptr, FF_RET | FF_BREAK | FF_CONT | FF_TAIL);
            if (params.is<varmap>()) {
                auto& map = params.to<varmap>();
                for (auto it = map.begin(); it != map.end(); ++it)
                    _w.state.frames.back().store(it.key(), it.value());
            }
            variant result = _w.walk_forest(body);
            while (_w.state.tail_flag) {
                _w.state.tail_flag = false;
                _w.state.ret_flag = false;
                result = _w.walk_forest(body);
            }
            _w.state.pop_frame();
            _w.state.current = _w.state.frames.empty() ? _w.state.root : _w.state.frames.back().ent;
            return result;
        }

        variant walker::op_program(const varvec& _tree, walker& _w) {
            for (size_t i = 1; i < _tree.size(); i++) {
                if (_tree[i].is_vec()) {
                    _w.walk_tree(_tree[i].to<varvec>());
                    if (_w.state.ret_flag || _w.state.break_flag || _w.state.cont_flag || _w.state.tail_flag) break;
                }
            }
            return variant();
        }

        variant walker::op_block(const varvec& _tree, walker& _w) {
            _w.state.push_frame(nullptr);
            variant result;
            for (size_t i = 1; i < _tree.size(); i++) {
                if (_tree[i].is_vec()) {
                    result = _w.walk_tree(_tree[i].to<varvec>());
                    if (_w.state.ret_flag || _w.state.break_flag || _w.state.cont_flag || _w.state.tail_flag) break;
                }
            }
            _w.state.pop_frame();
            return result;
        }

        variant walker::op_load(const varvec& _tree, walker& _w) {
            std::string name = _tree[1].to<std::string>();
            variant* p = _w.state.var_ptr(name);
            if (!p) throw script_exception{error_type::NameError, std::string("Undefined: " + name)};
            return *p;
        }

        // @'s string addresses an lvalue — a name or a chain of them, never an operation
        static bool reflection_is_chain(const varvec& _expr) {
            if (_expr.empty() || !_expr[0].is<OPTYPE>()) return false;
            op_enum head = static_cast<op_enum>(_expr[0].to<OPTYPE>());
            return head == O_LOAD || head == O_ILOAD || head == O_DOT || head == O_INDEX;
        }

        static void check_reflection_chain(const varvec& _expr, const std::string& _path) {
            if (reflection_is_chain(_expr)) return;
            std::string shown = _path.size() > 80 ? _path.substr(0, 80) + "..." : _path;
            throw script_exception{error_type::NameError,
                                   "reflection is not a legal name or chain: " + shown};
        }

        variant walker::op_iload(const varvec& _tree, walker& _w) {
            variant name_val = walker::eval_arg(_tree[1], _w);
            if (!name_val.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("Indirect load: variable did not evaluate to a string")};
            std::string path = name_val.to<std::string>();

            if (!path.empty() && path[0] == '$') {

                if (is_bare_ext_name(path.substr(1))) {
                    std::string name = path.substr(1);
                    if (_w.state.m_engine->fid_define(name))
                        return _w.state.m_engine->get_define(name);

                    if (_w.state.m_engine->fid_extend(name))
                        throw script_exception{error_type::TypeError,
                                               std::string("extension function cannot be read as value: $" + name)};
                    throw script_exception{error_type::NameError,
                                           std::string("static definition not found: $" + name)};
                }
            }

            if (walker::is_simple_name(path)) {
                variant* p = _w.state.var_ptr(path);
                if (!p) throw script_exception{error_type::NameError, std::string("Undefined: " + path)};
                return *p;
            }

            auto expr = walker::iload_parse_expr(path, _w.m_cfg.parse_depth);
            check_reflection_chain(expr, path);
            return _w.walk_tree(expr);
        }

        variant walker::op_var(const varvec& _tree, walker& _w) {

            for (size_t i = 1; i < _tree.size(); i += 2) {
                variant name = _tree[i];
                variant init_val;
                if (i + 1 < _tree.size() && _tree[i + 1].is_vec())
                    init_val = _w.walk_tree(_tree[i + 1].to<varvec>());
                walker::store_raw(_w, name, std::move(init_val));
            }
            return variant();
        }

        variant walker::op_store(const varvec& _tree, walker& _w) {
            auto& lhs = _tree[1].to<varvec>();
            variant val = _tree.size() > 2
                              ? _w.walk_tree(_tree[2].to<varvec>())
                              : variant();

            if (lhs[0].is<OPTYPE>() &&
                static_cast<op_enum>(lhs[0].to<OPTYPE>()) == O_LOAD) {

                // val was moved into the slot, so the result has to be read back from it
                walker::assign_raw(_w, lhs[1], std::move(val));
                variant* stored = _w.state.var_ptr(lhs[1].to<std::string>());
                return nullptr != stored ? *stored : variant();
            }

            target_resolved t = walker::resolve_target(lhs, _w, resolve_mode::write, false);
            if (t.type == target_resolved::kind::byte) {

                // the position is checked before the value: with overflow_check on, s[5] = 300 reports
                // the IndexError; an append has no position to check, so the value converts first
                if (t.key.null()) {
                    int_64 b = byte_value(val, _w.m_cfg.overflow_check);
                    byte_append(*t.parent, b);
                    return variant(b);
                }
                uint_64 i = byte_index(*t.parent, t.key);
                int_64 b = byte_value(val, _w.m_cfg.overflow_check);
                byte_put(*t.parent, i, b);
                return variant(b);
            }
            if (t.type == target_resolved::kind::slice) {
                walker::store_slice(t, val);
                return val;
            }
            if (t.type == target_resolved::kind::slot) {
                *t.slot = std::move(val);
                bind_owner(*t.slot, t.owner);
                return *t.slot;
            }
            return val;
        }

        variant walker::op_return(const varvec& _tree, walker& _w) {
            variant result = _tree.size() > 1 ? walker::eval_arg(_tree[1], _w)
                                              : variant();
            _w.state.ret_flag = true;
            return result;
        }

        variant walker::op_call(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2 || !_tree[1].is<std::string>()) return variant();
            const std::string& func_name = _tree[1].to<std::string>();
            if (func_name.empty())
                throw script_exception{error_type::NameError, std::string("Undefined function: " + func_name)};

            const varvec* def_tree = walker::find_def(_w, func_name);
            if (def_tree)
                return walker::invoke_def(_w.state.current, *def_tree, func_name, _tree, _w);

            throw script_exception{error_type::NameError, std::string("Undefined function: " + func_name)};
        }

        variant walker::op_ncall(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return variant();
            varvec dot_ast;
            dot_ast.push_back(variant(OPTYPE(O_DOT)));
            auto& keys = _tree[1].to<varvec>();
            for (size_t i = 0; i < keys.size(); i++)
                dot_ast.push_back(keys[i]);
            return walker::invoke_dot(resolve_dot(dot_ast, _w.state), _tree, _w);
        }

        variant walker::op_icall(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return variant();

            variant name_val = walker::eval_arg(_tree[1], _w);
            std::string path;
            if (name_val.is<std::string>())
                path = name_val.to<std::string>();
            if (path.empty()) {
                std::string msg = "Indirect call: expected function name string, got ";
                msg += type_name_script(name_val);
                throw script_exception{error_type::NameError, msg};
            }

            if (path[0] == '$') {

                std::string exname = path.substr(1);
                if (!is_bare_ext_name(exname))
                    throw script_exception{error_type::NameError,
                                           std::string("Indirect call: extension function name after $: '" + path + "'")};
                return invoke_extend(_w, exname, _tree, 2);
            }

            std::string src = path + ";";
            alx::bytes b(src.c_str());
            token_list tl;
            signal<const compile_error&> cmpl;
            compile_error ce;
            bool got_err = false;
            cmpl.connect([&](const compile_error& _e) {
                if (got_err) return;
                ce = _e;
                got_err = true;
            });
            tl.tokenize(alx::bytes_view(b), &cmpl);
            parser p(tl, &cmpl);
            varvec ast = p.parse();
            if (got_err) {
                std::string at = ce.loc.row ? (" at " + std::to_string(ce.loc.row) + ":" +
                                               std::to_string(ce.loc.col))
                                            : std::string();
                throw script_exception{error_type::ParseError,
                                       std::string("Indirect call: cannot parse path") + at +
                                           ": " + ce.msg};
            }
            if (p.has_error() || ast.size() < 2) {

                std::string shown = path.size() > 80 ? path.substr(0, 80) + "..." : path;
                throw script_exception{error_type::NameError,
                                       std::string("Indirect call: cannot parse path '" + shown + "'")};
            }
            auto& expr = ast[1].to<varvec>();
            if (expr.size() < 2) return variant();
            op_enum head = static_cast<op_enum>(expr[0].to<OPTYPE>());

            if (head == O_LOAD) {
                std::string func_name = expr[1].to<std::string>();
                const varvec* def_tree = walker::find_def(_w, func_name);
                if (def_tree)
                    return walker::invoke_def(_w.state.current, *def_tree, func_name, _tree, _w);
                throw script_exception{error_type::NameError,
                                       std::string("Indirect call: undefined function '" + func_name + "'")};
            }

            if (head != O_DOT)
                throw script_exception{error_type::NameError,
                                       std::string("Indirect call: invalid path '" + path + "'")};
            return walker::invoke_dot(resolve_dot(expr, _w.state), _tree, _w);
        }

        variant walker::op_tcall(const varvec& _tree, walker& _w) {
            const varvec* def = nullptr;
            scope_frame* def_frame = nullptr;
            for (auto it = _w.state.frames.rbegin(); it != _w.state.frames.rend(); ++it) {
                if (it->def) {
                    def = it->def;
                    def_frame = &*it;
                    break;
                }
            }
            if (!def)
                throw script_exception{error_type::RuntimeError,
                                       std::string("TCO: no enclosing function frame")};

            // The rewrite was by name: a local of that name makes the tail position an ordinary call
            if (_tree.size() > 1 && _tree[1].is<std::string>() &&
                walker::find_def(_w, _tree[1].to<std::string>()) != def)
                return walker::op_call(_tree, _w);

            walker::eval_and_bind_args(_w.state.current, *def, "", _tree, _w, true, def_frame);

            _w.state.tail_flag = true;
            return variant();
        }

        variant walker::op_def(const varvec& _tree, walker& _w) {
            std::string name = _tree[1].to<std::string>();
            walker::check_name_conflict(_w, name);
            walker::store_raw(_w, variant(name),
                              variant(anyptr_ex<call_able>::make(new call_able(&_tree))));
            return variant();
        }

        variant walker::op_if(const varvec& _tree, walker& _w) {
            variant test_val = _w.walk_tree(_tree[1].to<varvec>());
            if (to_bool_strict(test_val)) {
                return _w.walk_tree(_tree[2].to<varvec>());
            } else {
                return _w.walk_tree(_tree[3].to<varvec>());
            }
        }

        variant walker::op_while(const varvec& _tree, walker& _w) {
            variant result;

            _w.state.push_frame(nullptr, FF_BREAK | FF_CONT);
            auto& sf = _w.state.frames.back();

            while (true) {

                // continue leaves cont_flag set (it does not end the loop), so it is cleared here
                _w.state.break_flag = false;
                _w.state.cont_flag = false;

                if (!to_bool_strict(_w.walk_tree(_tree[1].to<varvec>()))) break;

                result = walker::walk_body(_tree[2].to<varvec>(), _w);
                if (_w.state.ret_flag) break;
                if (_w.state.break_flag) break;
                if (_w.state.tail_flag) break;

                // The frame outlives the iteration, so its dtor never runs: drop the body's locals by hand
                if (sf.ent->m_store.m_data.size() > sf.base) {
                    sf.var_map.clear();
                    sf.free.clear();
                    sf.ent->m_store.m_data.resize(sf.base);
                }

                if (!_w.checkpoint()) break;
            }
            _w.state.pop_frame();
            return result;
        }

        variant walker::op_for(const varvec& _tree, walker& _w) {
            variant result;

            _w.state.push_frame(nullptr, FF_BREAK | FF_CONT);
            auto& sf = _w.state.frames.back();

            if (_tree.size() > 1 && !_tree[1].null()) {
                if (_tree[1].is_vec()) _w.walk_tree(_tree[1].to<varvec>());
            }

            auto var_map_snap = sf.var_map;
            const size_t pre_size = sf.ent->m_store.m_data.size();
            sf.prot = &var_map_snap;

            while (true) {

                _w.state.break_flag = false;
                _w.state.cont_flag = false;

                if (_tree.size() > 2 && !_tree[2].null()) {
                    if (_tree[2].is_vec()) {
                        auto& test_vec = _tree[2].to<varvec>();
                        if (!test_vec.empty()) {
                            variant tv = _w.walk_tree(test_vec);
                            if (!to_bool_strict(tv)) break;
                        }
                    }
                }

                if (_tree.size() > 4 && _tree[4].is_vec())
                    result = walker::walk_body(_tree[4].to<varvec>(), _w);

                if (_w.state.ret_flag) break;
                if (_w.state.break_flag) break;
                if (_w.state.tail_flag) break;

                if (_tree.size() > 3 && !_tree[3].null()) {
                    if (_tree[3].is_vec()) _w.walk_tree(_tree[3].to<varvec>());
                }

                if (sf.ent->m_store.m_data.size() > pre_size) {

                    sf.var_map = var_map_snap;
                    sf.free.clear();
                    sf.ent->m_store.m_data.resize(pre_size);
                }

                if (!_w.checkpoint()) break;
            }
            _w.state.pop_frame();
            return result;
        }

        variant walker::op_foreach(const varvec& _tree, walker& _w) {
            variant result;

            _w.state.push_frame(nullptr, FF_BREAK | FF_CONT);
            auto& sf = _w.state.frames.back();

            variant iter_val = _w.walk_tree(_tree[2].to<varvec>());
            auto& target = _tree[1].to<varvec>();
            bool is_var = (target[0].is<OPTYPE>() && static_cast<op_enum>(target[0].to<OPTYPE>()) == O_VAR);

            variant vname = target[1];

            if (is_var) walker::store_raw(_w, vname, variant());

            auto var_map_snap = sf.var_map;
            const size_t pre_size = sf.ent->m_store.m_data.size();
            sf.prot = &var_map_snap;

            switch (iter_val.type()) {
            case variant::id<varvec>(): {
                auto& vec = iter_val.to<varvec>();
                for (size_t i = 0; i < vec.size(); i++) {
                    walker::assign_raw(_w, vname, vec[i]);

                    _w.state.break_flag = false;
                    _w.state.cont_flag = false;

                    result = walker::walk_body(_tree[3].to<varvec>(), _w);
                    if (_w.state.ret_flag) break;
                    if (_w.state.break_flag) break;
                    if (_w.state.tail_flag) break;

                    if (sf.ent->m_store.m_data.size() > pre_size) {
                        sf.var_map = var_map_snap;
                        sf.free.clear();
                        sf.ent->m_store.m_data.resize(pre_size);
                    }

                    if (!_w.checkpoint()) break;
                }
                break;
            }
            case variant::id<varlst>(): {
                for (auto& x : iter_val.to<varlst>()) {
                    walker::assign_raw(_w, vname, x);

                    _w.state.break_flag = false;
                    _w.state.cont_flag = false;

                    result = walker::walk_body(_tree[3].to<varvec>(), _w);
                    if (_w.state.ret_flag) break;
                    if (_w.state.break_flag) break;
                    if (_w.state.tail_flag) break;

                    if (sf.ent->m_store.m_data.size() > pre_size) {
                        sf.var_map = var_map_snap;
                        sf.free.clear();
                        sf.ent->m_store.m_data.resize(pre_size);
                    }

                    if (!_w.checkpoint()) break;
                }
                break;
            }
            case variant::id<varmap>(): {
                auto& map = iter_val.to<varmap>();
                for (varmap::const_iterator it = map.cbegin(); it != map.cend(); ++it) {
                    walker::assign_raw(_w, vname, variant(varmap{{"key", it.key()}, {"value", it.value()}}));

                    _w.state.break_flag = false;
                    _w.state.cont_flag = false;

                    result = walker::walk_body(_tree[3].to<varvec>(), _w);
                    if (_w.state.ret_flag) break;
                    if (_w.state.break_flag) break;
                    if (_w.state.tail_flag) break;

                    if (sf.ent->m_store.m_data.size() > pre_size) {
                        sf.var_map = var_map_snap;
                        sf.free.clear();
                        sf.ent->m_store.m_data.resize(pre_size);
                    }

                    if (!_w.checkpoint()) break;
                }
                break;
            }
            default:
                throw script_exception{error_type::TypeError, std::string("foreach expects vec, lst, or map")};
            }
            _w.state.pop_frame();
            return result;
        }

        variant walker::op_break(const varvec&, walker& _w) {
            _w.state.break_flag = true;
            return variant();
        }

        variant walker::op_continue(const varvec&, walker& _w) {
            _w.state.cont_flag = true;
            return variant();
        }

        variant walker::op_switch(const varvec& _tree, walker& _w) {
            variant test_val = _w.walk_tree(_tree[1].to<varvec>());
            variant result;
            bool matched = false;

            for (size_t i = 2; i < _tree.size(); i++) {
                auto& case_node = _tree[i].to<varvec>();

                if (case_node[0].is<OPTYPE>() && static_cast<op_enum>(case_node[0].to<OPTYPE>()) == O_CASE) {
                    variant case_val = _w.walk_tree(case_node[1].to<varvec>());
                    bool equals = false;
                    switch (type_pair(test_val, case_val)) {
                    case type_pair<int_64, int_64>():
                        equals = test_val.to<int_64>() == case_val.to<int_64>();
                        break;
                    case type_pair<double, double>():
                        equals = test_val.to<double>() == case_val.to<double>();
                        break;
                    case type_pair<std::string, std::string>():
                        equals = test_val.to<std::string>() == case_val.to<std::string>();
                        break;
                    case type_pair<bool, bool>():
                        equals = test_val.to<bool>() == case_val.to<bool>();
                        break;
                    default: break;
                    }

                    if (equals && !matched) {
                        matched = true;
                        _w.state.break_flag = false;
                        result = _w.walk_tree(case_node[2].to<varvec>());
                        if (_w.state.ret_flag) return result;
                    }
                } else if (case_node[0].is<OPTYPE>() && static_cast<op_enum>(case_node[0].to<OPTYPE>()) == O_DEFAULT && !matched) {
                    matched = true;
                    result = _w.walk_tree(case_node[1].to<varvec>());
                    if (_w.state.ret_flag) return result;
                }
            }
            // A break inside the matched case only leaves the switch, never the enclosing loop
            _w.state.break_flag = false;
            _w.state.cont_flag = false;
            return result;
        }

        variant walker::op_try(const varvec& _tree, walker& _w) {
            auto& catch_node = _tree[2].to<varvec>();
            std::string catch_name = catch_node[1].to<std::string>();

            size_t frame_snap = _w.state.frames.size();

            auto unwind_and_bind = [&](const std::string& _type, const variant& _val) {
                while (_w.state.frames.size() > frame_snap)
                    _w.state.pop_frame();
                _w.state.current = _w.state.frames.empty() ? _w.state.root : _w.state.frames.back().ent;

                varmap catch_info;
                catch_info["what"] = variant(_type);
                catch_info["info"] = _val;
                size_t data_snap = _w.state.current->m_store.m_data.size();
                _w.state.current->m_store.m_data.push_back(variant(std::move(catch_info)));

                scope_frame sf;
                sf.ent = _w.state.current;
                sf.base = data_snap;

                sf.var_map.set(catch_name, data_snap);

                // Pushed directly: push_frame would take the size after catch_info and leave that slot unowned
                _w.state.frames.push_back(std::move(sf));
                _w.state.current = _w.state.frames.back().ent;
            };

            auto cleanup_catch_frame = [&]() {
                if (!_w.state.frames.empty()) _w.state.pop_frame();
            };

            try {
                return _w.walk_tree(_tree[1].to<varvec>());
            } catch (const script_exception& _e) {
                unwind_and_bind(error_type_name(_e.type), _e.info);
                variant result = _w.walk_tree(catch_node[2].to<varvec>());
                cleanup_catch_frame();
                return result;
                // Native throws become catchable script errors: allocation is MemoryError, anything else NativeError
            } catch (const std::bad_alloc&) {

                unwind_and_bind(error_type_name(error_type::MemoryError), std::string("out of memory"));
                variant result = _w.walk_tree(catch_node[2].to<varvec>());
                cleanup_catch_frame();
                return result;
            } catch (const std::length_error& _e) {
                unwind_and_bind(error_type_name(error_type::MemoryError), std::string(_e.what()));
                variant result = _w.walk_tree(catch_node[2].to<varvec>());
                cleanup_catch_frame();
                return result;
            } catch (const std::exception& _e) {
                unwind_and_bind("NativeError", std::string(_e.what()));
                variant result = _w.walk_tree(catch_node[2].to<varvec>());
                cleanup_catch_frame();
                return result;
            } catch (...) {
                unwind_and_bind("NativeError", std::string("unknown native error"));
                variant result = _w.walk_tree(catch_node[2].to<varvec>());
                cleanup_catch_frame();
                return result;
            }
        }

        variant walker::op_throw(const varvec& _tree, walker& _w) {
            variant val = walker::eval_arg(_tree[1], _w);
            throw script_exception{error_type::RuntimeError, cov_string(val)};
        }

        variant walker::op_int(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return int_64(0);
            return variant(cov_int(walker::eval_arg(_tree[1], _w)));
        }

        variant walker::op_float(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return 0.0;
            return variant(cov_float(walker::eval_arg(_tree[1], _w)));
        }

        // Names fold to lowercase alnum only, so "UTF-8", "utf_8" and "utf8" name the same codec
        static std::string normalize_enc(const std::string& _enc) {
            std::string result;
            result.reserve(_enc.size());
            for (char c : _enc) {
                if (c >= 'A' && c <= 'Z') result += char(c - 'A' + 'a');
                else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) result += c;
            }
            return result;
        }

        static const char* const s_enc_names[]{"utf8", "utf8bom", "gbk", "utf16", "utf16le", "utf16lebom", "utf16be", "utf16bebom", "hex", "base64"};
        static const size_t s_enc_name_count = sizeof(s_enc_names) / sizeof(s_enc_names[0]);

        static size_t enc_edit_distance(const std::string& _a, const std::string& _b) {
            std::vector<size_t> row(_b.size() + 1);
            for (size_t j = 0; j <= _b.size(); j++) row[j] = j;
            for (size_t i = 1; i <= _a.size(); i++) {
                size_t diag = row[0];
                row[0] = i;
                for (size_t j = 1; j <= _b.size(); j++) {
                    size_t above = row[j];
                    row[j] = alx::min_value(diag + (_a[i - 1] == _b[j - 1] ? 0 : 1), alx::min_value(above + 1, row[j - 1] + 1));
                    diag = above;
                }
            }
            return row[_b.size()];
        }

        static std::string unknown_encoding(const std::string& _enc, const std::string& _key) {
            std::vector<std::pair<size_t, size_t>> ranked(s_enc_name_count);
            for (size_t i = 0; i < s_enc_name_count; i++)
                ranked[i] = std::make_pair(enc_edit_distance(_key, s_enc_names[i]), i);
            std::sort(ranked.begin(), ranked.end());

            std::string result{"Unknown encoding: "};
            result += _enc + " (did you mean: ";
            for (size_t i = 0; i < 3; i++) result += std::string(i ? ", " : "") + s_enc_names[ranked[i].second];
            result += "; valid: ";
            for (size_t i = 0; i < s_enc_name_count; i++) result += std::string(i ? ", " : "") + s_enc_names[i];
            return result + "; case and separators are ignored)";
        }

        static strutil::CODE_FORMAT parse_code_format(const std::string& _enc) {
            static const struct {
                const char* key;
                strutil::CODE_FORMAT fmt;
            } s_aliases[]{
                {"utf8", strutil::UTF8},
                {"utf8bom", strutil::UTF8_BOM},
                {"gbk", strutil::GBK},
                {"utf16", strutil::UTF16_LE},
                {"utf16le", strutil::UTF16_LE},
                {"utf16lebom", strutil::UTF16_LE_BOM},
                {"utf16be", strutil::UTF16_BE},
                {"utf16bebom", strutil::UTF16_BE_BOM},
            };
            std::string key = normalize_enc(_enc);
            for (const auto& alias : s_aliases) {
                if (key == alias.key) return alias.fmt;
            }

            throw script_exception{error_type::ConvError, unknown_encoding(_enc, key)};
        }

        variant walker::op_string(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return std::string();
            variant v1 = walker::eval_arg(_tree[1], _w);

            if (_tree.size() == 2) return variant(cov_string(v1));

            if (_tree.size() == 3) {
                if (v1.is<bytes>()) {
                    std::string enc = cov_string(walker::eval_arg(_tree[2], _w));
                    std::string key = normalize_enc(enc);
                    auto& b = v1.to<bytes>();
                    if (key == "hex") return variant(b.to_hex());
                    if (key == "base64") return variant(b.to_base64());
                    auto fmt = parse_code_format(enc);
                    auto result = strutil::code_conver(std::string(reinterpret_cast<const char*>(b.data()), b.size()), strutil::UTF8, fmt);
                    return variant(result);
                }
                if (v1.is<int_64>()) {
                    int_64 val = v1.to<int_64>();
                    int base = static_cast<int>(cov_int(walker::eval_arg(_tree[2], _w)));
                    if (base != 2 && base != 8 && base != 10 && base != 16)
                        throw script_exception{error_type::ConvError, std::string("Base must be 2, 8, 10, or 16")};
                    static const char digits[] = "0123456789abcdef";
                    bool neg = val < 0;
                    uint_64 u;
                    if (val >= 0) u = static_cast<uint_64>(val);
                    else u = static_cast<uint_64>(-(val + 1)) + 1ULL;
                    const char* prefix = base == 16 ? "0x" : base == 8 ? "0o"
                                                         : base == 2   ? "0b"
                                                                       : "";
                    if (u == 0) return variant((neg ? "-" : std::string()) + prefix + "0");
                    std::string s;
                    uint_64 ub = static_cast<uint_64>(base);
                    while (u > 0) {
                        s = digits[u % ub] + s;
                        u /= ub;
                    }
                    return variant((neg ? "-" : std::string()) + prefix + s);
                }
                if (v1.is<double>()) {
                    double val = v1.to<double>();
                    int prec = static_cast<int>(cov_int(walker::eval_arg(_tree[2], _w)));
                    char buf[512];
                    if (prec == 0)
                        snprintf(buf, sizeof(buf), "%.0f", val);
                    else if (prec > 0)
                        snprintf(buf, sizeof(buf), "%.*f", prec, val);
                    else
                        snprintf(buf, sizeof(buf), "%.*g", -prec, val);
                    return std::string(buf);
                }
            }

            std::string pattern = cov_string(v1);
            std::vector<std::string> fmt_args;
            for (size_t i = 2; i < _tree.size(); i++) {
                variant v = walker::eval_arg(_tree[i], _w);
                switch (v.type()) {
                case variant::id<std::string>(): fmt_args.push_back(v.to<std::string>()); break;
                case variant::id<int_64>(): fmt_args.push_back(std::to_string(v.to<int_64>())); break;
                case variant::id<double>(): fmt_args.push_back(fmt_double(v.to<double>())); break;
                case variant::id<bool>(): fmt_args.push_back(v.to<bool>() ? "true" : "false"); break;
                default: fmt_args.push_back(cov_string(v)); break;
                }
            }
            return variant(strutil::format(pattern, fmt_args));
        }

        variant walker::op_bool(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return false;
            return variant(cov_bool(walker::eval_arg(_tree[1], _w)));
        }

        variant walker::op_bytes(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2) return bytes();
            variant v1 = walker::eval_arg(_tree[1], _w);
            if (v1.null()) return variant(bytes());
            if (!v1.is<std::string>())
                throw script_exception{error_type::TypeError, std::string("bytes() expects string, got ") + type_name_script(v1)};
            if (_tree.size() == 2) {
                return variant(bytes(v1.to<std::string>()));
            }
            variant v2 = walker::eval_arg(_tree[2], _w);
            std::string enc = cov_string(v2);
            std::string key = normalize_enc(enc);
            auto& s = v1.to<std::string>();
            if (key == "hex") return variant(bytes::from_hex(s));
            if (key == "base64") return variant(bytes::from_base64(s));
            auto fmt = parse_code_format(enc);
            auto result = strutil::code_conver(s, fmt, strutil::UTF8);
            return variant(bytes(result));
        }

        variant walker::op_vec(const varvec& _tree, walker& _w) {
            if (_tree.size() == 4 && _tree[1].is<varvec>()) {

                auto& elems = _tree[1].to<varvec>();
                if (!elems.empty()) {

                    varvec result;
                    for (size_t i = 0; i < elems.size(); i++)
                        result.push_back(walker::eval_arg(elems[i], _w));

                    if (!_tree[3].null()) {
                        int_64 count = walker::eval_arg(_tree[3], _w).to<int_64>();
                        if (_w.m_cfg.max_vecfill > 0 && count > 0 && static_cast<size_t>(count) > _w.m_cfg.max_vecfill)
                            throw script_exception{
                                error_type::ResourceError,
                                std::string("Vec fill count exceeds limit")};
                        if (count > 0 && !result.empty()) {
                            variant last = result.back();
                            for (int_64 i = 1; i < count; i++) result.push_back(last);
                        }
                    }
                    return variant(std::move(result));
                }

                variant fill_val;
                if (!_tree[2].null()) {
                    fill_val = walker::eval_arg(_tree[2], _w);
                }
                int_64 count = 0;
                if (!_tree[3].null()) {
                    count = walker::eval_arg(_tree[3], _w).to<int_64>();
                }
                if (_w.m_cfg.max_vecfill > 0 && count > 0 && static_cast<size_t>(count) > _w.m_cfg.max_vecfill)
                    throw script_exception{
                        error_type::ResourceError,
                        std::string("Vec fill count exceeds limit")};
                varvec result;
                for (int_64 i = 0; i < count; i++) result.push_back(fill_val);
                return variant(std::move(result));
            }

            if (_tree.size() < 2) return varvec();
            variant v = walker::eval_arg(_tree[1], _w);
            return variant(cov_vec(v));
        }

        variant walker::op_map(const varvec& _tree, walker& _w) {

            if (_tree.size() < 2) return varmap();

            if (_tree.size() == 2) {
                variant v = walker::eval_arg(_tree[1], _w);
                return variant(cov_map(v));
            }

            varmap result;
            for (size_t i = 1; i + 1 < _tree.size(); i += 2) {
                variant key = walker::eval_arg(_tree[i], _w);
                if (!key.is<std::string>())
                    throw script_exception{error_type::TypeError,
                                           std::string("map key must be a string")};
                variant val = walker::eval_arg(_tree[i + 1], _w);
                result[key.to<std::string>()] = val;
            }
            return variant(std::move(result));
        }

        variant walker::op_lst(const varvec& _tree, walker& _w) {
            if (_tree.size() == 4 && _tree[1].is<varvec>()) {

                auto& elems = _tree[1].to<varvec>();
                if (!elems.empty()) {
                    varlst result;
                    for (size_t i = 0; i < elems.size(); i++)
                        result.push_back(walker::eval_arg(elems[i], _w));
                    if (!_tree[3].null()) {
                        int_64 count = walker::eval_arg(_tree[3], _w).to<int_64>();
                        if (_w.m_cfg.max_vecfill > 0 && count > 0 && static_cast<size_t>(count) > _w.m_cfg.max_vecfill)
                            throw script_exception{
                                error_type::ResourceError,
                                std::string("Lst fill count exceeds limit")};
                        if (count > 0 && !result.empty()) {
                            variant last = result.back();
                            for (int_64 i = 1; i < count; i++) result.push_back(last);
                        }
                    }
                    return variant(std::move(result));
                }

                variant fill_val;
                if (!_tree[2].null())
                    fill_val = walker::eval_arg(_tree[2], _w);
                int_64 count = 0;
                if (!_tree[3].null())
                    count = walker::eval_arg(_tree[3], _w).to<int_64>();
                if (_w.m_cfg.max_vecfill > 0 && count > 0 && static_cast<size_t>(count) > _w.m_cfg.max_vecfill)
                    throw script_exception{
                        error_type::ResourceError,
                        std::string("Lst fill count exceeds limit")};
                varlst result;
                for (int_64 i = 0; i < count; i++) result.push_back(fill_val);
                return variant(std::move(result));
            }

            if (_tree.size() < 2) return varlst();
            variant v = walker::eval_arg(_tree[1], _w);
            return variant(cov_lst(v));
        }

        variant walker::op_type(const varvec& _tree, walker& _w) {
            variant v = _tree.size() > 1 ? walker::eval_arg(_tree[1], _w) : variant();
            return std::string(type_name_script(v, _w.m_cfg.type_ex_ptr, _w.m_cfg.type_ex_ud));
        }

        variant walker::op_env(const varvec& _tree, walker& _w) {
            if (_tree.size() < 2 || !_tree[1].is_vec()) return variant();
            variant val = _w.walk_tree(_tree[1].to<varvec>());
            if (!val.is<varvec>()) return variant();
            auto& lst = val.to<varvec>();
            _w.state.current->m_env_paths.clear();

            // Relative entries resolve against the script's own directory; no script path (CLI/cache) means cwd
            std::string base = (_w.state.current->m_fly && !_w.state.current->m_fly->m_path.empty())
                                   ? dirname_of(_w.state.current->m_fly->m_path)
                                   : std::string();
            for (auto& v : lst) {
                if (v.is<std::string>()) {
                    std::string s = v.to<std::string>();
                    std::string abs;
                    if (file_info::is_absolute(s))
                        abs = file_info(s).path();
                    else if (!base.empty())
                        abs = file_info(base + "/" + s).path();
                    else
                        abs = file_info(s).path();
                    if (!abs.empty()) {
                        _w.state.current->m_env_paths.push_back(abs);
                        if (_w.state.on_csys && _w.state.current->m_fly)
                            _w.state.on_csys->exec(
                                alx::this_tid(),
                                "env: " + _w.state.current->m_fly->m_path + " add " + abs);
                    }
                }
            }
            return variant();
        }

        variant walker::op_import(const varvec& _tree, walker& _w) {
            if (_tree.size() < 3) return variant();
            std::string path_val = _tree[1].to<std::string>();
            std::string alias = _tree[2].to<std::string>();
            if (alias.empty()) return variant();

            if (!_w.mgr) return variant();

            walker::check_name_conflict(_w, alias);

            script_exception err{error_type::UnknownError, std::string()};
            impl_import* child = _w.mgr->make_import(path_val, &_w, err);
            if (!child) {

                // An interrupt must not be catchable: the failure branch returns instead of throwing through a try
                if (err.type == error_type::InterruptedError) {
                    _w.m_interrupted = true;
                    return variant();
                }
                if (err.type == error_type::ResourceError || err.type == error_type::MemoryError || err.type == error_type::NativeError) {
                    if (_w.state.on_cerr) _w.state.on_cerr->exec(err.info);
                    return variant();
                }
                throw err;
            }

            child->m_parent = _w.state.current;
            child->m_alias = alias;
            walker::store_raw(_w, variant(alias),
                              variant(anyptr_ex<impl_import>::make(child)));
            return variant();
        }

        variant walker::op_link(const varvec& _tree, walker& _w) {
            if (_tree.size() < 3) return variant();
            std::string path_val = _tree[1].to<std::string>();
            std::string alias = _tree[2].to<std::string>();
            if (alias.empty()) return variant();

            if (!_w.mgr) return variant();

            walker::check_name_conflict(_w, alias);

            script_exception err{error_type::UnknownError, std::string()};
            impl_link* child = _w.mgr->make_link(path_val, &_w, err);
            if (!child) {

                if (err.type == error_type::InterruptedError) {
                    _w.m_interrupted = true;
                    return variant();
                }
                if (err.type == error_type::ResourceError || err.type == error_type::MemoryError || err.type == error_type::NativeError) {
                    if (_w.state.on_cerr) _w.state.on_cerr->exec(err.info);
                    return variant();
                }
                throw err;
            }

            child->m_parent = _w.state.current;
            child->m_alias = alias;
            walker::store_raw(_w, variant(alias),
                              variant(anyptr_ex<impl_link>::make(child)));
            return variant();
        }

        variant walker::op_uplus(const varvec& _tree, walker& _w) {
            return walker::eval_arg(_tree[1], _w);
        }

        variant walker::op_uminus(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            if (a.is<int_64>()) {
                int_64 ia = a.to<int_64>();
                if (_w.m_cfg.overflow_check && ia == min_int_64)
                    throw script_exception{error_type::OverflowError, std::string("Integer negation overflow")};
                return variant(wrap_neg(ia));
            }
            if (a.is_number()) return variant(-a.to_number());
            throw script_exception{error_type::TypeError, std::string("Expected number for unary -")};
        }

        variant walker::op_add(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);

            switch (type_pair(a, b)) {
            case type_pair<int_64, int_64>(): {
                int_64 ia = a.to<int_64>(), ib = b.to<int_64>();
                if (_w.m_cfg.overflow_check &&
                    ((ib > 0 && ia > max_int_64 - ib) ||
                     (ib < 0 && ia < min_int_64 - ib)))
                    throw script_exception{error_type::OverflowError, std::string("Integer addition overflow")};
                return variant(wrap_add(ia, ib));
            }
            case type_pair<int_64, double>():
            case type_pair<double, int_64>():
            case type_pair<double, double>():
                return variant(a.to_number() + b.to_number());
            case type_pair<std::string, std::string>():
                return variant(a.to<std::string>() + b.to<std::string>());
            case type_pair<varvec, varvec>(): {
                varvec r = a.to<varvec>();
                const varvec& bv = b.to<varvec>();
                r.insert(r.end(), bv.begin(), bv.end());
                return variant(std::move(r));
            }
            case type_pair<varlst, varlst>(): {
                varlst r = a.to<varlst>();
                const varlst& bl = b.to<varlst>();
                r.insert(r.end(), bl.begin(), bl.end());
                return variant(std::move(r));
            }
            case type_pair<varmap, varmap>(): {
                varmap r(a.to<varmap>());
                const varmap& bm = b.to<varmap>();
                for (auto it = bm.cbegin(); it != bm.cend(); it++)
                    r[it.key()] = it.value();
                return variant(std::move(r));
            }
            default:
                throw script_exception{error_type::TypeError, std::string("Expected numbers, strings, arrays, lists or maps for +")};
            }
        }

        variant walker::op_sub(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            if (a.is<int_64>() && b.is<int_64>()) {
                int_64 ia = a.to<int_64>(), ib = b.to<int_64>();
                if (_w.m_cfg.overflow_check &&
                    ((ib < 0 && ia > max_int_64 + ib) ||
                     (ib > 0 && ia < min_int_64 + ib)))
                    throw script_exception{error_type::OverflowError, std::string("Integer subtraction overflow")};
                return variant(wrap_sub(ia, ib));
            }
            if (a.is_number() && b.is_number())
                return variant(a.to_number() - b.to_number());
            throw script_exception{error_type::TypeError, std::string("Expected numbers for -")};
        }

        variant walker::op_mul(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            if (a.is<int_64>() && b.is<int_64>()) {
                int_64 ia = a.to<int_64>(), ib = b.to<int_64>();
                bool overflow = false;
                if (ia > 0) {
                    if (ib > 0 && ia > max_int_64 / ib) overflow = true;
                    else if (ib < 0 && ib < min_int_64 / ia) overflow = true;
                } else if (ia < 0) {
                    if (ib > 0 && ia < min_int_64 / ib) overflow = true;
                    else if (ib < 0 && ia < max_int_64 / ib) overflow = true;
                }
                if (_w.m_cfg.overflow_check && overflow)
                    throw script_exception{error_type::OverflowError, std::string("Integer multiplication overflow")};
                return variant(wrap_mul(ia, ib));
            }
            if (a.is_number() && b.is_number())
                return variant(a.to_number() * b.to_number());
            throw script_exception{error_type::TypeError, std::string("Expected numbers for *")};
        }

        variant walker::op_div(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            if (a.is<int_64>() && b.is<int_64>()) {
                int_64 ia = a.to<int_64>(), ib = b.to<int_64>();
                if (ib == 0) throw script_exception{error_type::DivZeroError, std::string("Division by zero")};
                if (ib == -1 && ia == min_int_64)
                    throw script_exception{error_type::DivOverflowError, std::string("Integer division overflow")};
                return variant(ia / ib);
            }
            if (a.is_number() && b.is_number()) {
                double db = b.to_number();
                if (db == 0.0) throw script_exception{error_type::DivZeroError, std::string("Division by zero")};
                return variant(a.to_number() / db);
            }
            throw script_exception{error_type::TypeError, std::string("Expected numbers for /")};
        }

        variant walker::op_mod(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            int_64 ai = to_int_strict(a);
            int_64 bi = to_int_strict(b);
            if (bi == 0) throw script_exception{error_type::DivZeroError, std::string("Modulo by zero")};
            if (bi == -1 && ai == min_int_64)
                throw script_exception{error_type::DivOverflowError, std::string("Integer modulo overflow")};
            return variant(ai % bi);
        }

        variant walker::op_pre_inc(const varvec& _tree, walker& _w) {
            variant* p_ = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p_)
                throw script_exception{error_type::TypeError, std::string("not an lvalue")};
            int_64 old = to_int_strict(*p_);
            if (_w.m_cfg.overflow_check && (old == max_int_64))
                throw script_exception{error_type::OverflowError, std::string("Integer increment overflow")};
            int_64 val = wrap_add(old, 1);
            *p_ = variant(val);
            return variant(val);
        }

        variant walker::op_pre_dec(const varvec& _tree, walker& _w) {
            variant* p_ = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p_)
                throw script_exception{error_type::TypeError, std::string("not an lvalue")};
            int_64 old = to_int_strict(*p_);
            if (_w.m_cfg.overflow_check && (old == min_int_64))
                throw script_exception{error_type::OverflowError, std::string("Integer decrement overflow")};
            int_64 val = wrap_sub(old, 1);
            *p_ = variant(val);
            return variant(val);
        }

        variant walker::op_post_inc(const varvec& _tree, walker& _w) {
            variant* p_ = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p_)
                throw script_exception{error_type::TypeError, std::string("not an lvalue")};
            int_64 old = to_int_strict(*p_);
            if (_w.m_cfg.overflow_check && (old == max_int_64))
                throw script_exception{error_type::OverflowError, std::string("Integer increment overflow")};
            *p_ = variant(wrap_add(old, 1));
            return variant(old);
        }

        variant walker::op_post_dec(const varvec& _tree, walker& _w) {
            variant* p_ = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p_)
                throw script_exception{error_type::TypeError, std::string("not an lvalue")};
            int_64 old = to_int_strict(*p_);
            if (_w.m_cfg.overflow_check && (old == min_int_64))
                throw script_exception{error_type::OverflowError, std::string("Integer decrement overflow")};
            *p_ = variant(wrap_sub(old, 1));
            return variant(old);
        }

        variant walker::op_lshift(const varvec& _tree, walker& _w) {
            int_64 a = to_int_strict(walker::eval_arg(_tree[1], _w));
            int_64 b = to_int_strict(walker::eval_arg(_tree[2], _w));
            if (_w.m_cfg.overflow_check && (b < 0 || b >= 64))
                throw script_exception{error_type::ShiftError, std::string("Shift count out of range")};
            if (_w.m_cfg.overflow_check && (a < 0))
                throw script_exception{error_type::ShiftError, std::string("Left shift of negative value")};
            return variant(wrap_shl(a, b));
        }

        variant walker::op_rshift(const varvec& _tree, walker& _w) {
            int_64 a = to_int_strict(walker::eval_arg(_tree[1], _w));
            int_64 b = to_int_strict(walker::eval_arg(_tree[2], _w));
            if (_w.m_cfg.overflow_check && (b < 0 || b >= 64))
                throw script_exception{error_type::ShiftError, std::string("Shift count out of range")};
            return variant(wrap_shr(a, b));
        }

        variant walker::op_eq(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            if (a.is<int_64>() && b.is<int_64>())
                return variant(a.to<int_64>() == b.to<int_64>());
            return variant(eq_cmp(a, b));
        }

        variant walker::op_ne(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            variant b = walker::eval_arg(_tree[2], _w);
            if (a.is<int_64>() && b.is<int_64>())
                return variant(a.to<int_64>() != b.to<int_64>());
            return variant(!eq_cmp(a, b));
        }

#define DEFINE_ORD_CMP_OP(name, op)                                                                       \
    variant walker::op_##name(const varvec& _tree, walker& _w) {                                          \
        variant a = walker::eval_arg(_tree[1], _w);                                                       \
        variant b = walker::eval_arg(_tree[2], _w);                                                       \
        switch (type_pair(a, b)) {                                                                        \
        case type_pair<int_64, int_64>():                                                                 \
            return variant(a.to<int_64>() op b.to<int_64>());                                             \
        case type_pair<int_64, double>():                                                                 \
        case type_pair<double, int_64>():                                                                 \
        case type_pair<double, double>(): {                                                               \
            double da = a.is<double>() ? a.to<double>() : static_cast<double>(a.to<int_64>());            \
            double db = b.is<double>() ? b.to<double>() : static_cast<double>(b.to<int_64>());            \
            return variant(da op db);                                                                     \
        }                                                                                                 \
        case type_pair<std::string, std::string>():                                                       \
            return variant(a.to<std::string>() op b.to<std::string>());                                   \
        default:                                                                                          \
            throw script_exception{error_type::TypeError, std::string("Cannot compare different types")}; \
        }                                                                                                 \
    }
        DEFINE_ORD_CMP_OP(lt, <)
        DEFINE_ORD_CMP_OP(gt, >)
        DEFINE_ORD_CMP_OP(le, <=)
        DEFINE_ORD_CMP_OP(ge, >=)

#define DEFINE_BITWISE_OP(name, op)                               \
    variant walker::op_##name(const varvec& _tree, walker& _w) {  \
        int_64 a = to_int_strict(walker::eval_arg(_tree[1], _w)); \
        int_64 b = to_int_strict(walker::eval_arg(_tree[2], _w)); \
        return variant(a op b);                                   \
    }
        DEFINE_BITWISE_OP(bit_and, &)
        DEFINE_BITWISE_OP(bit_or, |)
        DEFINE_BITWISE_OP(bit_xor, ^)
        variant walker::op_bit_neg(const varvec& _tree, walker& _w) {
            variant a = walker::eval_arg(_tree[1], _w);
            return variant(~to_int_strict(a));
        }

        variant walker::op_and(const varvec& _tree, walker& _w) {
            if (!to_bool_strict(walker::eval_arg(_tree[1], _w))) return variant(false);
            return variant(to_bool_strict(walker::eval_arg(_tree[2], _w)));
        }

        variant walker::op_or(const varvec& _tree, walker& _w) {
            if (to_bool_strict(walker::eval_arg(_tree[1], _w))) return variant(true);
            return variant(to_bool_strict(walker::eval_arg(_tree[2], _w)));
        }

        variant walker::op_not(const varvec& _tree, walker& _w) {
            return variant(!to_bool_strict(walker::eval_arg(_tree[1], _w)));
        }

        variant walker::op_ternary(const varvec& _tree, walker& _w) {
            variant test = _w.walk_tree(_tree[1].to<varvec>());
            if (to_bool_strict(test))
                return _w.walk_tree(_tree[2].to<varvec>());
            else
                return _w.walk_tree(_tree[3].to<varvec>());
        }

        variant walker::op_comma(const varvec& _tree, walker& _w) {
            _w.walk_tree(_tree[1].to<varvec>());
            return _w.walk_tree(_tree[2].to<varvec>());
        }

        variant walker::op_dot(const varvec& _tree, walker& _w) {
            auto r = resolve_dot(_tree, _w.state);

            // data reached through a link is invisible at any depth, not only at the link's own slot
            if (r.link_owner && r.kind == TerminalKind::T_Variant)
                throw script_exception{error_type::TypeError,
                                       std::string("link namespace cannot be read as variable")};

            if (r.key.null() && r.kind == TerminalKind::T_Variant && r.parent) {
                if (r.parent->is<varvec>()) {
                    alx::varvec dummy;
                    return variant(static_cast<int_64>(r.parent->to(dummy).size()));
                }
                if (r.parent->is<varlst>()) {
                    alx::varlst dummy;
                    return variant(static_cast<int_64>(r.parent->to(dummy).size()));
                }
                if (r.parent->is<varmap>()) {
                    alx::varmap dummy;
                    return variant(static_cast<int_64>(r.parent->to(dummy).size()));
                }
                if (r.parent->is<std::string>()) {
                    std::string dummy;
                    return variant(static_cast<int_64>(r.parent->to(dummy).size()));
                }
                if (r.parent->is<bytes>()) {
                    bytes dummy;
                    return variant(static_cast<int_64>(r.parent->to(dummy).size()));
                }
            }

            // A byte element is not a variant*, so it is materialised as its byte value (0-255)
            if (r.key.is<int_64>() && r.kind == TerminalKind::T_Variant && r.parent &&
                (r.parent->is<std::string>() || r.parent->is<bytes>())) {
                int_64 i = r.key.to<int_64>();
                if (r.parent->is<std::string>()) {
                    std::string dummy;
                    const std::string& s = r.parent->to(dummy);
                    if (i == -1) i = static_cast<int_64>(s.size()) - 1;
                    if (i < 0 || static_cast<uint_64>(i) >= s.size())
                        throw script_exception{error_type::IndexError,
                                               std::string("string index out of range")};
                    return variant(static_cast<int_64>(static_cast<uint_8>(s[static_cast<uint_64>(i)])));
                }
                bytes dummy;
                const bytes& b = r.parent->to(dummy);
                if (i == -1) i = static_cast<int_64>(b.size()) - 1;
                if (i < 0 || static_cast<uint_64>(i) >= b.size())
                    throw script_exception{error_type::IndexError,
                                           std::string("bytes index out of range")};
                return variant(static_cast<int_64>(b[static_cast<uint_64>(i)]));
            }
            variant* p = r.get(true);
            if (p) {

                // reached through a link: the terminal hands back the link's own slot — its sub-data stays opaque
                if (p == r.parent && p->is<anyptr>() && anyptr_ex<impl_link>::as(p->to<anyptr>()))
                    throw script_exception{error_type::TypeError,
                                           std::string("link namespace cannot be read as variable")};
                return *p;
            }
            if (r.kind == TerminalKind::T_Slot || r.kind == TerminalKind::T_Callable) {
                std::string name = r.key.is<std::string>() ? r.key.to<std::string>() : std::string();
                throw script_exception{error_type::NameError, std::string("Undefined: " + name)};
            }

            if (r.kind == TerminalKind::T_Variant && r.parent) {

                if (r.key.null())
                    throw script_exception{error_type::TypeError,
                                           std::string("type does not support [null] index")};
                if (r.key.is<int_64>()) {
                    if (r.parent->is<varvec>() || r.parent->is<varlst>())
                        throw script_exception{error_type::IndexError, std::string("vec index out of range")};
                    throw script_exception{error_type::TypeError, std::string("type does not support []")};
                }
                if (r.key.is<std::string>() && !r.parent->is<varmap>())
                    throw script_exception{error_type::TypeError, std::string("not a map")};
            }
            return variant();
        }

        variant walker::op_index(const varvec& _tree, walker& _w) {

            // Fast path: index the live slot in place, where the generic path below copies the container
            if (_tree.size() == 3 && _tree[1].is_vec()) {
                auto& lhs = _tree[1].to<varvec>();
                variant* pv = nullptr;
                if (lhs.size() == 2 && lhs[0].is<OPTYPE>() &&
                    static_cast<op_enum>(lhs[0].to<OPTYPE>()) == O_LOAD &&
                    lhs[1].is<std::string>())
                    pv = _w.state.var_ptr(lhs[1].to<std::string>());
                if (pv) {

                    if (pv->is<varvec>() && _tree[2].is_vec()) {
                        auto& rhs = _tree[2].to<varvec>();
                        int_64 idx = 0;
                        bool have_idx = false;
                        if (rhs.size() == 1 && rhs[0].is<int_64>()) {
                            idx = rhs[0].to<int_64>();
                            have_idx = true;
                        } else if (rhs.size() == 2 && rhs[0].is<OPTYPE>() &&
                                   static_cast<op_enum>(rhs[0].to<OPTYPE>()) == O_LOAD) {
                            variant* pi = nullptr;
                            if (rhs[1].is<std::string>())
                                pi = _w.state.var_ptr(rhs[1].to<std::string>());
                            if (pi && pi->is<int_64>()) {
                                idx = pi->to<int_64>();
                                have_idx = true;
                            }
                        }
                        if (have_idx) {
                            auto& vec = pv->to<varvec>();
                            if (idx == -1) idx = static_cast<int_64>(vec.size()) - 1;
                            if (idx >= 0 && static_cast<size_t>(idx) < vec.size())
                                return vec[static_cast<size_t>(idx)];
                            throw script_exception{error_type::IndexError,
                                                   std::string("vec index out of range")};
                        }
                    }

                    if (pv->is<varmap>() && !_tree[2].is_vec() && _tree[2].is<std::string>()) {
                        std::string key = _tree[2].to<std::string>();
                        auto& m = pv->to<varmap>();
                        if (m.contain(key)) return m.value(key);
                        throw script_exception{error_type::KeyError, std::string("map key not found: " + key)};
                    }

                    if (pv->is<varmap>() && _tree[2].is_vec()) {
                        variant key = walker::eval_arg(_tree[2], _w);
                        // Re-resolved: evaluating the key may run script code and realloc the store
                        variant* pv2 = _w.state.var_ptr(lhs[1].to<std::string>());
                        if (pv2 && pv2->is<varmap>()) {
                            auto& m = pv2->to<varmap>();
                            if (key.null()) return variant(static_cast<int_64>(m.size()));
                            if (!key.is<std::string>())
                                throw script_exception{error_type::TypeError,
                                                       std::string("map key must be a string")};
                            std::string ks = key.to<std::string>();
                            if (m.contain(ks)) return m.value(ks);
                            throw script_exception{
                                error_type::KeyError,
                                std::string("map key not found: " + ks)};
                        }
                    }
                }
            }

            // The whole index chain evaluates left to right before the base is read: an index can
            // replace, resize or remove the container it indexes
            std::vector<const varvec*> chain;
            const varvec* node = &_tree;
            while (!node->empty() && (*node)[0].is<OPTYPE>() &&
                   static_cast<op_enum>((*node)[0].to<OPTYPE>()) == O_INDEX) {
                chain.push_back(node);
                node = &(*node)[1].to<varvec>();
            }
            std::vector<variant> keys(chain.size());
            for (size_t k = 0; k < chain.size(); k++)
                keys[k] = walker::eval_arg((*chain[chain.size() - 1 - k])[2], _w);

            variant obj = _w.walk_tree(*node);
            for (size_t k = 0; k < keys.size(); k++)
                obj = index_value(obj, keys[k]);
            return obj;
        }

        variant walker::op_slice(const varvec& _tree, walker& _w) {
            // the bounds can run script code: they evaluate before the container is read
            variant fv, tv;
            int_64 step;
            walker::slice_eval_bounds(_tree, _w, fv, tv, step);

            variant obj = _w.walk_tree(_tree[1].to<varvec>());
            int_64 from, to;
            walker::slice_apply_bounds(obj, fv, tv, step, from, to);

            if (obj.is<varvec>()) return slice_vec(obj.to<varvec>(), from, to, step);
            if (obj.is<varlst>()) return slice_lst(obj.to<varlst>(), from, to, step);
            if (obj.is<std::string>()) return slice_str(obj.to<std::string>(), from, to, step);
            return slice_bytes(obj.to<bytes>(), from, to, step);
        }

        // The bounds can run script code: evaluate them before any container pointer is taken
        void walker::slice_eval_bounds(const varvec& _tree, walker& _w,
                                       variant& _fv, variant& _tv, int_64& _step) {
            auto eval_bound = [&](const variant& _op) -> variant {
                if (_op.is<int_64>() || _op.null()) return _op;
                return walker::eval_arg(_op, _w);
            };

            _fv = eval_bound(_tree[2]);
            _tv = eval_bound(_tree[3]);
            _step = _tree[4].is<int_64>() ? _tree[4].to<int_64>()
                                          : cov_int(walker::eval_arg(_tree[4], _w));

            if (_step == 0)
                throw script_exception{error_type::ArgError,
                                       std::string("slice step cannot be zero")};
        }

        // Fold the bounds against the container size; a straddling step clamps, so the walk stays in range
        void walker::slice_apply_bounds(const variant& _obj, const variant& _fv, const variant& _tv,
                                        int_64& _step, int_64& _from, int_64& _to) {
            int_64 size = 0;
            if (_obj.is<varvec>()) size = static_cast<int_64>(_obj.to<varvec>().size());
            else if (_obj.is<varlst>()) size = static_cast<int_64>(_obj.to<varlst>().size());
            else if (_obj.is<std::string>()) size = static_cast<int_64>(_obj.to<std::string>().size());
            else if (_obj.is<bytes>()) size = static_cast<int_64>(_obj.to<bytes>().size());
            else
                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support [i, j] slice")};

            _from = _fv.null() ? size : cov_int(_fv);
            _to = _tv.null() ? size : cov_int(_tv);

            if (_from < 0) _from += size;
            if (_to < 0) _to += size;

            if (_from < 0 || _from > size || _to < 0 || _to > size)
                throw script_exception{error_type::IndexError,
                                       std::string("slice index out of range")};

            // a negative step starting at size would read one element past the end
            if (_step < 0 && _from == size) _from = size - 1;

            // a step beyond the span visits the same positions; the clamp keeps the arithmetic in range
            _step = _step > 0 ? alx::min_value(_step, size + 1) : alx::max_value(_step, -(size + 1));
        }

        variant walker::slice_vec(const varvec& _vec, int_64 _start, int_64 _end, int_64 _step) {
            varvec result;
            if (_step > 0) {
                for (int_64 i = _start; i < _end; i += _step)
                    result.push_back(_vec[static_cast<size_t>(i)]);
            } else {
                for (int_64 i = _start; i > _end; i += _step)
                    result.push_back(_vec[static_cast<size_t>(i)]);
            }
            return variant(std::move(result));
        }

        variant walker::slice_lst(const varlst& _lst, int_64 _start, int_64 _end, int_64 _step) {
            varlst result;
            int_64 abs_step = _step > 0 ? _step : -_step;
            // varlst is forward-only: walk it once in scan order, then flip for a reverse traversal
            int_64 idx = 0;
            for (auto& elem : _lst) {
                bool in_range = _step > 0 ? (idx >= _start && idx < _end)
                                          : (idx <= _start && idx > _end);
                if (in_range) {
                    int_64 dist = _step > 0 ? (idx - _start) : (_start - idx);
                    if (dist % abs_step == 0) result.push_back(elem);
                }
                if (_step > 0 && idx >= _end) break;
                if (_step < 0 && idx > _start) break;
                ++idx;
            }
            if (_step < 0) result.reverse();
            return variant(std::move(result));
        }

        variant walker::slice_str(const std::string& _str, int_64 _start, int_64 _end, int_64 _step) {
            std::string result;
            result.reserve(static_cast<size_t>(slice_count(_start, _end, _step)));
            for (int_64 i = _start; _step > 0 ? i < _end : i > _end; i += _step)
                result.push_back(_str[static_cast<size_t>(i)]);
            return variant(std::move(result));
        }

        variant walker::slice_bytes(const bytes& _buf, int_64 _start, int_64 _end, int_64 _step) {
            bytes result(static_cast<uint_64>(slice_count(_start, _end, _step)));
            uint_8* out = result.data();
            int_64 k = 0;
            for (int_64 i = _start; _step > 0 ? i < _end : i > _end; i += _step)
                out[k++] = _buf[static_cast<uint_64>(i)];
            return variant(std::move(result));
        }

        static const char* const s_delete_target =
            "delete target must be a variable, member access, or index expression";

        variant walker::op_del(const varvec& _tree, walker& _w) {
            auto& target = _tree[1];

            if (target.is<std::string>())
                return variant(del_name(target, _w));

            auto& subtree = target.to<varvec>();
            if (!subtree.empty() && subtree[0].is<OPTYPE>()) {
                switch (static_cast<op_enum>(subtree[0].to<OPTYPE>())) {
                case O_ILOAD: return variant(del_name(target, _w));
                case O_DOT: return variant(del_dot(subtree, _w));
                case O_INDEX: return variant(del_index(subtree, _w));
                default: break;
                }
            }
            throw script_exception{error_type::TypeError, std::string(s_delete_target)};
        }

        variant walker::op_excall(const varvec& _tree, walker& _w) {
            return invoke_extend(_w, _tree[1].to<std::string>(), _tree, 2);
        }

        // Every compound assignment writes through here, so a function binding can never be overwritten
        static void ass_write(walker& _w, variant* _p, variant&& _val) {
            if (_p->is<anyptr>() && anyptr_ex<call_able>::as(_p->to<anyptr>()))
                throw script_exception{error_type::NameError,
                                       std::string("Cannot assign to function")};
            *_p = std::move(_val);
        }

        variant walker::op_ass_add(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();

            switch (type_pair(*p, b)) {
            case type_pair<int_64, int_64>(): {
                int_64 ia = p->to<int_64>(), ib = b.to<int_64>();
                if (_w.m_cfg.overflow_check && ((ib > 0 && ia > max_int_64 - ib) ||
                                                (ib < 0 && ia < min_int_64 - ib)))
                    throw script_exception{error_type::OverflowError, std::string("Integer addition overflow")};
                int_64 r = wrap_add(ia, ib);
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            case type_pair<int_64, double>():
            case type_pair<double, int_64>():
            case type_pair<double, double>(): {
                variant r(p->to_number() + b.to_number());
                ass_write(_w, p, variant(r));
                return r;
            }
            case type_pair<std::string, std::string>(): {
                variant r(p->to<std::string>() + b.to<std::string>());
                ass_write(_w, p, variant(r));
                return r;
            }
            case type_pair<varvec, varvec>(): {
                varvec r = p->to<varvec>();
                const varvec& bv = b.to<varvec>();
                r.insert(r.end(), bv.begin(), bv.end());
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            case type_pair<varlst, varlst>(): {
                varlst r = p->to<varlst>();
                const varlst& bl = b.to<varlst>();
                r.insert(r.end(), bl.begin(), bl.end());
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            case type_pair<varmap, varmap>(): {
                varmap r(p->to<varmap>());
                const varmap& bm = b.to<varmap>();
                for (auto it = bm.cbegin(); it != bm.cend(); it++)
                    r[it.key()] = it.value();
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            default:
                throw script_exception{error_type::TypeError, std::string("Expected numbers, strings, arrays, lists or maps for +=")};
            }
        }

        variant walker::op_ass_sub(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            if (p->is<int_64>() && b.is<int_64>()) {
                int_64 ia = p->to<int_64>(), ib = b.to<int_64>();
                if (_w.m_cfg.overflow_check && ((ib < 0 && ia > max_int_64 + ib) ||
                                                (ib > 0 && ia < min_int_64 + ib)))
                    throw script_exception{error_type::OverflowError, std::string("Integer subtraction overflow")};
                int_64 r = wrap_sub(ia, ib);
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            if (p->is_number() && b.is_number()) {
                variant r(p->to_number() - b.to_number());
                ass_write(_w, p, variant(r));
                return r;
            }
            throw script_exception{error_type::TypeError, std::string("Expected numbers for -")};
        }

        variant walker::op_ass_mul(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            if (p->is<int_64>() && b.is<int_64>()) {
                int_64 ia = p->to<int_64>(), ib = b.to<int_64>();
                bool overflow = false;
                if (ia > 0) {
                    if (ib > 0 && ia > max_int_64 / ib) overflow = true;
                    else if (ib < 0 && ib < min_int_64 / ia) overflow = true;
                } else if (ia < 0) {
                    if (ib > 0 && ia < min_int_64 / ib) overflow = true;
                    else if (ib < 0 && ia < max_int_64 / ib) overflow = true;
                }
                if (_w.m_cfg.overflow_check && overflow)
                    throw script_exception{error_type::OverflowError, std::string("Integer multiplication overflow")};
                int_64 r = wrap_mul(ia, ib);
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            if (p->is_number() && b.is_number()) {
                variant r(p->to_number() * b.to_number());
                ass_write(_w, p, variant(r));
                return r;
            }
            throw script_exception{error_type::TypeError, std::string("Expected numbers for *")};
        }

        variant walker::op_ass_div(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            if (p->is<int_64>() && b.is<int_64>()) {
                int_64 ia = p->to<int_64>(), ib = b.to<int_64>();
                if (ib == 0) throw script_exception{error_type::DivZeroError, std::string("Division by zero")};
                if (ib == -1 && ia == min_int_64)
                    throw script_exception{error_type::DivOverflowError, std::string("Integer division overflow")};
                int_64 r = ia / ib;
                ass_write(_w, p, variant(r));
                return variant(r);
            }
            if (p->is_number() && b.is_number()) {
                double db = b.to_number();
                if (db == 0.0) throw script_exception{error_type::DivZeroError, std::string("Division by zero")};
                variant r(p->to_number() / db);
                ass_write(_w, p, variant(r));
                return r;
            }
            throw script_exception{error_type::TypeError, std::string("Expected numbers for /")};
        }

        variant walker::op_ass_mod(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 ai = to_int_strict(*p);
            int_64 bi = to_int_strict(b);
            if (bi == 0) throw script_exception{error_type::DivZeroError, std::string("Modulo by zero")};
            if (bi == -1 && ai == min_int_64)
                throw script_exception{error_type::DivOverflowError, std::string("Integer modulo overflow")};
            int_64 r = ai % bi;
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_ass_lshift(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 a = to_int_strict(*p);
            int_64 sb = to_int_strict(b);
            if (_w.m_cfg.overflow_check && (sb < 0 || sb >= 64))
                throw script_exception{error_type::ShiftError, std::string("Shift count out of range")};
            if (_w.m_cfg.overflow_check && (a < 0))
                throw script_exception{error_type::ShiftError, std::string("Left shift of negative value")};
            int_64 r = wrap_shl(a, sb);
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_ass_rshift(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 a = to_int_strict(*p);
            int_64 sb = to_int_strict(b);
            if (_w.m_cfg.overflow_check && (sb < 0 || sb >= 64))
                throw script_exception{error_type::ShiftError, std::string("Shift count out of range")};
            int_64 r = wrap_shr(a, sb);
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_ass_bit_and(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 a = to_int_strict(*p), bb = to_int_strict(b);
            int_64 r = a & bb;
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_ass_bit_or(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 a = to_int_strict(*p), bb = to_int_strict(b);
            int_64 r = a | bb;
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_ass_bit_xor(const varvec& _tree, walker& _w) {
            variant b = walker::eval_arg(_tree[2], _w);
            variant* p = resolve_slot(_tree[1].to<varvec>(), _w, true);
            if (!p) return variant();
            int_64 a = to_int_strict(*p), bb = to_int_strict(b);
            int_64 r = a ^ bb;
            ass_write(_w, p, variant(r));
            return variant(r);
        }

        variant walker::op_econst(const varvec& _tree, walker& _w) {
            std::string name = _tree[1].to<std::string>();
            if (!_w.state.m_engine->fid_define(name)) {
                if (_w.state.m_engine->fid_extend(name))
                    throw script_exception{error_type::TypeError,
                                           std::string("extension function cannot be read as value: $" + name)};
                throw script_exception{error_type::NameError,
                                       std::string("static definition not found: $" + name)};
            }
            return _w.state.m_engine->get_define(name);
        }

        static const char* const s_not_a_target =
            "assignment target must be a variable, member access, or index expression";

        walker::target_resolved walker::resolve_target(const varvec& _lhs, walker& _w, resolve_mode _mode,
                                                       bool _rmw, bool _tail_create) {
            target_resolved r;
            bool writing = _mode == resolve_mode::write;
            if (_lhs.empty()) return r;
            if (!_lhs[0].is<OPTYPE>()) {
                if (writing)
                    throw script_exception{error_type::TypeError, std::string(s_not_a_target)};
                return r;
            }

            // The container an element or slice write lands in: an addressable base resolves, anything
            // else is an rvalue and never a container
            impl_import* base_owner = nullptr;
            auto container_of = [&](const varvec& _t) -> variant* {
                if (_t.empty() || !_t[0].is<OPTYPE>()) return nullptr;
                auto op = static_cast<op_enum>(_t[0].to<OPTYPE>());
                if (op != O_LOAD && op != O_ILOAD && op != O_INDEX && op != O_DOT) return nullptr;
                target_resolved br = walker::resolve_target(_t, _w, _mode, _rmw, false);
                if (br.type == target_resolved::kind::slot) {
                    base_owner = br.owner;
                    return br.slot;
                }
                // a byte element is a value, not a container to index through
                if (br.type == target_resolved::kind::byte) {
                    if (!writing) return nullptr;
                    throw script_exception{error_type::TypeError, std::string("Type does not support [i]")};
                }
                return nullptr;
            };

            // A resolution failure: the write path reports it; a probe (nav) answers "not an address"
            // with none — the host asked for a slot, and anything else is simply not there
            auto fail = [&](error_type _type, std::string _info) -> target_resolved {
                if (!writing) return r;
                throw script_exception{_type, std::move(_info)};
            };

            switch (static_cast<op_enum>(_lhs[0].to<OPTYPE>())) {
            case O_LOAD: {
                variant* p = _w.state.var_ptr(_lhs[1].to<std::string>());
                if (!p) {
                    if (!writing) return r;
                    throw script_exception{error_type::NameError,
                                           std::string("Undefined: ") + _lhs[1].to<std::string>()};
                }

                r.type = target_resolved::kind::slot;
                r.slot = p;
                r.owner = _w.state.current;
                return r;
            }

            case O_ILOAD: {
                variant name_val = walker::eval_arg(_lhs[1], _w);
                if (!name_val.is<std::string>())
                    throw script_exception{error_type::TypeError, std::string("Indirect load: variable did not evaluate to a string")};
                std::string path = name_val.to<std::string>();
                if (walker::is_simple_name(path)) {
                    variant* p = _w.state.var_ptr(path);
                    if (!p) {
                        if (!writing) return r;
                        throw script_exception{error_type::NameError,
                                               std::string("Undefined: ") + path};
                    }
                    r.type = target_resolved::kind::slot;
                    r.slot = p;
                    r.owner = _w.state.current;
                    return r;
                }
                auto expr = walker::iload_parse_expr(path, _w.m_cfg.parse_depth);
                if (writing) check_reflection_chain(expr, path);
                return walker::resolve_target(expr, _w, _mode, _rmw, _tail_create);
            }

            case O_DOT: {
                auto dr = resolve_dot(_lhs, _w.state);
                r.owner = dr.slot_owner;

                // the write-side guards, keyed on the mode: a host probe is exempt, a script write is not
                if (writing && dr.kind == TerminalKind::T_Slot && dr.parent) {
                    auto& sv = *dr.parent;
                    if (sv.is<anyptr>()) {
                        const anyptr& ap_w = sv.to<anyptr>();
                        if (anyptr_ex<impl_link>::as(ap_w))
                            throw script_exception{error_type::TypeError, std::string("cannot write to link module variable")};
                    }
                }
                // a chain that entered a link's data resolves inside it at any depth: never a script target
                if (writing && dr.link_owner && dr.kind == TerminalKind::T_Variant)
                    throw script_exception{error_type::TypeError, std::string("cannot write to link module variable")};
                // a byte position first: [null] on a byte container is an element, not an append slot
                bool byte_pos = dr.kind == TerminalKind::T_Variant && dr.parent &&
                                (dr.key.null() || dr.key.is<int_64>()) &&
                                (dr.parent->is<std::string>() || dr.parent->is<bytes>());
                // a null key would append inside get(): reject it before the push for a read-modify-write
                if (_rmw && !byte_pos && dr.kind == TerminalKind::T_Variant && dr.parent && dr.key.null())
                    throw script_exception{error_type::TypeError,
                                           std::string("append index [null] is not a valid target for read-modify-write")};
                variant* p = dr.get(!_tail_create);
                if (p) {

                    // an existing area native names a registered function, not a writable slot
                    if (writing && dr.parent_kind == ParentKind::Area)
                        throw script_exception{error_type::NameError,
                                               std::string("Cannot assign to function: " +
                                                           (dr.key.is<std::string>() ? dr.key.to<std::string>() : std::string()))};
                    r.type = target_resolved::kind::slot;
                    r.slot = p;
                    return r;
                }
                if (dr.kind == TerminalKind::T_Variant && dr.parent) {

                    // a byte element is not a variant slot: the write path takes it as a byte position
                    // (a string key on a byte container stays the tail's "not a map", as the read side has it)
                    if (writing && byte_pos) {
                        r.type = target_resolved::kind::byte;
                        r.parent = dr.parent;
                        r.key = dr.key;
                        return r;
                    }
                    if (dr.key.null()) {

                        if (dr.parent->is<std::string>())
                            return fail(error_type::TypeError, "string does not support index write");
                        if (dr.parent->is<varmap>())
                            return fail(error_type::TypeError, "map does not support [null] write");
                        return fail(error_type::TypeError, "type does not support [null] write");
                    }
                    if (dr.key.is<int_64>()) {
                        if (dr.parent->is<varvec>() || dr.parent->is<varlst>())
                            return fail(error_type::IndexError, "vec index out of range");
                        if (dr.parent->is<std::string>())
                            return fail(error_type::TypeError, "string does not support index write");
                        return fail(error_type::TypeError, "type does not support []");
                    }
                    if (dr.key.is<std::string>() && !dr.parent->is<varmap>())
                        return fail(error_type::TypeError, "not a map");
                }
                if (dr.key.is<std::string>())
                    return fail(error_type::NameError, "Undefined: " + dr.key.to<std::string>());
                return fail(error_type::TypeError, s_not_a_target);
            }

            case O_SLICE: {
                // a probe gets none before the bounds run: a slice is not an address, and a path that
                // only fails must not run script code
                if (!writing) return r;
                // the bounds can run script code: they evaluate before the container is resolved
                variant fv, tv;
                int_64 step;
                walker::slice_eval_bounds(_lhs, _w, fv, tv, step);
                if (_rmw)
                    throw script_exception{error_type::TypeError,
                                           std::string("slice does not support compound assignment or increment")};
                variant* parent = container_of(_lhs[1].to<varvec>());
                if (!parent)
                    throw script_exception{error_type::TypeError, std::string("cannot assign to an rvalue")};
                if (!parent->is<varvec>() && !parent->is<varlst>() && !parent->is<std::string>() &&
                    !parent->is<bytes>())
                    throw script_exception{error_type::TypeError,
                                           std::string("Type does not support [i, j] slice")};
                int_64 from, to;
                walker::slice_apply_bounds(*parent, fv, tv, step, from, to);
                r.type = target_resolved::kind::slice;
                r.parent = parent;
                r.owner = base_owner;
                r.from = from;
                r.to = to;
                r.step = step;
                return r;
            }

            case O_INDEX: {
                // Every index on the chain evaluates left to right before any container pointer is
                // taken: an index can create, replace or resize the container it indexes
                std::vector<const varvec*> chain;
                const varvec* node = &_lhs;
                while (!node->empty() && (*node)[0].is<OPTYPE>() &&
                       static_cast<op_enum>((*node)[0].to<OPTYPE>()) == O_INDEX) {
                    chain.push_back(node);
                    node = &(*node)[1].to<varvec>();
                }
                std::vector<variant> keys(chain.size());
                for (size_t k = 0; k < chain.size(); k++)
                    keys[k] = walker::eval_arg((*chain[chain.size() - 1 - k])[2], _w);

                variant* v = container_of(*node);
                if (!v) {
                    if (!writing) return r;
                    throw script_exception{error_type::TypeError, std::string("cannot assign to an rvalue")};
                }
                r.owner = base_owner;

                // every level but the last must land on a container; nothing below runs script code
                for (size_t k = 0; k + 1 < keys.size(); k++) {
                    const variant& key = keys[k];
                    if (v->is<varvec>()) {
                        if (key.null())
                            return fail(error_type::TypeError, "cannot index an append position");
                        int_64 i = cov_int(key);
                        varvec& vec = v->as<varvec>();
                        if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
                        if (i < 0 || static_cast<uint_64>(i) >= vec.size())
                            return fail(error_type::IndexError, "vec index out of range");
                        v = &vec[static_cast<uint_64>(i)];
                        continue;
                    }
                    if (v->is<varlst>()) {
                        if (key.null())
                            return fail(error_type::TypeError, "cannot index an append position");
                        int_64 i = cov_int(key);
                        varlst& lst = v->as<varlst>();
                        if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
                        if (i < 0 || static_cast<uint_64>(i) >= lst.size())
                            return fail(error_type::IndexError, "lst index out of range");
                        auto it = lst.begin();
                        for (int_64 n = 0; n < i; ++n) ++it;
                        v = &*it;
                        continue;
                    }
                    if (v->is<varmap>()) {
                        if (!key.is<std::string>())
                            return fail(error_type::TypeError, "map key must be a string");
                        varmap& m = v->as<varmap>();
                        std::string ks = key.to<std::string>();
                        if (!m.contain(ks))
                            return fail(error_type::KeyError, "map key not found: " + ks);
                        v = &m[ks];
                        continue;
                    }
                    return fail(error_type::TypeError, "Type does not support [i]");
                }

                const variant& key = keys.back();
                if (v->is<varvec>()) {
                    varvec& vec = v->as<varvec>();
                    if (key.null()) {
                        if (_rmw)
                            throw script_exception{error_type::TypeError,
                                                   std::string("append index [null] is not a valid target for read-modify-write")};
                        if (!_tail_create)
                            return fail(error_type::TypeError, "cannot index an append position");
                        vec.push_back(variant(static_cast<int_64>(0)));
                        r.type = target_resolved::kind::slot;
                        r.slot = &vec.back();
                        return r;
                    }
                    int_64 i = cov_int(key);
                    if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
                    if (i < 0 || static_cast<uint_64>(i) >= vec.size())
                        return fail(error_type::IndexError, "vec index out of range");
                    r.type = target_resolved::kind::slot;
                    r.slot = &vec[static_cast<uint_64>(i)];
                    return r;
                }
                if (v->is<varlst>()) {
                    varlst& lst = v->as<varlst>();
                    if (key.null()) {
                        if (_rmw)
                            throw script_exception{error_type::TypeError,
                                                   std::string("append index [null] is not a valid target for read-modify-write")};
                        if (!_tail_create)
                            return fail(error_type::TypeError, "cannot index an append position");
                        lst.push_back(variant(static_cast<int_64>(0)));
                        r.type = target_resolved::kind::slot;
                        r.slot = &lst.back();
                        return r;
                    }
                    int_64 i = cov_int(key);
                    if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
                    if (i < 0 || static_cast<uint_64>(i) >= lst.size())
                        return fail(error_type::IndexError, "lst index out of range");
                    auto it = lst.begin();
                    for (int_64 n = 0; n < i; ++n) ++it;
                    r.type = target_resolved::kind::slot;
                    r.slot = &*it;
                    return r;
                }
                if (v->is<varmap>()) {
                    if (!key.is<std::string>())
                        return fail(error_type::TypeError, "map key must be a string");
                    varmap& m = v->as<varmap>();
                    std::string ks = key.to<std::string>();
                    if (!m.contain(ks) && !_tail_create)
                        return fail(error_type::KeyError, "map key not found: " + ks);
                    r.type = target_resolved::kind::slot;
                    r.slot = &m[ks];
                    return r;
                }
                if (writing && (v->is<std::string>() || v->is<bytes>())) {
                    r.type = target_resolved::kind::byte;
                    r.parent = v;
                    r.key = key;
                    return r;
                }
                if (v->is<std::string>())
                    return fail(error_type::TypeError, "string does not support index write");
                return fail(error_type::TypeError, "Type does not support [i]");
            }

            default:

                // a host probe gets none; an assignment target has to be an lvalue
                if (writing)
                    throw script_exception{error_type::TypeError, std::string(s_not_a_target)};
                return r;
            }
        }

        variant* walker::resolve_slot(const varvec& _lhs, walker& _w, bool _rmw) {

            // a read-modify-write reads first: a missing key is a refusal, not a slot to create
            target_resolved t = walker::resolve_target(_lhs, _w, resolve_mode::write, _rmw, false);
            if (t.type == target_resolved::kind::slot) return t.slot;
            if (t.type == target_resolved::kind::byte)
                throw script_exception{error_type::TypeError,
                                       std::string(t.parent->is<std::string>() ? "string" : "bytes") +
                                           " element does not support compound assignment or increment"};
            if (t.type == target_resolved::kind::slice)
                throw script_exception{error_type::TypeError,
                                       std::string("slice does not support compound assignment or increment")};
            return nullptr;
        }

        variant* walker::resolve_iload(const varvec& _lhs, walker& _w, bool _tail_create) {
            target_resolved t = walker::resolve_target(_lhs, _w, resolve_mode::nav, false, _tail_create);
            return t.type == target_resolved::kind::slot ? t.slot : nullptr;
        }

        void walker::store_slice(const target_resolved& _t, const variant& _val) {
            variant* parent = _t.parent;
            int_64 from = _t.from, to = _t.to, step = _t.step;
            int_64 count = slice_count(from, to, step);

            // The operand is the same container type, scattered in the read's own position order, and
            // its length must equal the position count; a fill is written as a constructed [v: N]
            if (parent->is<varvec>()) {
                if (!_val.is<varvec>()) throw script_exception{error_type::TypeError, slice_type_error("vec", _val)};
                const varvec& src = _val.to<varvec>();
                if (static_cast<int_64>(src.size()) != count)
                    throw script_exception{error_type::ArgError, slice_length_error(count, src.size())};
                auto& dst = parent->as<varvec>();
                int_64 k = 0;
                for (int_64 i = from; step > 0 ? i < to : i > to; i += step) {
                    dst[static_cast<uint_64>(i)] = src[static_cast<uint_64>(k++)];
                    bind_owner(dst[static_cast<uint_64>(i)], _t.owner);
                }
                return;
            }
            if (parent->is<varlst>()) {
                if (!_val.is<varlst>()) throw script_exception{error_type::TypeError, slice_type_error("lst", _val)};
                const varlst& src = _val.to<varlst>();
                if (static_cast<int_64>(src.size()) != count)
                    throw script_exception{error_type::ArgError, slice_length_error(count, src.size())};

                // forward-only: the target positions are collected in the read's order first, then assigned
                auto& dst = parent->as<varlst>();
                std::vector<variant*> targets(static_cast<size_t>(count));
                int_64 idx = 0, found = 0;
                for (auto it = dst.begin(); it != dst.end(); ++it, ++idx) {
                    bool hit = step > 0 ? (idx >= from && idx < to && (idx - from) % step == 0)
                                        : (idx <= from && idx > to && (from - idx) % (-step) == 0);
                    if (!hit) continue;
                    targets[static_cast<size_t>((idx - from) / step)] = &*it;
                    ++found;
                }
                if (found != count)
                    throw script_exception{error_type::IndexError, std::string("slice index out of range")};
                auto rit = src.begin();
                for (int_64 k = 0; k < count; ++k) {
                    *targets[static_cast<size_t>(k)] = *rit;
                    bind_owner(*targets[static_cast<size_t>(k)], _t.owner);
                    ++rit;
                }
                return;
            }
            if (parent->is<std::string>()) {
                if (!_val.is<std::string>())
                    throw script_exception{error_type::TypeError, slice_type_error("string", _val)};
                const std::string& src = _val.to<std::string>();
                if (static_cast<int_64>(src.size()) != count)
                    throw script_exception{error_type::ArgError, slice_length_error(count, src.size())};
                auto& dst = parent->as<std::string>();
                int_64 k = 0;
                for (int_64 i = from; step > 0 ? i < to : i > to; i += step)
                    dst[static_cast<uint_64>(i)] = src[static_cast<uint_64>(k++)];
                return;
            }
            if (!_val.is<bytes>()) throw script_exception{error_type::TypeError, slice_type_error("bytes", _val)};
            const bytes& src = _val.to<bytes>();
            if (static_cast<int_64>(src.size()) != count)
                throw script_exception{error_type::ArgError, slice_length_error(count, src.size())};
            auto& dst = parent->as<bytes>();
            int_64 k = 0;
            for (int_64 i = from; step > 0 ? i < to : i > to; i += step)
                dst[static_cast<uint_64>(i)] = src[static_cast<uint_64>(k++)];
        }

        void walker::store_raw(walker& _w, const variant& _name, variant&& _init_val) {
            std::string name = _name.to<std::string>();
            walker::check_name_conflict(_w, name);
            if (_w.state.frames.empty()) {
                variant* p = _w.state.current->m_store.store(name, std::move(_init_val));
                bind_owner(*p, _w.state.current);
            } else {
                _w.state.frames.back().store(name, std::move(_init_val));
            }
        }

        // a function binding is not an assignable name
        static void assign_name_guard(const variant& _p, const std::string& _name) {
            if (!_p.is<anyptr>()) return;
            const anyptr& ap = _p.to<anyptr>();
            if (anyptr_ex<call_able>::as(ap))
                throw script_exception{error_type::NameError, std::string("Cannot assign to function: " + _name)};
        }

        void walker::assign_raw(walker& _w, const variant& _name, const variant& _val) {
            std::string name = _name.to<std::string>();
            variant* p = _w.state.var_ptr(name);
            if (!p) throw script_exception{error_type::NameError, std::string("Undefined: " + name)};

            assign_name_guard(*p, name);
            *p = _val;
            bind_owner(*p, _w.state.current);
        }

        void walker::assign_raw(walker& _w, const variant& _name, variant&& _val) {
            std::string name = _name.to<std::string>();
            variant* p = _w.state.var_ptr(name);
            if (!p) throw script_exception{error_type::NameError, std::string("Undefined: " + name)};

            assign_name_guard(*p, name);
            *p = std::move(_val);
            bind_owner(*p, _w.state.current);
        }

        const varvec* walker::find_def(walker& _w, const std::string& _name) {
            if (!_w.state.current) return nullptr;
            variant* v = _w.state.var_ptr(_name);
            if (v && v->is<anyptr>()) {
                const anyptr& ap = v->to<anyptr>();
                if (auto* ca = anyptr_ex<call_able>::as(ap)) {
                    if (ca->is_def()) return ca->m_def;
                }
            }
            return nullptr;
        }

        variant walker::eval_arg(const variant& _node, walker& _w) {
            if (!_node.is_vec()) return _node;
            auto& v = _node.to<varvec>();

            if (v.size() == 2 && v[0].is<OPTYPE>() &&
                static_cast<op_enum>(v[0].to<OPTYPE>()) == O_LOAD) {

                if (v[1].is<std::string>()) {
                    variant* p = _w.state.var_ptr(v[1].to<std::string>());
                    if (!p) throw script_exception{error_type::NameError,
                                                   std::string("Undefined: " + v[1].to<std::string>())};
                    return *p;
                }
            }

            if (!v.empty() && !v[0].is<OPTYPE>()) return v[0];
            return _w.walk_tree(v);
        }

        std::vector<variant*> walker::collect_native_args(const varvec& _tree,
                                                          size_t _arg_start,
                                                          walker& _w,
                                                          std::list<variant>& _tmp) {
            std::vector<variant*> ptrs;
            ptrs.reserve(_tree.size() - _arg_start);

            // Two passes: non-variable args are evaluated into _tmp (a list, so the pointers stay valid),
            // then the variable names that were deferred get resolved once every evaluation has run
            for (size_t ai = _arg_start; ai < _tree.size(); ai++) {
                const variant& node = _tree[ai];
                if (node.is_vec()) {
                    auto& v = node.to<varvec>();
                    if (v.size() == 2 && v[0].is<OPTYPE>() &&
                        static_cast<op_enum>(v[0].to<OPTYPE>()) == O_LOAD &&
                        v[1].is<std::string>()) {
                        const std::string& name = v[1].to<std::string>();
                        if (!_w.state.var_ptr(name))
                            throw script_exception{error_type::NameError,
                                                   std::string("Undefined: " + name)};
                        ptrs.push_back(nullptr);
                        continue;
                    }
                }
                if (node.is_vec())
                    _tmp.push_back(_w.walk_tree(node.to<varvec>()));
                else
                    _tmp.push_back(node);
                ptrs.push_back(&_tmp.back());
            }

            for (size_t ai = _arg_start; ai < _tree.size(); ai++) {
                size_t pos = ai - _arg_start;
                if (ptrs[pos]) continue;
                const std::string& name = _tree[ai].to<varvec>()[1].to<std::string>();
                variant* p = _w.state.var_ptr(name);
                if (!p)
                    throw script_exception{error_type::NameError,
                                           std::string("Undefined: " + name)};
                ptrs[pos] = p;
            }
            return ptrs;
        }

        bool walker::is_bare_ext_name(const std::string& _s) {
            if (_s.empty() || (!isalpha((unsigned char) _s[0]) && _s[0] != '_')) return false;
            for (char c : _s)
                if (!isalnum((unsigned char) c) && c != '_') return false;
            return true;
        }

        variant walker::invoke_extend(walker& _w, const std::string& _name,
                                      const varvec& _tree, size_t _arg_start) {
            native_func handler = _w.state.m_engine->get_extend(_name);
            if (!handler)
                throw script_exception{error_type::NameError,
                                       std::string("extension function not found: $" + _name)};
            std::list<variant> tmp;
            variant ret_slot;
            fwrap_impl fw(walker::collect_native_args(_tree, _arg_start, _w, tmp),
                          &ret_slot, &_w.state.current->m_store, &_w);
            handler(fw);
            return ret_slot;
        }

        void walker::eval_and_bind_args(impl_import* _ent, const varvec& _def,
                                        const std::string& _func_name,
                                        const varvec& _tree, walker& _w,
                                        bool _tco, scope_frame* _tco_frame) {
            auto& params = _def[2].to<varvec>();

            std::vector<variant> pos_args;
            pos_args.reserve(_tree.size() > 2 ? _tree.size() - 2 : 0);

            bool has_spread = false;
            bool is_map_spread = false;
            size_t spread_idx = 0;
            varvec spread_vec;
            varmap spread_map;

            for (size_t ai = 2; ai < _tree.size(); ai++) {
                if (_tree[ai].is<varvec>()) {
                    auto& node = _tree[ai].to<varvec>();
                    if (!node.empty() && node[0].is<OPTYPE>() &&
                        static_cast<op_enum>(node[0].to<OPTYPE>()) == O_UNPACK) {
                        variant packed = _w.walk_tree(node[1].to<varvec>());
                        switch (packed.type()) {
                        case variant::id<varmap>():
                            spread_map = packed.to<varmap>();
                            is_map_spread = true;
                            has_spread = true;
                            break;
                        case variant::id<varvec>():
                            spread_vec = std::move(packed.to<varvec>());
                            has_spread = true;
                            break;
                        case variant::id<varlst>():
                            for (auto& e : packed.to<varlst>()) spread_vec.push_back(std::move(e));
                            has_spread = true;
                            break;
                        default:
                            throw script_exception{error_type::TypeError,
                                                   std::string("spread []: expected vec, lst, or map")};
                        }
                    } else if (node.size() == 2 && node[0].is<OPTYPE>() &&
                               static_cast<op_enum>(node[0].to<OPTYPE>()) == O_LOAD &&
                               node[1].is<std::string>()) {

                        const std::string& name = node[1].to<std::string>();
                        if (!_w.state.var_ptr(name))
                            throw script_exception{error_type::NameError,
                                                   std::string("Undefined: " + name)};
                        pos_args.push_back(variant());
                    } else {
                        pos_args.push_back(walker::eval_arg(_tree[ai], _w));
                    }
                } else {
                    pos_args.push_back(walker::eval_arg(_tree[ai], _w));
                }
            }

            // Runs before the frame push: after it the entity barrier hides caller locals and a TCO resize wipes slots
            size_t pos = 0;
            for (size_t ai = 2; ai < _tree.size(); ai++) {
                if (_tree[ai].is_vec()) {
                    const varvec& node = _tree[ai].to<varvec>();
                    if (!node.empty() && node[0].is<OPTYPE>() &&
                        static_cast<op_enum>(node[0].to<OPTYPE>()) == O_UNPACK)
                        continue;
                    if (node.size() == 2 && node[0].is<OPTYPE>() &&
                        static_cast<op_enum>(node[0].to<OPTYPE>()) == O_LOAD && node[1].is<std::string>()) {
                        const std::string& name = node[1].to<std::string>();
                        variant* p = _w.state.var_ptr(name);
                        if (!p)
                            throw script_exception{error_type::NameError,
                                                   std::string("Undefined: " + name)};
                        pos_args[pos] = *p;
                    }
                }
                ++pos;
            }

            const char* tag = _tco ? "Tail call: " : "";
            scope_frame* frame;
            if (_tco) {

                // The reused frame is the callee's, not frames.back(): a tail call inside a block has block frames on top
                auto& cf = _tco_frame ? *_tco_frame : _w.state.frames.back();
                cf.ent->m_store.m_data.resize(cf.base);
                cf.var_map.clear();
                cf.free.clear();
                frame = &cf;
            } else {
                auto& sf = _w.state.push_frame(_ent, FF_RET | FF_BREAK | FF_CONT | FF_TAIL);
                sf.def = &_def;
                _w.state.current = _ent;
                frame = &sf;
            }

            size_t pos_i = 0;
            size_t spread_used = 0;

            for (size_t pi = 0; pi < params.size(); pi++) {
                const std::string& pname = params[pi].to<std::string>();

                if (pos_i < pos_args.size()) {
                    frame->store(pname, std::move(pos_args[pos_i++]));
                    continue;
                }

                if (has_spread) {
                    if (is_map_spread) {
                        if (spread_map.contain(pname)) {
                            frame->store(pname, std::move(spread_map[pname]));
                            spread_used++;
                            continue;
                        }
                    } else {
                        if (spread_idx < spread_vec.size()) {
                            frame->store(pname, std::move(spread_vec[spread_idx++]));
                            spread_used++;
                            continue;
                        }
                    }
                }

                if (_def[3].is<varmap>()) {
                    auto& defmap = _def[3].to<varmap>();
                    if (defmap.contain(pname)) {
                        auto dv = defmap.value(pname);
                        frame->store(pname, dv.is<varvec>() ? _w.walk_tree(dv.to<varvec>()) : dv);
                        continue;
                    }
                }

                throw script_exception{error_type::ArgError,
                                       std::string(tag) + "Missing argument: " + pname +
                                           (_func_name.empty() ? "" : " for " + _func_name)};
            }

            if (pos_i < pos_args.size())
                throw script_exception{error_type::ArgError,
                                       std::string(tag) + "Too many arguments" +
                                           (_func_name.empty() ? "" : " for " + _func_name)};

            if (has_spread && spread_used == 0)
                throw script_exception{error_type::ArgError,
                                       std::string("spread [] contributed no values for " + _func_name)};
        }

        variant walker::invoke_def(impl_import* _ent, const varvec& _def,
                                   const std::string& _func_name,
                                   const varvec& _tree, walker& _w) {
            walker::eval_and_bind_args(_ent, _def, _func_name, _tree, _w, false);

            // The def frame is already the body's scope, so the body block is walked without another frame
            auto& body = _def[4].to<varvec>();
            variant result = walker::walk_body(body, _w);

            while (_w.state.tail_flag) {
                _w.state.tail_flag = false;
                _w.state.ret_flag = false;
                result = walker::walk_body(body, _w);
            }

            _w.state.pop_frame();
            _w.state.current = _w.state.frames.empty() ? _w.state.root : _w.state.frames.back().ent;
            return result;
        }

        variant walker::invoke_dot(const dot_resolved& r, const varvec& _tree, walker& _w) {
            std::string func_name = r.key.is<std::string>() ? r.key.to<std::string>() : "";
            if (func_name.empty())
                throw script_exception{error_type::NameError, std::string("empty function name")};

            if (r.kind == TerminalKind::T_Callable && r.parent) {
                auto& v = *r.parent;
                if (v.is<anyptr>()) {
                    const anyptr& ap = v.to<anyptr>();
                    if (auto* ca = anyptr_ex<call_able>::as(ap)) {
                        if (ca->is_def()) {
                            impl_import* ent = r.slot_owner ? r.slot_owner : _w.state.current;
                            return walker::invoke_def(ent, *ca->m_def, func_name, _tree, _w);
                        }
                        if (ca->is_native()) {
                            std::list<variant> tmp;
                            variant ret_slot;
                            fwrap_impl fw(walker::collect_native_args(_tree, 2, _w, tmp),
                                          &ret_slot, &_w.state.current->m_store, &_w);
                            ca->m_fn(fw);
                            return ret_slot;
                        }
                    }
                }
                throw script_exception{error_type::NameError,
                                       std::string("Undefined function: " + func_name)};
            }

            if (r.kind == TerminalKind::T_Slot) {
                if (!r.parent) {

                    if (r.slot_owner && r.key.is<std::string>()) {
                        impl_import* saved = _w.state.current;
                        _w.state.current = r.slot_owner;
                        const varvec* def = walker::find_def(_w, func_name);
                        _w.state.current = saved;
                        if (def) return walker::invoke_def(r.slot_owner, *def, func_name, _tree, _w);
                    }
                    throw script_exception{error_type::NameError,
                                           std::string("Undefined function: " + func_name)};
                }
                auto& v = *r.parent;
                if (v.is<anyptr>()) {
                    const anyptr& ap = v.to<anyptr>();

                    if (auto* link = anyptr_ex<impl_link>::as(ap)) {
                        variant* lv = link->m_store.find(func_name);
                        if (lv && lv->is<anyptr>()) {
                            const anyptr& lap = lv->to<anyptr>();
                            if (auto* ca = anyptr_ex<call_able>::as(lap)) {
                                if (ca->is_native()) {
                                    std::list<variant> tmp;
                                    variant ret_slot;
                                    fwrap_impl fw(walker::collect_native_args(_tree, 2, _w, tmp),
                                                  &ret_slot, &link->m_store, &_w);
                                    ca->m_fn(fw);
                                    return ret_slot;
                                }
                            }
                        }
                        throw script_exception{error_type::NameError,
                                               std::string("Undefined function: " + func_name)};
                    }

                    if (auto* area = anyptr_ex<link_area>::as(ap)) {
                        auto ni = area->m_natives.find(func_name);
                        if (ni != area->m_natives.end() && ni->second.is<anyptr>()) {
                            auto* ca = anyptr_ex<call_able>::as(ni->second.to<anyptr>());
                            if (ca && ca->is_native()) {
                                std::list<variant> tmp;
                                variant ret_slot;
                                data_store* store = r.link_owner ? &r.link_owner->m_store : &_w.state.current->m_store;
                                fwrap_impl fw(walker::collect_native_args(_tree, 2, _w, tmp),
                                              &ret_slot, store, &_w);
                                fw.m_area = ca->m_area;
                                ca->m_fn(fw);
                                return ret_slot;
                            }
                        }
                        throw script_exception{error_type::NameError,
                                               std::string("Undefined function: " + func_name)};
                    }
                }

                {
                    impl_import* saved = _w.state.current;
                    if (r.slot_owner) _w.state.current = r.slot_owner;
                    const varvec* def = walker::find_def(_w, func_name);
                    _w.state.current = saved;
                    if (def) return walker::invoke_def(r.slot_owner ? r.slot_owner : saved,
                                                       *def, func_name, _tree, _w);
                }
            }

            throw script_exception{error_type::NameError,
                                   std::string("Undefined function: " + func_name)};
        }

        bool walker::del_name(const variant& _target, walker& _w) {
            std::string name;
            if (_target.is<std::string>()) {
                name = _target.to<std::string>();
            } else {

                auto& iload_tree = _target.to<varvec>();
                variant name_val = walker::eval_arg(iload_tree[1], _w);
                if (!name_val.is<std::string>())
                    throw script_exception{error_type::TypeError,
                                           std::string("delete @: expression did not evaluate to a string")};
                name = name_val.to<std::string>();

                if (!walker::is_simple_name(name)) {
                    auto expr = walker::iload_parse_expr(name, _w.m_cfg.parse_depth);
                    check_reflection_chain(expr, name);
                    op_enum head = static_cast<op_enum>(expr[0].to<OPTYPE>());
                    if (head == O_DOT) return del_dot(expr, _w);
                    if (head == O_INDEX) return del_index(expr, _w);
                    return del_name(expr, _w);
                }
            }

            // the module store is reachable from the module top layer only: a frame deletes its own vars only
            if (_w.state.frames.empty()) {

                if (_w.state.current->m_store.contain(name)) {
                    _w.state.current->m_store.remove(name);
                    return true;
                }
                return false;
            }
            return _w.state.frames.back().remove(name);
        }

        bool walker::del_dot(const varvec& _tree, walker& _w) {
            if (_tree.size() < 3) return false;
            dot_resolved r = resolve_dot(_tree, _w.state);

            if (r.kind == TerminalKind::T_Callable) {
                throw script_exception{error_type::TypeError,
                                       std::string("cannot delete callable via dot")};
            }

            if (r.key.null())
                throw script_exception{error_type::ConvError,
                                       std::string("Cannot delete with null index")};

            std::string kstr = r.key.is<std::string>() ? r.key.to<std::string>() : std::string();
            bool ok = false;

            // any terminal reached through link data is data: undeletable whatever it holds or how deep it sits
            if (r.link_owner && r.parent_kind != ParentKind::Area)
                throw script_exception{error_type::TypeError,
                                       std::string("cannot delete link data variable") +
                                           (kstr.empty() ? std::string() : ": " + kstr)};

            switch (r.parent_kind) {
            case ParentKind::Frame:

                if (r.parent)
                    throw script_exception{error_type::TypeError,
                                           std::string("Type does not support delete ." + kstr)};
                if (!_w.state.frames.empty())
                    ok = _w.state.frames.back().remove(kstr);
                break;

            case ParentKind::Entity:

                if (r.parent)
                    throw script_exception{error_type::TypeError,
                                           std::string("Type does not support delete ." + kstr)};
                if (r.slot_owner != _w.state.current)
                    throw script_exception{error_type::NameError,
                                           std::string("cannot delete across module boundary")};
                if (_w.state.frames.empty() && r.slot_owner->m_store.contain(kstr)) {
                    r.slot_owner->m_store.remove(kstr);
                    ok = true;
                }
                break;

            case ParentKind::Link: {

                auto* link = r.link_owner;
                if (!link && r.parent && r.parent->is<anyptr>()) {
                    const anyptr& ap = r.parent->to<anyptr>();
                    link = anyptr_ex<impl_link>::as(ap);
                }
                if (link) {
                    variant* lv = link->m_store.find(kstr);
                    if (lv) {
                        if (lv->is<anyptr>()) {

                            if (anyptr_ex<call_able>::as(lv->to<anyptr>()))
                                throw script_exception{error_type::TypeError,
                                                       std::string("cannot delete function: " + kstr)};
                            link->m_store.remove(kstr);
                            ok = true;
                        } else {
                            throw script_exception{error_type::TypeError,
                                                   std::string("cannot delete link data variable: " + kstr)};
                        }
                    }
                }
                break;
            }
            case ParentKind::Area: {

                throw script_exception{error_type::TypeError,
                                       std::string("cannot delete function: " + kstr)};
            }

            case ParentKind::Value:

                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support delete [i]")};

            case ParentKind::Map: {

                if (!r.parent || !r.parent->is<varmap>())
                    throw script_exception{error_type::TypeError,
                                           std::string("Type does not support delete [i]")};
                if (!r.key.is<std::string>())
                    throw script_exception{error_type::TypeError,
                                           std::string("map key must be a string")};
                alx::varmap dummy;
                auto& m = r.parent->to(dummy);
                if (m.contain(kstr)) {
                    m.erase(kstr);
                    ok = true;
                }
                break;
            }
            case ParentKind::Vec:
                ok = del_vec(*r.parent, r.key);
                break;

            case ParentKind::Lst:
                ok = del_lst(*r.parent, r.key);
                break;

            default: break;
            }

            if (!ok && r.kind == TerminalKind::T_Slot && !r.parent && r.slot_owner && r.key.is<std::string>()) {
                if (r.slot_owner != _w.state.current)
                    throw script_exception{error_type::NameError,
                                           std::string("cannot delete across module boundary")};
                if (_w.state.frames.empty() && r.slot_owner->m_store.contain(kstr)) {
                    r.slot_owner->m_store.remove(kstr);
                    ok = true;
                }
            }
            return ok;
        }

        bool walker::del_index(const varvec& _tree, walker& _w) {
            auto& obj_load = _tree[1].to<varvec>();
            variant idx = walker::eval_arg(_tree[2], _w);

            if (!(obj_load[0].is<OPTYPE>() && static_cast<op_enum>(obj_load[0].to<OPTYPE>()) == O_LOAD))
                throw script_exception{error_type::TypeError,
                                       std::string("Cannot delete from temporary index")};

            std::string vname = obj_load[1].to<std::string>();
            variant* v = _w.state.var_ptr(vname);
            if (!v) return false;

            switch (v->type()) {
            case variant::id<varvec>(): return del_vec(*v, idx);
            case variant::id<varlst>(): return del_lst(*v, idx);
            case variant::id<varmap>(): return del_map(*v, idx);
            default:
                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support delete [i]")};
            }
        }

        bool walker::del_vec(variant& _container, const variant& _key) {

            // to(dummy) hands back the dummy on a type mismatch, so each helper re-checks the container type
            if (!_container.is<varvec>())
                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support delete [i]")};
            if (_key.null())
                throw script_exception{error_type::ConvError, std::string("Cannot delete with null index")};
            alx::varvec dummy;
            auto& vec = _container.to(dummy);
            int_64 i = cov_int(_key);
            if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
            if (i < 0 || static_cast<size_t>(i) >= vec.size()) return false;
            vec.erase(vec.begin() + static_cast<ptrdiff_t>(i));
            return true;
        }

        bool walker::del_lst(variant& _container, const variant& _key) {
            if (!_container.is<varlst>())
                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support delete [i]")};
            if (_key.null())
                throw script_exception{error_type::ConvError, std::string("Cannot delete with null index")};
            alx::varlst dummy;
            auto& lst = _container.to(dummy);
            int_64 i = cov_int(_key);
            if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
            if (i < 0 || static_cast<size_t>(i) >= lst.size()) return false;
            auto it = lst.begin();
            for (int_64 n = 0; n < i; ++n) ++it;
            lst.erase(it);
            return true;
        }

        bool walker::del_map(variant& _container, const variant& _key) {
            if (!_container.is<varmap>())
                throw script_exception{error_type::TypeError,
                                       std::string("Type does not support delete [i]")};
            if (_key.null())
                throw script_exception{error_type::ConvError, std::string("Cannot delete with null index")};
            if (!_key.is<std::string>())
                throw script_exception{error_type::TypeError, std::string("map key must be a string")};
            alx::varmap dummy;
            auto& map = _container.to(dummy);
            std::string key = _key.to<std::string>();
            if (!map.contain(key)) return false;
            map.erase(key);
            return true;
        }

        std::string walker::find_func_name(impl_import* _ent, const varvec* _def) {
            if (!_ent || !_def) return {};
            for (auto& _p : _ent->m_store.m_map) {
                if (_p.second >= _ent->m_store.m_data.size()) continue;
                auto& dv = _ent->m_store.m_data[_p.second];
                if (!dv.is<anyptr>()) continue;
                const anyptr& dap = dv.to<anyptr>();
                if (auto* dca = anyptr_ex<call_able>::as(dap)) {
                    if (dca->is_def() && dca->m_def == _def) return _p.first;
                }
            }
            return std::string("?");
        }

        static std::string file_base(const std::string& _p) {
            if (_p.empty()) return std::string();
            auto pos = _p.rfind('/');
            return (pos != std::string::npos && pos + 1 < _p.size()) ? _p.substr(pos + 1) : _p;
        }

        std::string walker::pos_file(const scope_frame* _f, impl_import* _root) {
            if (!_f) {

                if (!_root || !_root->m_fly || !_root->m_fly->m_ast) return std::string();
                return file_base(_root->m_fly->m_path);
            }
            if (!_f->def) return std::string();
            if (!_f->ent || !_f->ent->m_fly || _f->ent == _root) return std::string();
            return file_base(_f->ent->m_fly->m_path);
        }

        std::string walker::pos_func(const scope_frame* _f) {
            if (!_f || !_f->ent) return std::string();
            if (_f->def) return find_func_name(_f->ent, _f->def);
            return (_f->flags & FF_RET) ? std::string("eval") : std::string("?");
        }

        varmap walker::pos_map(const scope_frame* _f, const walk_state& _st, impl_import* _root) {
            varmap m;
            const src_pos& p = _f ? _f->pos : _st.top_pos;
            m["row"] = variant(static_cast<int_64>(p.row));
            m["col"] = variant(static_cast<int_64>(p.col));
            m["ofst"] = variant(static_cast<int_64>(p.ofst));
            m["file"] = variant(pos_file(_f, _root));
            return m;
        }

        std::string walker::walk_state_trace(walker& _w) {

            auto item = [](uint_32 _row, uint_32 _col, const std::string& _ent,
                           const std::string& _name) {
                std::string s = _ent.empty() ? "[::]" : ("[" + _ent + "]");
                if (!_name.empty()) s += " " + _name;
                if (_row) s += ":" + std::to_string(_row) + ":" + std::to_string(_col);
                return s;
            };
            std::vector<std::string> seq;

            for (auto it = _w.state.frames.rbegin(); it != _w.state.frames.rend(); ++it) {
                if (!(it->flags & FF_RET) && !it->pos.row) continue;
                seq.push_back(item(it->pos.row, it->pos.col, pos_file(&*it, _w.state.root_entity),
                                   pos_func(&*it)));
            }

            seq.push_back(item(_w.state.top_pos.row, _w.state.top_pos.col,
                               pos_file(nullptr, _w.state.root_entity), std::string()));

            auto compressed = compress_trace(seq);

            std::string trace;
            format_trace(compressed, trace, "");
            return trace;
        }

        bool walker::call_hook(walker& _w, hook_event _type, variant _info, bool _clear_insn) {
            hook_fn fn = _w.m_hook.load(std::memory_order_relaxed);
            if (!fn) return true;
            if (_clear_insn) _w.m_insn = 0;
            hook_info hi;
            hi.type = _type;
            hi.info = std::move(_info);
            hi.hkdt = _w.m_hook_ud.load(std::memory_order_relaxed);
            hi.desc = &_w.m_interrupt_desc;
            uint_64 cur = _w.m_hook_interval.load(std::memory_order_relaxed);
            hi.freq = cur;
            hi.wkdt = &_w;
            bool cont = true;
            try {
                cont = fn(hi);
            } catch (...) {
                cont = false;
                _w.m_interrupt_desc = "hook callback threw exception";
            }
            if (hi.freq != cur) _w.m_hook_interval.store(hi.freq, std::memory_order_relaxed);
            if (!cont) _w.m_interrupted = true;
            return cont;
        }

        bool walker::check_dependencies(const bytes_view& _data, walker& _w,
                                        std::list<std::string>& _missing_imports,
                                        std::list<std::string>& _missing_links) {
            compile_result cr;
            if (!get_dependencies(_data, cr)) return false;
            auto& paths = _w.state.m_search_paths ? *_w.state.m_search_paths
                                                  : std::list<std::string>();
            auto exists = [&](const std::string& _p) {
                if (file_info::is_absolute(_p)) return file_info(_p).is_exist();
                for (auto& sp : paths)
                    if (file_info(sp + "/" + _p).is_exist()) return true;
                return file_info(_p).is_exist();
            };
            bool ok = true;
            for (auto& p : cr.imports) {
                if (!exists(p)) {
                    _missing_imports.push_back(p);
                    ok = false;
                }
            }
            for (auto& p : cr.links) {
                if (!exists(p)) {
                    _missing_links.push_back(p);
                    ok = false;
                }
            }
            return ok;
        }

        void walker::check_name_conflict(walker& _w, const std::string& _name) {
            impl_import* ent = _w.state.current;
            if (!ent) return;

            if (!_w.state.frames.empty()) {
                auto& cf = _w.state.frames.back();
                uint_64 vi = cf.var_map.get(_name);
                if (vi != uint_64_npos && vi >= cf.base)
                    throw script_exception{error_type::NameError,
                                           std::string("Name conflict: '" + _name + "' already defined as variable")};
            } else {

                if (ent->m_store.contain(_name))
                    throw script_exception{error_type::NameError,
                                           std::string("Name conflict: '" + _name + "' already defined as variable")};
            }

            // Every name lives in m_map now, so the checks above are the whole conflict test
            if (ent->m_fly) {
            }
        }

        bool walker::is_simple_name(const std::string& _s) {
            if (_s.empty()) return false;
            if (!std::isalpha(static_cast<unsigned char>(_s[0])) && _s[0] != '_') return false;
            for (size_t i = 1; i < _s.size(); i++) {
                if (!std::isalnum(static_cast<unsigned char>(_s[i])) && _s[i] != '_') return false;
            }
            return true;
        }

        // Parsed without debug markers: they would push the expression down and callers take ast[1]
        varvec walker::iload_parse_expr(const std::string& _path, const size_t _max_nest) {
            std::string src = _path + ";";
            bytes b(src.c_str());
            signal<const compile_error&> cmpl;
            compile_error ce;
            bool got_err = false;
            cmpl.connect([&](const compile_error& _e) {
                if (got_err) return;
                ce = _e;
                got_err = true;
            });
            auto ast = parser::parse(bytes_view(b), "", {}, nullptr, nullptr, _max_nest, false,
                                     &cmpl);

            if (got_err) {
                std::string at = ce.loc.row ? (" at " + std::to_string(ce.loc.row) + ":" +
                                               std::to_string(ce.loc.col))
                                            : std::string();
                throw script_exception{error_type::ParseError,
                                       std::string("Cannot parse path") + at + ": " + ce.msg};
            }
            if (ast.size() < 2 || !ast[1].is_vec()) {

                std::string shown = _path.size() > 80 ? _path.substr(0, 80) + "..." : _path;
                throw script_exception{error_type::RuntimeError,
                                       std::string("Cannot parse path: " + shown)};
            }
            return ast[1].to<varvec>();
        }

        variant walker::walk_body(const varvec& _body, walker& _w) {
            variant result;
            // An O_BLOCK body is walked inline: the enclosing frame is already its scope
            if (!_body.empty() && _body[0].is<OPTYPE>() &&
                static_cast<op_enum>(_body[0].to<OPTYPE>()) == O_BLOCK) {
                for (size_t i = 1; i < _body.size(); i++) {
                    if (_body[i].is_vec()) {
                        result = _w.walk_tree(_body[i].to<varvec>());
                        if (_w.state.ret_flag || _w.state.break_flag || _w.state.cont_flag || _w.state.tail_flag)
                            break;
                    }
                }
            } else {
                result = _w.walk_tree(_body);
            }
            return result;
        }
    }
}
