/*****************************************************************/ /**
 * \file   ascript.cpp
 * \brief  Script engine — thin wrapper over context + compile
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript.h"

#include <new>
#include <stdexcept>
#include "acompress.h"
#include "afile.h"
#include "astring.h"
#include "avarsolid.h"
#include "script/ascript_compile.h"
#include "script/ascript_lex.h"
#include "script/ascript_parse.h"
#include "script/ascript_modmng.h"
#include "script/ascript_walk.h"
#include <chrono>

namespace alx {
    namespace script {

        class engine_impl : public engine {
        public:
            engine_impl(const engine_config& _cfg) : m_cfg(_cfg), m_w(m_cfg) {
                reset();
            }

            ~engine_impl() override {

                // walker first: reset() drops the impls while m_mod and the shared layer are alive
                m_w.reset();
            }

            void reset() override {
                m_w.mgr = &m_mod;
                m_w.reset();
                m_w.state.m_engine = this;
                m_w.state.m_search_paths = &m_cfg.search_paths;
                m_w.state.on_cerr = &on_cerr;
                m_w.state.on_csys = &m_mod.m_res->on_csys;
            }

            alx::signal<uint_64, const std::string&>& get_csys() override { return m_mod.m_res->on_csys; }

            bool set_extend(const std::string& _name, native_func _handler) override {
                if (alx::script::is_reserved_name(_name)) return false;
                if (m_define.find(_name) != m_define.end()) return false;
                m_extend[_name] = _handler;
                return true;
            }
            void del_extend(const std::string& _name) override {
                m_extend.erase(_name);
            }
            native_func get_extend(const std::string& _name) const override {
                auto it = m_extend.find(_name);
                return it != m_extend.end() ? it->second : nullptr;
            }
            bool fid_extend(const std::string& _name) const override {
                return m_extend.find(_name) != m_extend.end();
            }

            bool set_define(const std::string& _name, const variant& _value) override {
                if (alx::script::is_reserved_name(_name)) return false;
                if (m_extend.find(_name) != m_extend.end()) return false;
                m_define[_name] = _value;
                return true;
            }
            void del_define(const std::string& _name) override {
                m_define.erase(_name);
            }
            variant get_define(const std::string& _name) const override {
                auto it = m_define.find(_name);
                return it != m_define.end() ? it->second : variant();
            }
            bool fid_define(const std::string& _name) const override {
                return m_define.find(_name) != m_define.end();
            }

            void set_etype(const std::string& _type) override { m_cfg.etype = _type; }
            void set_vtype(uint_64 _ver) override { m_cfg.vtype = _ver; }

            variant* load(const std::string& _name,
                          bool _auto_create = false) override {
                variant* p = m_w.m_root.m_store.find(_name);
                if (p) return p;
                if (!_auto_create) return nullptr;
                return m_w.m_root.m_store.store(_name, variant());
            }

            result call(const std::string& _name, const varvec& _args) override {
                variant* p = m_w.m_root.m_store.find(_name);
                if (!p || !p->is<anyptr>())
                    return {variant("function not found: " + _name), 0, error_type::NameError};

                call_able* ca = anyptr_ex<call_able>::as(p->to<anyptr>());
                if (!ca || !ca->is_def())
                    return {variant("not a def function: " + _name), 0, error_type::NameError};

                auto t0 = std::chrono::steady_clock::now();
                call_walk cw;
                cw.ca = ca;
                cw.name = _name;
                cw.tree.push_back(variant(OPTYPE(O_CALL)));
                cw.tree.push_back(variant(_name));
                for (const auto& a : _args) {
                    varvec leaf;
                    leaf.push_back(variant(a));
                    cw.tree.push_back(variant(std::move(leaf)));
                }
                if (!m_w.state.current) m_w.state.current = &m_w.m_root;
                return run_walk(t0, walk_entry_call, &cw);
            }

            result exec(const bytes_view& _data,
                        const std::string& _home_dir) override {
                auto t0 = std::chrono::steady_clock::now();
                result res;
                struct running_guard {
                    std::atomic<bool>& flag;
                    running_guard(std::atomic<bool>& f) : flag(f) { flag.store(true, std::memory_order_relaxed); }
                    ~running_guard() { flag.store(false, std::memory_order_relaxed); }
                } _rg(m_running);

                m_w.m_insn = 0;
                m_w.m_insn_total = 0;
                m_w.m_interrupted = false;
                m_w.m_interrupt_desc.clear();

                // the walk state survives a run, so a stale position must not stick to a marker-less AST
                m_w.state.clear_pos();

                m_w.state.ret_flag = false;
                m_w.state.break_flag = false;
                m_w.state.cont_flag = false;
                m_w.state.tail_flag = false;

                varvec ast;
                if (varsolid::is_valid(_data)) {
                    varmap vm;
                    varsolid::to_varmap(_data, vm);

                    std::string verr = check_version(vm);
                    if (!verr.empty()) {
                        res.error = error_type::VersionError;
                        res.value = variant(verr);
                        on_cerr(verr);
                        return res;
                    }
                    varmap inner_modules;
                    if (!decompress_inner(vm, ast, inner_modules)) {

                        const char* msg = "corrupt or tampered binary";
                        res.error = error_type::ImportError;
                        res.value = variant(msg);
                        on_cerr(msg);
                        return res;
                    }
                    if (!inner_modules.empty())
                        vm["modules"] = variant(inner_modules);
                    // a member on purpose: state.compiled_modules points into it for the whole run
                    m_compiled_vm = std::move(vm);
                    m_w.state.compiled_modules = m_compiled_vm.contain("modules")
                                                     ? &m_compiled_vm.value("modules").to<varmap>()
                                                     : nullptr;
                } else {
                    ast = do_parse(_data, _home_dir);
                    if (ast.empty()) {
                        res.error = error_type::ParseError;
                        return res;
                    }
                }

                m_w.m_root_fly.m_path = file_info(_home_dir).path();

                m_w.m_root.m_env_paths.clear();
                if (!m_w.m_root_fly.m_path.empty())
                    m_w.m_root.m_env_paths.push_back(m_w.m_root_fly.m_path);

                // a list, so nodes stay put: a registered def keeps a pointer into this AST
                m_w.m_root_asts.push_back(std::move(ast));
                m_w.state.root = &m_w.m_root;
                m_w.state.current = &m_w.m_root;
                return run_walk(t0, walk_entry_forest, nullptr);
            }

            bytes compile(const bytes_view& _data,
                          const std::string& _home_dir, bool _cmps, bool _embed,
                          const bytes_view& _hint) const override {
                compile_result result;
                auto ast = do_parse(_data, _home_dir, _embed ? &result : nullptr);
                if (ast.empty()) return bytes();
                // embed mode already filled result, and its AST holds module keys, not paths
                if (!_embed) collect_deps(ast, result);

                const std::string& main_file = m_src_file.empty() ? _home_dir : m_src_file;
                return make_compile_binary(main_file, ast, m_cfg.etype, m_cfg.vtype, result, _cmps,
                                           _hint);
            }

            const engine_config& config() const override { return m_cfg; }
            void set_max_stack(size_t _n) override { m_cfg.max_stack = _n; }
            void set_parse_depth(size_t _n) override { m_cfg.parse_depth = _n; }
            void set_overflow_check(bool _on) override { m_cfg.overflow_check = _on; }
            void set_debug_enable(bool _on) override { m_cfg.debug_enable = _on; }
            void set_max_vecfill(size_t _n) override { m_cfg.max_vecfill = _n; }
            void set_search_paths(const std::list<std::string>& _paths) override {
                m_cfg.search_paths = _paths;
            }

            void set_hook(hook_fn _fn, void* _ud, uint_64 _interval) override {
                m_cfg.hook_fn_ptr = _fn;
                m_cfg.hook_ud = _ud;
                m_cfg.hook_interval = _interval;
                // the walker calls its own atomic copies; the m_cfg fields are what config() reports
                m_w.m_hook.store(_fn);
                m_w.m_hook_ud.store(_ud);
                m_w.m_hook_interval.store(_interval);
            }

            void set_pipe(pipe_in _in, pipe_out _out, void* _in_ud, void* _out_ud) override {
                m_cfg.pipe_in_ptr = _in;
                m_cfg.pipe_out_ptr = _out;
                m_cfg.pipe_in_ud = _in_ud;
                m_cfg.pipe_out_ud = _out_ud;
            }

            void set_type_ex(type_ex _fn, void* _ud) override {
                m_cfg.type_ex_ptr = _fn;
                m_cfg.type_ex_ud = _ud;
            }

            bool running() const override { return m_running.load(std::memory_order_relaxed); }
            void set_interrupt() override { m_w.m_interrupted.store(true, std::memory_order_relaxed); }

            varvec do_parse(const bytes_view& _src, const std::string& _file_path,
                            compile_result* _embed_out = nullptr) const {

                const std::string& parse_path = m_src_file.empty() ? _file_path : m_src_file;
                token_list tl;
                tl.tokenize(_src, &on_cmpl, parse_path);
                parser p(tl, &on_cmpl, parse_path, m_cfg.search_paths, m_w.m_cfg.parse_depth, &m_extend, &m_define,
                         _embed_out, m_cfg.etype, m_cfg.vtype, m_cfg.debug_enable);
                varvec ast = p.parse();
                // an empty AST is the failure sentinel: the parser recovers and reports via on_cmpl
                if (p.has_error()) return {};
                return ast;
            }

        private:
            /// A walk body run_walk drives; _ud carries the entry's own context
            using walk_entry = variant (*)(engine_impl&, void*);

            /// call()'s context for its entry
            struct call_walk {
                call_able* ca = nullptr;
                std::string name;
                varvec tree;
            };

            /// The shared failure ladder of every engine entry: an interrupt outranks whatever the
            /// walk raised, and a walk stopped at a checkpoint (a normal return) reports through the
            /// same exit — so a run is interruptible wherever it was entered
            result run_walk(const std::chrono::steady_clock::time_point& _t0, walk_entry _walk, void* _ud) {
                result res;
                try {
                    res.value = _walk(*this, _ud);
                } catch (const script_exception& _e) {
                    if (m_w.m_interrupted) return interrupt_result(_t0);
                    std::string trace;
                    try {
                        trace = walker::walk_state_trace(m_w);
                    } catch (...) {
                    }
                    m_w.reset();
                    std::string msg = strutil::format("Uncaught: [%1] %2",
                                                      error_type_name(_e.type), _e.info);
                    if (!trace.empty()) msg += "\n" + trace;
                    on_cerr(msg);
                    res.value = _e.info;
                    res.error = _e.type;
                } catch (const std::bad_alloc&) {
                    if (m_w.m_interrupted) return interrupt_result(_t0);
                    m_w.reset();
                    on_cerr("MemoryError: out of memory");
                    res.value = variant(std::string("out of memory"));
                    res.error = error_type::MemoryError;
                } catch (const std::length_error& _e) {
                    if (m_w.m_interrupted) return interrupt_result(_t0);
                    m_w.reset();
                    on_cerr("MemoryError: " + std::string(_e.what()));
                    res.value = variant(std::string(_e.what()));
                    res.error = error_type::MemoryError;
                } catch (const std::exception& _e) {
                    if (m_w.m_interrupted) return interrupt_result(_t0);
                    m_w.reset();
                    std::string msg = "NativeError: " + std::string(_e.what());
                    on_cerr(msg);
                    res.value = variant(std::string(_e.what()));
                    res.error = error_type::NativeError;
                } catch (...) {
                    if (m_w.m_interrupted) return interrupt_result(_t0);
                    m_w.reset();
                    on_cerr("NativeError: unknown");
                    res.error = error_type::NativeError;
                }
                if (m_w.m_interrupted) return interrupt_result(_t0);
                res.elapsed_us = static_cast<int_64>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - _t0)
                        .count());
                return res;
            }

            static variant walk_entry_forest(engine_impl& _e, void*) {
                return _e.m_w.walk_forest(_e.m_w.m_root_asts.back());
            }

            static variant walk_entry_call(engine_impl& _e, void* _ud) {
                auto& c = *static_cast<call_walk*>(_ud);
                return walker::invoke_def(_e.m_w.state.current, *c.ca->m_def, c.name, c.tree, _e.m_w);
            }

            result interrupt_result(const std::chrono::steady_clock::time_point& _t0) {
                // the hook's reject reason, written through hook_info::desc; reset() clears it
                std::string desc = m_w.m_interrupt_desc;
                m_w.reset();
                std::string reason = "execution interrupted";
                if (!desc.empty()) reason += " — " + desc;
                on_cerr("Uncaught: [InterruptedError] " + reason);
                result r;
                r.value = variant(reason);
                r.error = error_type::InterruptedError;
                r.elapsed_us = static_cast<int_64>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - _t0)
                        .count());
                return r;
            }

            std::string check_version(const varmap& _vm) const {

                if (m_cfg.etype.empty() && m_cfg.vtype == 0) return {};

                std::string axp_type = _vm.value("etype").to<std::string>("");
                uint_64 axp_ver = _vm.value("vtype").to<uint_64>(0);

                if (axp_type != m_cfg.etype)
                    return "engine type mismatch: expected '" + m_cfg.etype + "', got '" + axp_type + "'";

                if (axp_ver > m_cfg.vtype)
                    return "version too new: axp v" + std::to_string(axp_ver) +
                           ", engine v" + std::to_string(m_cfg.vtype);

                return {};
            }

            engine_config m_cfg;
            mod_mng m_mod;
            walker m_w;
            varmap m_compiled_vm;

            std::unordered_map<std::string, native_func> m_extend;
            std::unordered_map<std::string, variant> m_define;

            std::atomic<bool> m_running{false};

        public:
            void set_src_file(const std::string& _f) const { m_src_file = _f; }
            void clr_src_file() const { m_src_file.clear(); }
            // bound by the file wrappers for their scope: diagnostics, here() and the product's file
            mutable std::string m_src_file;

            struct src_file_guard {
                const engine_impl* impl;
                src_file_guard(const engine_impl* _i, const std::string& _f)
                    : impl(_i) {
                    impl->set_src_file(_f);
                }
                ~src_file_guard() { impl->clr_src_file(); }
            };
        };

        engine* engine::create(const engine_config& _cfg) { return new engine_impl(_cfg); }

        bool engine::unpack(const bytes_view& _data, varmap& _out) {
            return ::alx::script::unpack(_data, _out);
        }

        std::string engine::prtast(const bytes_view& _data) {

            return ::alx::script::prtast(_data, nullptr);
        }

        std::string engine::prtinf(const bytes_view& _data) {

            return ::alx::script::prtinf(_data);
        }

        bytes engine::prtfmt(const bytes_view& _data) {

            return ::alx::script::prtfmt(_data);
        }

        engine::result engine::exec(const std::string& _file_path) {
            bytes content = file::read_all(_file_path);
            // read_all cannot tell them apart: an empty file reports the same as an unreadable one
            if (content.empty())
                return {variant("cannot read file: " + _file_path), 0,
                        error_type::RuntimeError};
            file_info fi(_file_path);
            std::string home_dir = fi.is_dir() ? fi.path() : fi.get_parent().path();
            auto* impl = static_cast<engine_impl*>(this);
            engine_impl::src_file_guard g(impl, fi.is_dir() ? std::string() : fi.path());
            return exec(bytes_view(content), home_dir);
        }

        bytes engine::compile(const std::string& _path, bool _cmps, bool _embed,
                              const std::string& _hint) const {
            bytes content = file::read_all(_path);
            if (content.empty()) return bytes();
            bytes hint;
            if (!_hint.empty()) hint = file::read_all(_hint);
            file_info fi(_path);
            std::string home_dir = fi.is_dir() ? fi.path() : fi.get_parent().path();
            auto* impl = const_cast<engine_impl*>(static_cast<const engine_impl*>(this));
            engine_impl::src_file_guard g(impl, fi.is_dir() ? std::string() : fi.path());
            return compile(bytes_view(content), home_dir, _cmps, _embed, bytes_view(hint));
        }

        const engine_config& hook_info::wkconfig() const {
            return static_cast<const walker*>(wkdt)->m_cfg;
        }
        uint_64 hook_info::wkfm_size() const {
            return static_cast<const walker*>(wkdt)->state.frames.size();
        }

        std::string hook_info::wkfm_func(uint_64 _i) const {
            auto& f = static_cast<const walker*>(wkdt)->state.frames[_i];
            if (!f.def) return std::string(f.flags & FF_RET ? "eval" : "?");
            return walker::find_func_name(f.ent, f.def);
        }

        const void* hook_info::wkfm_eptr(uint_64 _i) const {
            return static_cast<const walker*>(wkdt)->state.frames[_i].ent;
        }

        std::vector<std::string> hook_info::wkfm_data_keys(uint_64 _i) {
            auto& f = static_cast<const walker*>(wkdt)->state.frames[_i];
            std::vector<std::string> keys;
            // promoted: the slot array is frozen and the hash holds the live names
            if (f.var_map.is_promoted()) {

                const auto& hm = f.var_map.get_hash();
                keys.reserve(hm.size());
                for (auto& p : hm) keys.push_back(p.first);
            } else {
                const auto& sa = f.var_map.get_slot();
                for (uint_64 i = 0; i < sa.size; i++)
                    if (sa.val[i] != uint_64_npos) keys.push_back(sa.key[i]);
            }
            return keys;
        }

        const variant* hook_info::wkfm_data_cptr(uint_64 _i, const char* _key) const {
            auto& f = static_cast<const walker*>(wkdt)->state.frames[_i];
            uint_64 idx = f.var_map.get(_key);
            if (idx == uint_64_npos) return nullptr;
            return &f.ent->m_store.m_data[idx];
        }

        const void* hook_info::wken_of(const variant* _v) const {
            if (!_v || !_v->is<anyptr>()) return nullptr;
            return anyptr_ex<impl_import>::as(_v->to<anyptr>());
        }

        const void* hook_info::wken_root() const {
            return static_cast<const walker*>(wkdt)->state.root;
        }

        std::vector<std::string> hook_info::wken_data_keys(const void* _e) {
            auto& w = *static_cast<const walker*>(wkdt);
            auto* ent = static_cast<const impl_import*>(_e);
            // a slot at or past a frame's base is that frame's local, so entity level ends below it
            size_t base = ent->m_store.m_data.size();
            for (auto& f : w.state.frames)
                if (f.ent == ent && f.base < base) base = f.base;
            std::vector<std::string> keys;
            for (auto& p : ent->m_store.m_map)
                if (p.second < base) keys.push_back(p.first);
            return keys;
        }

        const variant* hook_info::wken_data_cptr(const void* _e, const char* _key) const {
            auto& w = *static_cast<const walker*>(wkdt);
            auto* ent = static_cast<const impl_import*>(_e);
            size_t base = ent->m_store.m_data.size();
            for (auto& f : w.state.frames)
                if (f.ent == ent && f.base < base) base = f.base;
            auto it = ent->m_store.m_map.find(_key);
            if (it == ent->m_store.m_map.end() || it->second >= base) return nullptr;
            return &ent->m_store.m_data[it->second];
        }

        std::string hook_info::wken_file(const void* _e) const {
            auto* ent = static_cast<const impl_import*>(_e);
            if (!ent->m_fly) return "::";
            return ent->m_fly->m_path.empty() ? "::" : ent->m_fly->m_path;
        }

        std::string hook_info::wken_name(const void* _e) const {
            return static_cast<const impl_import*>(_e)->m_alias;
        }

        const void* hook_info::wken_pptr(const void* _e) const {
            return static_cast<const impl_import*>(_e)->m_parent;
        }

        const void* hook_info::wklk_of(const variant* _v) const {
            if (!_v || !_v->is<anyptr>()) return nullptr;
            return anyptr_ex<impl_link>::as(_v->to<anyptr>());
        }

        std::vector<std::string> hook_info::wklk_data_keys(const void* _l) {
            auto* lk = static_cast<const impl_link*>(_l);
            std::vector<std::string> keys;
            keys.reserve(lk->m_store.m_map.size());
            for (auto& p : lk->m_store.m_map) keys.push_back(p.first);
            return keys;
        }

        const variant* hook_info::wklk_data_cptr(const void* _l, const char* _key) const {
            auto* lk = static_cast<const impl_link*>(_l);
            auto it = lk->m_store.m_map.find(_key);
            if (it == lk->m_store.m_map.end()) return nullptr;
            return &lk->m_store.m_data[it->second];
        }

        std::vector<std::string> hook_info::wklk_area_funs(const void* _l, const char* _area) {
            auto* lk = static_cast<const impl_link*>(_l);
            std::vector<std::string> names;
            auto it = lk->m_store.m_map.find(_area);
            if (it == lk->m_store.m_map.end() ||
                it->second >= lk->m_store.m_data.size())
                return names;
            auto& dv = lk->m_store.m_data[it->second];
            if (!dv.is<anyptr>()) return names;
            auto* area = anyptr_ex<link_area>::as(dv.to<anyptr>());
            if (!area) return names;
            names.reserve(area->m_natives.size());
            for (auto& p : area->m_natives) names.push_back(p.first);
            return names;
        }

        const void* hook_info::wklk_pptr(const void* _l) const {
            return static_cast<const impl_link*>(_l)->m_parent;
        }

        bool hook_info::wkis_ent(const variant* _v) const {
            return _v && _v->is<anyptr>() &&
                   anyptr_ex<impl_import>::as(_v->to<anyptr>()) != nullptr;
        }

        bool hook_info::wkis_link(const variant* _v) const {
            return _v && _v->is<anyptr>() &&
                   anyptr_ex<impl_link>::as(_v->to<anyptr>()) != nullptr;
        }

        bool hook_info::wkis_func(const variant* _v) const {
            return _v && _v->is<anyptr>() &&
                   anyptr_ex<call_able>::as(_v->to<anyptr>()) != nullptr;
        }

        bool hook_info::wkis_area(const variant* _v) const {
            return _v && _v->is<anyptr>() &&
                   anyptr_ex<link_area>::as(_v->to<anyptr>()) != nullptr;
        }

        varmap hook_info::wkdt_pos() const {
            auto& w = *static_cast<const walker*>(wkdt);
            const scope_frame* f = w.state.frames.empty() ? nullptr : &w.state.frames.back();
            return walker::pos_map(f, w.state, w.state.root_entity);
        }

        std::list<varmap> hook_info::wkdt_fpos() const {
            auto& w = *static_cast<const walker*>(wkdt);
            std::list<varmap> out;

            for (auto it = w.state.frames.rbegin(); it != w.state.frames.rend(); ++it) {
                // a position-less frame is dropped, an eval frame kept: the error trace's own rule
                if (!(it->flags & FF_RET) && !it->pos.row) continue;
                varmap m = walker::pos_map(&*it, w.state, w.state.root_entity);
                m["func"] = variant(walker::pos_func(&*it));
                out.push_back(std::move(m));
            }
            varmap top = walker::pos_map(nullptr, w.state, w.state.root_entity);
            top["func"] = variant(std::string());
            out.push_back(std::move(top));
            return out;
        }

    }
}
