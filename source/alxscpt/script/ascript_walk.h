/*****************************************************************/ /**
 * \file   ascript_walk.h
 * \brief  Script walker — interpreter: owns state, op dispatch, all builtin ops
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_WALK_H_
#define _ALEXIS_SCRIPT_WALK_H_

#include "ascript_dot.h"
#include "ascript_resmng.h"
#include <atomic>

namespace alx {
    namespace script {

        using op_func = variant (*)(const varvec&, walker&);
        using op_table = op_func[O_ENUMSIZE];

        class walker {
        public:
            // must precede `state`: ~walk_state runs first, and its frame dtors truncate m_root's store
            impl_import m_root;
            // synthetic fly owned by this walker alone; m_root carries no mod_mng, so it is never refcounted
            fly_import m_root_fly;
            std::list<varvec> m_root_asts;
            // borrows the engine's config, which outlives this walker
            const engine_config& m_cfg;

            walk_state state;
            // non-owning back pointer to the module layer; import/link silently no-op while it is null
            mod_mng* mgr = nullptr;

            // host-side writers (set_hook/set_interrupt) against the exec thread: each field is atomic on its own
            std::atomic<hook_fn> m_hook{nullptr};
            std::atomic<void*> m_hook_ud{nullptr};
            std::atomic<uint_64> m_hook_interval{0};
            // m_insn counts ops since the last exec event; m_insn_total counts the events (hook_info::info)
            uint_64 m_insn = 0;
            uint_64 m_insn_total = 0;
            std::atomic<bool> m_interrupted{false};
            // hook_info::desc points here: the hook writes the reject reason, exec reads it after the walk
            std::string m_interrupt_desc;

            walker(const engine_config& _cfg);
            ~walker();

            void init();
            /// Fill the process-wide dispatch table; init() runs it exactly once
            static void fill_ops();
            void reset();
            variant walk_tree(const varvec& _tree);
            // stops at the first statement that sets a control-flow flag (return/break/continue/tail)
            variant walk_forest(const varvec& _forest);
            // false = interrupted; the due hook_event::exec fires from inside here
            bool checkpoint();

        public:
            static variant op_nop(const varvec&, walker&);
            static variant op_here(const varvec&, walker&);
            static variant op_trap(const varvec&, walker&);
            static variant op_debug(const varvec&, walker&);
            static variant op_eval(const varvec&, walker&);
            static variant op_program(const varvec&, walker&);
            static variant op_block(const varvec&, walker&);
            static variant op_load(const varvec&, walker&);
            static variant op_iload(const varvec&, walker&);
            static variant op_var(const varvec&, walker&);
            static variant op_store(const varvec&, walker&);
            static variant op_return(const varvec&, walker&);
            static variant op_call(const varvec&, walker&);
            static variant op_ncall(const varvec&, walker&);
            static variant op_icall(const varvec&, walker&);
            static variant op_tcall(const varvec&, walker&);
            static variant op_def(const varvec&, walker&);
            static variant op_if(const varvec&, walker&);
            static variant op_while(const varvec&, walker&);
            static variant op_for(const varvec&, walker&);
            static variant op_foreach(const varvec&, walker&);
            static variant op_break(const varvec&, walker&);
            static variant op_continue(const varvec&, walker&);
            static variant op_switch(const varvec&, walker&);
            static variant op_try(const varvec&, walker&);
            static variant op_throw(const varvec&, walker&);
            static variant op_int(const varvec&, walker&);
            static variant op_float(const varvec&, walker&);
            static variant op_string(const varvec&, walker&);
            static variant op_bool(const varvec&, walker&);
            static variant op_bytes(const varvec&, walker&);
            static variant op_vec(const varvec&, walker&);
            static variant op_map(const varvec&, walker&);
            static variant op_lst(const varvec&, walker&);
            static variant op_type(const varvec&, walker&);
            static variant op_env(const varvec&, walker&);
            static variant op_import(const varvec&, walker&);
            static variant op_link(const varvec&, walker&);
            static variant op_uplus(const varvec&, walker&);
            static variant op_uminus(const varvec&, walker&);
            static variant op_add(const varvec&, walker&);
            static variant op_sub(const varvec&, walker&);
            static variant op_mul(const varvec&, walker&);
            static variant op_div(const varvec&, walker&);
            static variant op_mod(const varvec&, walker&);
            static variant op_pre_inc(const varvec&, walker&);
            static variant op_pre_dec(const varvec&, walker&);
            static variant op_post_inc(const varvec&, walker&);
            static variant op_post_dec(const varvec&, walker&);
            static variant op_lshift(const varvec&, walker&);
            static variant op_rshift(const varvec&, walker&);
            static variant op_eq(const varvec&, walker&);
            static variant op_ne(const varvec&, walker&);
            static variant op_lt(const varvec&, walker&);
            static variant op_gt(const varvec&, walker&);
            static variant op_le(const varvec&, walker&);
            static variant op_ge(const varvec&, walker&);
            static variant op_bit_and(const varvec&, walker&);
            static variant op_bit_or(const varvec&, walker&);
            static variant op_bit_xor(const varvec&, walker&);
            static variant op_bit_neg(const varvec&, walker&);
            static variant op_and(const varvec&, walker&);
            static variant op_or(const varvec&, walker&);
            static variant op_not(const varvec&, walker&);
            static variant op_ternary(const varvec&, walker&);
            static variant op_comma(const varvec&, walker&);
            static variant op_dot(const varvec&, walker&);
            static variant op_index(const varvec&, walker&);
            static variant op_slice(const varvec&, walker&);
            static variant op_del(const varvec&, walker&);

            static variant op_ass_add(const varvec&, walker&);
            static variant op_ass_sub(const varvec&, walker&);
            static variant op_ass_mul(const varvec&, walker&);
            static variant op_ass_div(const varvec&, walker&);
            static variant op_ass_mod(const varvec&, walker&);
            static variant op_ass_lshift(const varvec&, walker&);
            static variant op_ass_rshift(const varvec&, walker&);
            static variant op_ass_bit_and(const varvec&, walker&);
            static variant op_ass_bit_or(const varvec&, walker&);
            static variant op_ass_bit_xor(const varvec&, walker&);

            static variant op_excall(const varvec&, walker&);
            static variant op_econst(const varvec&, walker&);

        public:
            // What an assignment target resolved to: a variant slot, one byte of a string/bytes, or a
            // slice range. One pass decides all three, so the evaluation order -- index/bounds before
            // the container, nothing after the address is taken -- lives in a single place
            struct target_resolved {
                enum class kind : uint_8 { none,
                                           slot,
                                           byte,
                                           slice };

                kind type = kind::none;
                // slot: the writable variant; byte/slice: the container holding the positions
                variant* slot = nullptr;
                variant* parent = nullptr;
                // byte: the evaluated index (null = append); slice: the position sequence
                variant key;
                int_64 from = 0;
                int_64 to = 0;
                int_64 step = 1;
                // the entity the target lives in, for the binding sites to stamp on a handle value
                impl_import* owner = nullptr;
            };

            // write runs the write-side guards; nav is the host probe (guards off, an unresolved name or
            // path yields none) -- a script write must never resolve in nav mode
            enum class resolve_mode : uint_8 { write,
                                               nav };

            // _rmw: the caller reads and writes back the same slot, so an append index ([null]) is not a
            // usable target; _tail_create: the terminal map key or the append may be created -- false for
            // the base of an element or slice write, where a miss is an error and nothing may be left behind
            static target_resolved resolve_target(const varvec& _lhs, walker& _w, resolve_mode _mode,
                                                  bool _rmw, bool _tail_create = true);
            // Slot-only view: a byte element or a slice has no variant slot to hand back
            static variant* resolve_slot(const varvec& _lhs, walker& _w, bool _rmw);
            // Host view (fwrap), named after the op it runs: an indirect load by path. `call` is the
            // host's dispatcher and uses _tail_create = false -- locating a callee creates nothing;
            // `fwrap::iload` is the host's one slot handle and hands back a writable slot, so it keeps
            // the write posture (a missing terminal key is created)
            static variant* resolve_iload(const varvec& _lhs, walker& _w, bool _tail_create = true);
            // Slice assignment: the operand is the same container type, scattered in the read's own
            // position order, and its length must equal the position count
            static void store_slice(const target_resolved& _t, const variant& _val);
            static void store_raw(walker& _w, const variant& _name, variant&& _init_val);
            static void assign_raw(walker& _w, const variant& _name, const variant& _val);
            static void assign_raw(walker& _w, const variant& _name, variant&& _val);
            static const varvec* find_def(walker& _w, const std::string& _name);
            static variant eval_arg(const variant& _node, walker& _w);

            /// An argument scanned before any address is taken: a value, or a bare name deferred until
            /// the callee's path has run
            struct call_arg {
                enum kind_enum {
                    V_VALUE,
                    V_NAME,
                    V_UNPACK
                };
                kind_enum kind = V_VALUE;
                variant val;
                std::string name;
            };
            static std::vector<call_arg> scan_call_args(const varvec& _tree, size_t _arg_start, walker& _w);
            // deferred names resolve to their slots; the vector was reserved once, so its pointers stay valid
            static std::vector<variant*> native_arg_ptrs(std::vector<call_arg>& _args, walker& _w);

            static bool is_bare_ext_name(const std::string& _s);
            static variant invoke_extend(walker& _w, const std::string& _name,
                                         std::vector<call_arg>& _args);
            // _tco reuses _tco_frame (or the top frame) instead of pushing: params are written in place
            static void eval_and_bind_args(impl_import* _ent, const varvec& _def,
                                           const std::string& _func_name,
                                           const varvec& _tree, walker& _w, bool _tco,
                                           scope_frame* _tco_frame, std::vector<call_arg>& _args);
            static variant invoke_def(impl_import* _ent, const varvec& _def,
                                      const std::string& _func_name,
                                      const varvec& _tree, walker& _w,
                                      std::vector<call_arg>& _args);
            static variant invoke_dot(const dot_resolved& _r, const varvec& _tree, walker& _w,
                                      std::vector<call_arg>& _args);
            static bool del_name(const variant& _target, walker& _w);
            static bool del_dot(const varvec& _tree, walker& _w);
            static bool del_index(const varvec& _tree, walker& _w);
            static bool del_vec(variant& _container, const variant& _key);
            static bool del_lst(variant& _container, const variant& _key);
            static bool del_map(variant& _container, const variant& _key);
            // Bounds evaluate first (they can run script code), then fold against the container's size
            static void slice_eval_bounds(const varvec& _tree, walker& _w,
                                          variant& _fv, variant& _tv, int_64& _step);
            static void slice_apply_bounds(const variant& _obj, const variant& _fv, const variant& _tv,
                                           int_64& _step, int_64& _from, int_64& _to);
            static variant slice_vec(const varvec& _vec, int_64 _start, int_64 _end, int_64 _step);
            static variant slice_lst(const varlst& _lst, int_64 _start, int_64 _end, int_64 _step);
            static variant slice_str(const std::string& _str, int_64 _start, int_64 _end, int_64 _step);
            static variant slice_bytes(const bytes& _buf, int_64 _start, int_64 _end, int_64 _step);
            static std::string find_func_name(impl_import* _ent, const varvec* _def);

            // _f = nullptr = top level: the root script's file, or walk_state::top_pos in pos_map
            static std::string pos_file(const scope_frame* _f, impl_import* _root);
            static std::string pos_func(const scope_frame* _f);
            static varmap pos_map(const scope_frame* _f, const walk_state& _st, impl_import* _root);
            static std::string walk_state_trace(walker& _w);

            // _clear_insn = false (debug events) leaves the checkpoint counter alone, so the cadence holds
            static bool call_hook(walker& _w, hook_event _type, variant _info, bool _clear_insn = true);
            static bool check_dependencies(const bytes_view& _data, walker& _w,
                                           std::list<std::string>& _missing_imports,
                                           std::list<std::string>& _missing_links);

            static void check_name_conflict(walker& _w, const std::string& _name);
            static bool is_simple_name(const std::string& _s);
            static varvec iload_parse_expr(const std::string& _path, size_t _max_nest);

        private:
            static variant walk_body(const varvec& _body, walker& _w);

        private:
            // process-wide dispatch table filled once by init(); a null slot means the opcode has no handler
            static op_table s_ops;
        };

    }
}

#endif
