/*****************************************************************/ /**
 * \file   ascript_modmng.cpp
 * \brief  Per-engine module instance manager implementation
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_modmng.h"
#include "ascript_resmng.h"
#include "ascript_utils.h"
#include "script/ascript_fwrap.h"
#include "script/ascript_walk.h"
#include <algorithm>
#include <memory>

namespace alx {
    namespace script {

        // one resource layer per process: the first mod_mng builds it, the last one left deletes it
        res_mng* mod_mng::s_res = nullptr;
        int mod_mng::s_users = 0;
        std::mutex mod_mng::s_res_mtx;

        mod_mng::mod_mng() {
            std::lock_guard<std::mutex> lk(s_res_mtx);
            if (!s_res) s_res = new res_mng();
            ++s_users;
            m_res = s_res;
        }

        mod_mng::~mod_mng() {
            // clear() first, so the layer the last destructor deletes still holds no resource
            clear();
            std::lock_guard<std::mutex> lk(s_res_mtx);
            if (--s_users == 0) {
                delete s_res;
                s_res = nullptr;
            }
        }

        // a cloned child still names the owner it was copied from: re-point it, and its own subtree, at the new one
        static void rebind_children(data_store& _store, impl_import* _owner) {
            for (const auto& p : _store.m_map) {
                if (p.second >= _store.m_data.size()) continue;
                auto& v = _store.m_data[p.second];
                if (!v.is<anyptr>()) continue;
                const anyptr& ap = v.to<anyptr>();
                if (auto* child = anyptr_ex<impl_import>::as(ap)) {
                    child->m_alias = p.first;
                    child->m_parent = _owner;
                    rebind_children(child->m_store, child);
                    continue;
                }
                if (auto* child_link = anyptr_ex<impl_link>::as(ap)) {
                    child_link->m_alias = p.first;
                    child_link->m_parent = _owner;
                }
            }
        }

        // an anyptr copy clones the impl, so every clone takes its own fly ref for its destructor
        // a fresh copy has no home: its m_parent is stamped by the binding site it lands on
        impl_import::impl_import(const impl_import& _o)
            : m_fly(_o.m_fly), m_mng(_o.m_mng), m_store(_o.m_store),
              m_env_paths(_o.m_env_paths), m_parent(nullptr),
              m_alias(_o.m_alias) {
            // re-bind first, ref last: a throw in the re-bind must not leave a reference no destructor returns
            rebind_children(m_store, this);
            if (m_mng && m_fly) m_fly->m_ref.ref();
        }

        impl_import& impl_import::operator=(const impl_import& _o) {
            if (this == &_o) return *this;
            // ref the incoming fly before releasing the old one: they may be the same fly, and every throw
            // below must leave this object holding a reference its destructor will return
            if (_o.m_mng && _o.m_fly) _o.m_fly->m_ref.ref();
            if (m_mng && m_fly) m_mng->release_fly(m_fly);
            m_fly = _o.m_fly;
            m_mng = _o.m_mng;
            m_store = _o.m_store;
            m_env_paths = _o.m_env_paths;
            m_parent = nullptr;
            m_alias = _o.m_alias;
            rebind_children(m_store, this);
            return *this;
        }

        impl_link::impl_link(const impl_link& _o)
            : m_fly(_o.m_fly), m_mng(_o.m_mng), m_store(_o.m_store),
              m_parent(nullptr), m_alias(_o.m_alias) {
            if (m_mng && m_fly) m_fly->m_ref.ref();
        }

        impl_link& impl_link::operator=(const impl_link& _o) {
            if (this == &_o) return *this;
            if (m_mng && m_fly) m_mng->release_fly(m_fly);
            m_fly = _o.m_fly;
            m_mng = _o.m_mng;
            m_store = _o.m_store;
            m_parent = nullptr;
            m_alias = _o.m_alias;
            if (m_mng && m_fly) m_fly->m_ref.ref();
            return *this;
        }

        impl_import::~impl_import() {
            m_store.m_data.clear();
            if (m_mng) m_mng->release_fly(m_fly);
        }

        impl_link::~impl_link() {
            // per instance: the LINK_RELEASE hook runs with the instance store still populated
            if (m_fly && m_fly->m_link && m_fly->m_link->m_release_fn) {
                std::vector<variant*> empty_args;
                fwrap_impl fw(std::move(empty_args), nullptr, &m_store, nullptr);
                m_fly->m_link->m_release_fn(fw);
            }
            m_store.m_data.clear();
            if (m_mng) m_mng->release_fly(m_fly);
        }

        impl_import* mod_mng::make_import(const std::string& _path, walker* _w,
                                          script_exception& _err) {
            fly_import* fly = ref_fly_import(_path, _w, _err);
            if (!fly) return nullptr;

            impl_import* inst = init_fly_import(fly, _w, _err) ? impl_fly_import(fly, _w, _err) : nullptr;

            // no instance took the ref over on this path, so it goes back to the pool here
            if (!inst) {
                release_fly(fly);
                return nullptr;
            }
            return inst;
        }

        impl_link* mod_mng::make_link(const std::string& _path, walker* _w,
                                      script_exception& _err) {
            fly_link* fly = ref_fly_link(_path, _w, _err);
            if (!fly) return nullptr;
            if (!init_fly_link(fly, _w, _err)) {
                release_fly(fly);
                return nullptr;
            }
            impl_link* inst = impl_fly_link(fly, _w, _err);
            if (!inst) {
                release_fly(fly);
                return nullptr;
            }
            return inst;
        }

        fly_import* mod_mng::ref_fly_import(const std::string& _path, walker* _w,
                                            script_exception& _err) {
            std::string resolved = res_mng::resolve_runtime_path(_path, _w->state.current,
                                                                 *_w->state.m_search_paths,
                                                                 _w->state.compiled_modules);
            if (resolved.empty())
                resolved = res_mng::resolve_runtime_path_auto_import(_path, _w->state.current,
                                                                     *_w->state.m_search_paths,
                                                                     _w->state.compiled_modules);
            if (resolved.empty()) {
                _err = {error_type::ImportError,
                        std::string("Cannot import: ") + _path};
                return nullptr;
            }

            if (!walker::call_hook(*_w, hook_event::import, variant(resolved))) {
                _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                return nullptr;
            }

            uint_64 key = res_make_key(resolved);

            auto it = m_imports.find(key);
            if (it != m_imports.end()) {
                it->second->m_ref.ref();
                return it->second;
            }

            // resource before fly: a load failure leaves the pool untouched, so the next import retries
            ast_resource* ast = m_res->ref_ast(resolved, _w, _err);
            if (!ast) return nullptr;

            std::unique_ptr<fly_import> fly(new fly_import());
            fly->m_id = key;
            fly->m_path = resolved;
            fly->m_ast = ast;
            // the template root's identity: pos_file and env() read m_fly, and m_mng stays null
            // so the fly's own root never holds a reference to the fly
            fly->m_root.m_fly = fly.get();
            // the module's own directory leads its env search, so nested imports resolve next to it
            fly->m_root.m_env_paths.push_back(dirname_of(resolved));
            // unsharable until here, where every deref() reads as the last one: this is the one owner
            fly->m_ref.init_owned();
            fly_import* ptr = fly.release();
            m_imports[key] = ptr;
            return ptr;
        }

        fly_link* mod_mng::ref_fly_link(const std::string& _path, walker* _w,
                                        script_exception& _err) {
            std::string resolved = res_mng::resolve_runtime_path(_path, _w->state.current,
                                                                 *_w->state.m_search_paths,
                                                                 _w->state.compiled_modules);
            if (resolved.empty())
                resolved = res_mng::resolve_runtime_path_auto_link(_path, _w->state.current,
                                                                   *_w->state.m_search_paths,
                                                                   _w->state.compiled_modules);
            if (resolved.empty()) {
                _err = {error_type::ImportError,
                        std::string("Cannot link: ") + _path};
                return nullptr;
            }

            if (!walker::call_hook(*_w, hook_event::link, variant(resolved))) {
                _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                return nullptr;
            }

            uint_64 key = res_make_key(resolved);

            auto it = m_links.find(key);
            if (it != m_links.end()) {
                it->second->m_ref.ref();
                return it->second;
            }

            link_resource* lr = m_res->ref_link(resolved, _w, _err);
            if (!lr) return nullptr;

            std::unique_ptr<fly_link> fly(new fly_link());
            fly->m_id = key;
            fly->m_path = resolved;
            fly->m_link = lr;
            fly->m_ref.init_owned();
            fly_link* ptr = fly.release();
            m_links[key] = ptr;
            return ptr;
        }

        template <typename FlyT, typename DoInit>
        bool init_fly_gate(FlyT* fly, mod_mng* mng, walker* _w,
                           script_exception& _err, const char* _self_msg,
                           const char* _circ_msg, DoInit&& _do_init) {
            if (fly->m_init_done) return true;
            // a failure is latched with its text: while the fly stays pooled, later imports replay it
            if (fly->m_bad) {
                _err = {fly->m_err_type, fly->m_err_value};
                return false;
            }

            auto& st = mng->m_loading_stack;
            auto it = std::find(st.begin(), st.end(), fly->m_path);
            if (it != st.end()) {
                // the newest stack entry is a self-import; a deeper hit is a cycle
                bool self = (it + 1 == st.end());
                _err = {error_type::ImportError,
                        std::string(self ? _self_msg : _circ_msg) + fly->m_path};
                return false;
            }

            st.push_back(fly->m_path);
            bool ok = _do_init(fly, _w, _err);
            st.pop_back();

            if (ok) {
                fly->m_init_done = true;
            } else {
                fly->m_bad = true;
                fly->m_err_type = _err.type;
                fly->m_err_value = _err.info;
            }
            return ok;
        }

        bool mod_mng::init_fly_import(fly_import* fly, walker* _w,
                                      script_exception& _err) {
            return init_fly_gate(fly, this, _w, _err, "Self-import: ", "Circular import: ",
                                 [this](fly_import* f, walker* w, script_exception& e) {
                                     return run_init_walk(f, w, e);
                                 });
        }

        bool mod_mng::init_fly_link(fly_link* fly, walker* _w,
                                    script_exception& _err) {
            return init_fly_gate(fly, this, _w, _err, "Self-link: ", "Circular link: ",
                                 [](fly_link* f, walker*, script_exception& e) {
                                     bool ok = true;
                                     // once per fly: the LINK_LOAD hook builds the template instances copy
                                     if (f->m_link && f->m_link->m_load_fn) {

                                         std::vector<variant*> empty_args;
                                         fwrap_impl fw(std::move(empty_args), nullptr,
                                                       &f->m_store, nullptr);
                                         try {
                                             f->m_link->m_load_fn(fw);
                                         } catch (const script_exception& _e) {
                                             e = _e;
                                             ok = false;
                                         } catch (const std::exception& _e) {
                                             e = {error_type::NativeError,
                                                  std::string("NativeError: ") + _e.what()};
                                             ok = false;
                                         } catch (...) {
                                             e = {error_type::NativeError,
                                                  std::string("unknown native error")};
                                             ok = false;
                                         }
                                     }
                                     return ok;
                                 });
        }

        static std::string init_err(fly_import* _fly, const std::string& _what, walker& _sub) {
            std::string msg = "in module " + _fly->m_path + ": " + _what;
            try {
                std::string t = walker::walk_state_trace(_sub);
                if (!t.empty()) msg += "\n" + t;
            } catch (...) {
            }
            return msg;
        }

        bool mod_mng::run_init_walk(fly_import* fly, walker* _w,
                                    script_exception& _err) {
            walker sub(_w->m_cfg);
            sub.mgr = this;
            // the walk runs on the fly's own root: the store needs no move at the end, and a nested
            // import/link binding its parent to it names the fly, not this stack frame
            sub.state.root = &fly->m_root;
            sub.state.current = &fly->m_root;
            sub.state.root_entity = &fly->m_root;

            sub.state.m_engine = _w->state.m_engine;
            sub.state.m_search_paths = _w->state.m_search_paths;
            sub.state.m_cfg = _w->state.m_cfg;
            sub.state.on_csys = _w->state.on_csys;
            sub.state.on_cerr = _w->state.on_cerr;
            sub.m_hook = _w->m_hook.load();
            sub.m_hook_ud = _w->m_hook_ud.load();
            sub.m_hook_interval = _w->m_hook_interval.load();

            // a packed module carries its own inner @id table, which wins over the engine's
            sub.state.compiled_modules =
                fly->m_ast->m_inner_modules.empty() ? _w->state.compiled_modules : &fly->m_ast->m_inner_modules;

            try {
                sub.walk_forest(fly->m_ast->m_ast);
                // an interrupt outranks the walk's own outcome, and its reason travels up with it
                if (sub.m_interrupted || _w->m_interrupted) {
                    _w->m_interrupted = true;
                    if (_w->m_interrupt_desc.empty())
                        _w->m_interrupt_desc = sub.m_interrupt_desc;
                    _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                    return false;
                }
                // the module body's variables and added env paths are already on the fly's root
                return true;
            } catch (const script_exception& _e) {
                if (sub.m_interrupted || _w->m_interrupted) {
                    _w->m_interrupted = true;
                    if (_w->m_interrupt_desc.empty())
                        _w->m_interrupt_desc = sub.m_interrupt_desc;
                    _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                    return false;
                }
                // a nested ImportError names its own module and carries the trace already: keep it
                if (_e.type == error_type::ImportError)
                    _err = _e;
                else
                    _err = {error_type::ImportError,
                            init_err(fly, std::string(error_type_name(_e.type)) + ": " + _e.info,
                                     sub)};
            } catch (const std::exception& _e) {
                if (sub.m_interrupted || _w->m_interrupted) {
                    _w->m_interrupted = true;
                    if (_w->m_interrupt_desc.empty())
                        _w->m_interrupt_desc = sub.m_interrupt_desc;
                    _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                    return false;
                }
                _err = {error_type::ImportError,
                        init_err(fly, std::string("NativeError: ") + _e.what(), sub)};
            } catch (...) {
                if (sub.m_interrupted || _w->m_interrupted) {
                    _w->m_interrupted = true;
                    if (_w->m_interrupt_desc.empty())
                        _w->m_interrupt_desc = sub.m_interrupt_desc;
                    _err = {error_type::InterruptedError, _w->m_interrupt_desc};
                    return false;
                }
                _err = {error_type::ImportError,
                        init_err(fly, "unknown native error", sub)};
            }
            return false;
        }

        impl_import* mod_mng::impl_fly_import(fly_import* fly, walker* _w,
                                              script_exception& _err) {
            std::unique_ptr<impl_import> inst(new impl_import());
            // the caller's ref is inherited, not counted again: this instance owes one release
            inst->m_fly = fly;
            inst->m_env_paths = fly->m_root.m_env_paths;

            try {
                // a deep copy of the prefab, children included: every clone is re-bound to this copy
                inst->m_store = fly->m_root.m_store;
                rebind_children(inst->m_store, inst.get());
            } catch (const std::bad_alloc&) {
                _err = {error_type::MemoryError, std::string("out of memory")};
                return nullptr;
            } catch (const std::length_error& _e) {
                _err = {error_type::MemoryError, std::string(_e.what())};
                return nullptr;
            } catch (const std::exception& _e) {
                _err = {error_type::NativeError,
                        std::string("NativeError: ") + _e.what()};
                return nullptr;
            } catch (...) {
                _err = {error_type::NativeError,
                        std::string("unknown native error")};
                return nullptr;
            }
            // set last: a throw above unwinds this instance without releasing the ref make_import owns
            inst->m_mng = this;
            return inst.release();
        }

        impl_link* mod_mng::impl_fly_link(fly_link* fly, walker* _w,
                                          script_exception& _err) {
            (void) _w;
            std::unique_ptr<impl_link> inst(new impl_link());
            inst->m_fly = fly;

            try {
                // per instance: LINK_CREATE builds a fresh area, else the template store is copied
                if (fly->m_link && fly->m_link->m_create_fn) {

                    std::vector<variant*> empty_args;
                    fwrap_impl fw(std::move(empty_args), nullptr,
                                  &inst->m_store, nullptr);
                    fly->m_link->m_create_fn(fw);
                } else {
                    const auto& src_data = fly->m_store.m_data;
                    inst->m_store.m_data.resize(src_data.size());
                    for (size_t i = 0; i < src_data.size(); ++i)
                        inst->m_store.m_data[i] = src_data[i];
                    inst->m_store.m_map = fly->m_store.m_map;
                    inst->m_store.m_free = fly->m_store.m_free;
                }
            } catch (const script_exception& _e) {
                _err = _e;
                return nullptr;
            } catch (const std::bad_alloc&) {
                _err = {error_type::MemoryError, std::string("out of memory")};
                return nullptr;
            } catch (const std::length_error& _e) {
                _err = {error_type::MemoryError, std::string(_e.what())};
                return nullptr;
            } catch (const std::exception& _e) {
                _err = {error_type::NativeError,
                        std::string("NativeError: ") + _e.what()};
                return nullptr;
            } catch (...) {
                _err = {error_type::NativeError,
                        std::string("unknown native error")};
                return nullptr;
            }
            // set last for the same reason: make_link owns the ref until this returns
            inst->m_mng = this;
            return inst.release();
        }

        void mod_mng::release_fly(fly_import* _fw) {
            if (!_fw) return;
            // a true deref() means a holder is left; the last release evicts the entry, so the next import re-parses
            if (_fw->m_ref.deref()) return;
            m_imports.erase(_fw->m_id);
            ast_resource* ast = _fw->m_ast;
            delete _fw;
            m_res->release_ast(ast);
        }
        void mod_mng::release_fly(fly_link* _fw) {
            if (!_fw) return;
            if (_fw->m_ref.deref()) return;
            m_links.erase(_fw->m_id);
            link_resource* lr = _fw->m_link;
            // per fly, on the template store: the LINK_UNLOAD counterpart of the per-instance release
            if (lr && lr->m_unload_fn) {
                std::vector<variant*> empty_args;
                fwrap_impl fw(std::move(empty_args), nullptr,
                              &_fw->m_store, nullptr);
                lr->m_unload_fn(fw);
            }
            delete _fw;
            m_res->release_link(lr);
        }

        // teardown: every pooled fly goes regardless of its refcount, links running their unload hook
        void mod_mng::clear() {
            while (!m_imports.empty()) {
                auto it = m_imports.begin();
                fly_import* fly = it->second;
                m_imports.erase(it);
                ast_resource* ast = fly->m_ast;
                delete fly;
                m_res->release_ast(ast);
            }
            while (!m_links.empty()) {
                auto it = m_links.begin();
                fly_link* fly = it->second;
                m_links.erase(it);
                link_resource* lr = fly->m_link;
                if (lr && lr->m_unload_fn) {
                    std::vector<variant*> empty_args;
                    fwrap_impl fw(std::move(empty_args), nullptr,
                                  &fly->m_store, nullptr);
                    lr->m_unload_fn(fw);
                }
                delete fly;
                m_res->release_link(lr);
            }
        }

    }
}
