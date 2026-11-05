/*****************************************************************/ /**
 * \file   ascript_fwrap.cpp
 * \brief  fwrap implementations — one class per call context
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_fwrap.h"
#include "ascript_modmng.h"

namespace alx {
    namespace script {

        namespace {

            // the handle fwrap::currying mints: a name and the engine, nothing resolved
            class currying_handle : public fwrap::callback {
            public:
                currying_handle(engine* _e, std::string _name)
                    : m_engine(_e), m_name(std::move(_name)) {}

                std::future<result> call(const varvec& _args, uint_32 _priority,
                                         uint_32 _expiry_ms, uint_32 _delay_ms) override {
                    return m_engine->yield(m_name, _args, clamp(_priority), _expiry_ms, _delay_ms);
                }
                std::future<result> call(varvec&& _args, uint_32 _priority,
                                         uint_32 _expiry_ms, uint_32 _delay_ms) override {
                    return m_engine->yield(m_name, std::move(_args), clamp(_priority), _expiry_ms,
                                           _delay_ms);
                }
                uint_64 drop(uint_32 _priority, uint_64 _maxcnt) override {
                    return m_engine->clr_yield(clamp(_priority), _maxcnt);
                }

            private:
                uint_32 clamp(uint_32 _pri) const {
                    uint_32 cap = m_engine->config().yield_rsvprio;
                    return _pri > cap ? cap : _pri;
                }

                engine* m_engine;
                std::string m_name;
            };

        }

        const engine_config fwrap_common_impl::s_empty_cfg{};

        // A context without a walker (the entries) has nothing to hand back to a caller
        size_t fwrap_common_impl::size() const {
            throw script_exception{error_type::ContextError,
                                   std::string("size not available outside a native call")};
        }

        variant& fwrap_common_impl::operator[](size_t _i) const {
            throw script_exception{error_type::ContextError,
                                   std::string("argument access not available outside a native call")};
        }

        void fwrap_common_impl::freturn(const variant& _v) {
            throw script_exception{error_type::ContextError,
                                   std::string("freturn not available outside a native call")};
        }

        void fwrap_common_impl::freturn(variant&& _v) {
            throw script_exception{error_type::ContextError,
                                   std::string("freturn not available outside a native call")};
        }

        variant* fwrap_common_impl::iload(const std::string& _key) {
            throw script_exception{error_type::ContextError,
                                   std::string("iload not available outside a native call")};
        }

        variant fwrap_common_impl::call(const variant& _func, const varvec& _args) {
            throw script_exception{error_type::ContextError,
                                   std::string("call not available outside a native call")};
        }

        void fwrap_common_impl::raise(const variant& _info, error_type _what) {
            throw script_exception{_what, cov_string(_info)};
        }

        bool fwrap_common_impl::bind(const std::string& _name, void (*_func)(fwrap&),
                                     const std::string& _area) {
            throw script_exception{error_type::ContextError,
                                   std::string("bind not available in this entry point")};
        }

        bool fwrap_common_impl::bind(const std::string& _area, anyptr&& _obj,
                                     const std::map<std::string, void (*)(fwrap&)>& _methods) {
            throw script_exception{error_type::ContextError,
                                   std::string("bind(area, object, methods) not available in this entry point")};
        }

        bool fwrap_common_impl::bind(uint_64 _rid, const char* _name,
                                     std::string (*_desc)(const anyptr&), variant (*_view)(const anyptr&)) {
            throw script_exception{error_type::ContextError,
                                   std::string("type bind only available from the load entry point")};
        }

        anyptr* fwrap_common_impl::object() {
            throw script_exception{error_type::ContextError,
                                   std::string("object not available outside an area-bound native")};
        }

        variant* fwrap_common_impl::load(const std::string& _name) {
            if (!m_store) return nullptr;
            return m_store->find(_name);
        }

        variant* fwrap_common_impl::store(const std::string& _name, const variant& _val) {
            if (!m_store) return nullptr;
            return m_store->store(_name, _val);
        }

        variant* fwrap_common_impl::store(const std::string& _name, variant&& _val) {
            if (!m_store) return nullptr;
            return m_store->store(_name, std::move(_val));
        }

        bool fwrap_common_impl::remove(const std::string& _name) {
            if (!m_store) return false;
            return m_store->remove(_name);
        }

        const engine_config& fwrap_common_impl::config() const {
            return m_ws ? m_ws->m_cfg : s_empty_cfg;
        }

        fwrap::callback* fwrap_common_impl::currying(const std::string& _name) {
            throw script_exception{error_type::ContextError,
                                   std::string("currying not available outside a native call")};
        }

        bool fwrap_common_impl::bind_native(const std::string& _name, void (*_func)(fwrap&),
                                            const std::string& _area) {
            if (_area.empty()) {
                if (m_store->find(_name)) return false;
                m_store->store(_name, anyptr_ex<call_able>::make(new call_able(_func)));
                return true;
            }

            link_area* area = nullptr;
            variant* v = m_store->find(_area);
            if (v) {
                // The area name must already hold an area; anything else is a refusal, not a conversion
                if (!v->is<anyptr>()) return false;
                area = anyptr_ex<link_area>::as(v->to<anyptr>());
                if (!area) return false;
            }
            if (area) {
                if (area->m_natives.find(_name) != area->m_natives.end()) return false;
            } else {
                area = new link_area();
                m_store->store(_area, anyptr_ex<link_area>::make(area));
            }
            auto* ca = new call_able(_func);
            ca->m_area = area;
            area->m_natives[_name] = variant(anyptr_ex<call_able>::make(ca));
            return true;
        }

        bool fwrap_common_impl::bind_area(const std::string& _area, anyptr&& _obj,
                                          const std::map<std::string, void (*)(fwrap&)>& _methods) {
            if (_area.empty() || _obj.null() || m_store->find(_area)) return false;
            auto* area = new link_area();
            area->m_object = std::move(_obj);
            m_store->store(_area, anyptr_ex<link_area>::make(area));
            for (const auto& m : _methods) {
                auto* ca = new call_able(m.second);
                ca->m_area = area;
                area->m_natives[m.first] = variant(anyptr_ex<call_able>::make(ca));
            }
            return true;
        }

        fwrap_native::fwrap_native(std::vector<variant*>&& _args, variant* _ret,
                                   data_store* _store, walker* _ws)
            : fwrap_common_impl(_store, _ws), m_args(std::move(_args)), m_ret(_ret) {}

        size_t fwrap_native::size() const { return m_args.size(); }

        variant& fwrap_native::operator[](size_t _i) const {
            if (_i >= m_args.size() || !m_args[_i])
                throw script_exception{error_type::IndexError,
                                       std::string("argument index out of range: ") +
                                           std::to_string(_i)};
            return *m_args[_i];
        }

        // A call site that drops the value passes no return slot, so m_ret is null there
        void fwrap_native::freturn(const variant& _v) {
            if (m_ret) *m_ret = _v;
        }

        void fwrap_native::freturn(variant&& _v) {
            if (m_ret) *m_ret = std::move(_v);
        }

        variant* fwrap_native::iload(const std::string& _key) {
            if (_key.empty()) return nullptr;
            if (!m_ws) return nullptr;
            if (!m_store)
                throw script_exception{error_type::TypeError,
                                       std::string("iload: private data not available")};
            varvec iload;
            iload.push_back(variant(OPTYPE(O_ILOAD)));
            iload.push_back(variant(_key));
            // the write posture: _tail_create stays true — a missing terminal key is created
            return walker::resolve_iload(iload, *m_ws);
        }

        variant fwrap_native::call(const variant& _func, const varvec& _args) {
            if (!m_ws) throw script_exception{error_type::ContextError,
                                              std::string("call not available during load/unload")};

            call_able* ca = nullptr;
            // rl is never assigned: the delete rl calls below are no-ops
            thread_safe_rlock* rl = nullptr;

            if (_func.is<anyptr>()) {
                ca = anyptr_ex<call_able>::as(_func.to<anyptr>());
            } else if (_func.is<std::string>()) {
                std::string path = _func.to<std::string>();
                varvec iload;
                iload.push_back(variant(OPTYPE(O_ILOAD)));
                iload.push_back(variant(path));
                variant* v = walker::resolve_iload(iload, *m_ws, false);
                if (v && v->is<anyptr>())
                    ca = anyptr_ex<call_able>::as(v->to<anyptr>());
            }

            if (!ca) {
                delete rl;
                throw script_exception{error_type::NameError,
                                       std::string("call: function not found")};
            }

            if (ca->is_native()) {
                variant ret;
                // A local copy: the callee writes through pointers into it, not into the caller's vector
                varvec va(_args);
                std::vector<variant*> ptrs;
                ptrs.reserve(va.size());
                for (auto& a : va) ptrs.push_back(&a);
                fwrap_native fw(std::move(ptrs), &ret, m_store, m_ws);
                fw.m_area = ca->m_area;
                ca->m_fn(fw);
                delete rl;
                return ret;
            }

            varvec tree;
            tree.push_back(variant(OPTYPE(O_CALL)));
            tree.push_back(std::string());
            for (const auto& a : _args) {
                varvec leaf;
                leaf.push_back(a);
                tree.push_back(variant(std::move(leaf)));
            }
            std::string fn = ca->m_def->size() >= 2 && (*ca->m_def)[1].is<std::string>()
                                 ? (*ca->m_def)[1].to<std::string>()
                                 : std::string();
            auto args = walker::scan_call_args(tree, 2, *m_ws);
            variant result = walker::invoke_def(m_ws->state.current, ca, fn, tree, *m_ws, args);
            delete rl;
            return result;
        }

        anyptr* fwrap_native::object() {
            if (!m_area)
                throw script_exception{error_type::ContextError,
                                       std::string("object() outside an area-bound native")};
            return &m_area->m_object;
        }

        fwrap::callback* fwrap_native::currying(const std::string& _name) {
            // The caller owns the returned handle; a load/unload entry point has no engine to name
            if (!m_ws || !m_ws->state.m_engine)
                throw script_exception{error_type::ContextError,
                                       std::string("currying not available during load/unload")};
            return new currying_handle(m_ws->state.m_engine, _name);
        }

        bool fwrap_native::bind(const std::string& _name, void (*_func)(fwrap&),
                                const std::string& _area) {
            return bind_native(_name, _func, _area);
        }

        bool fwrap_native::bind(const std::string& _area, anyptr&& _obj,
                                const std::map<std::string, void (*)(fwrap&)>& _methods) {
            return bind_area(_area, std::move(_obj), _methods);
        }

        bool fwrap_entry_impl::bind(const std::string& _name, void (*_func)(fwrap&),
                                    const std::string& _area) {
            return bind_native(_name, _func, _area);
        }

        bool fwrap_entry_impl::bind(const std::string& _area, anyptr&& _obj,
                                    const std::map<std::string, void (*)(fwrap&)>& _methods) {
            return bind_area(_area, std::move(_obj), _methods);
        }

        bool fwrap_load::bind(uint_64 _rid, const char* _name,
                              std::string (*_desc)(const anyptr&), variant (*_view)(const anyptr&)) {
            if (!m_mgr || !_name || !_name[0]) return false;
            if (m_mgr->type_taken(_rid)) return false;
            for (const auto& p : m_pending)
                if (p.m_rid == _rid) return false;
            m_pending.push_back({_rid, _name, _desc, _view});
            return true;
        }

    }
}
