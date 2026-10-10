/*****************************************************************/ /**
 * \file   ascript_fwrap.cpp
 * \brief  fwrap implementation — per-call native interface
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_fwrap.h"

namespace alx {
    namespace script {

        fwrap_impl::fwrap_impl(std::vector<variant*>&& _args, variant* _ret,
                               data_store* _store, walker* _ws)
            : m_args(std::move(_args)), m_ret(_ret),
              m_store(_store), m_ws(_ws) {}

        const engine_config fwrap_impl::s_empty_cfg{};

        size_t fwrap_impl::size() const { return m_args.size(); }

        variant& fwrap_impl::operator[](size_t _i) const {
            if (_i >= m_args.size() || !m_args[_i])
                throw script_exception{error_type::IndexError,
                                       std::string("argument index out of range: ") +
                                           std::to_string(_i)};
            return *m_args[_i];
        }

        // A load/unload entry point has no return slot, so m_ret is null there and the value is dropped
        void fwrap_impl::freturn(const variant& _v) {
            if (m_ret) *m_ret = _v;
        }

        void fwrap_impl::freturn(variant&& _v) {
            if (m_ret) *m_ret = std::move(_v);
        }

        variant* fwrap_impl::load(const std::string& _name) {
            if (!m_store) return nullptr;
            return m_store->find(_name);
        }

        variant* fwrap_impl::store(const std::string& _name, const variant& _val) {
            if (!m_store) return nullptr;
            return m_store->store(_name, _val);
        }

        variant* fwrap_impl::store(const std::string& _name, variant&& _val) {
            if (!m_store) return nullptr;
            return m_store->store(_name, std::move(_val));
        }

        bool fwrap_impl::remove(const std::string& _name) {
            if (!m_store) return false;
            return m_store->remove(_name);
        }

        variant* fwrap_impl::iload(const std::string& _key) {
            if (_key.empty()) return nullptr;
            if (!m_ws) return nullptr;
            if (!m_store)
                throw script_exception{error_type::TypeError,
                                       std::string("iload: private data not available")};
            varvec iload;
            iload.push_back(variant(OPTYPE(O_ILOAD)));
            iload.push_back(variant(_key));
            // true = navigation: a path that resolves to nothing yields null instead of throwing
            return walker::resolve_iload(iload, *m_ws);
        }

        variant fwrap_impl::call(const variant& _func, const varvec& _args) {
            if (!m_ws) throw script_exception{error_type::LinkError,
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
                fwrap_impl fw(std::move(ptrs), &ret, m_store, m_ws);
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
            variant result = walker::invoke_def(m_ws->state.current, *ca->m_def, fn, tree, *m_ws);
            delete rl;
            return result;
        }

        void fwrap_impl::raise(const variant& _info, error_type _what) {
            throw script_exception{_what, cov_string(_info)};
        }

        void fwrap_impl::bind(const std::string& _name, void (*_func)(fwrap&),
                              const std::string& _area) {

            if (_area.empty()) {
                m_store->store(_name, anyptr_ex<call_able>::make(new call_able(_func)));
            } else {
                link_area* area = nullptr;
                variant* v = m_store->find(_area);
                if (v && v->is<anyptr>()) {
                    area = anyptr_ex<link_area>::as(v->to<anyptr>());
                    if (!area) {
                        area = new link_area();
                        anyptr dummy;
                        // An object already stored under _area is moved into the new area instead of being lost
                        std::swap(area->m_object, v->to(dummy));
                        *v = anyptr_ex<link_area>::make(area);
                    }
                }
                if (!area) {
                    area = new link_area();
                    m_store->store(_area, anyptr_ex<link_area>::make(area));
                }
                auto* ca = new call_able(_func);
                ca->m_area = area;
                area->m_natives[_name] = variant(anyptr_ex<call_able>::make(ca));
            }
        }

        anyptr* fwrap_impl::object() {
            return &m_area->m_object;
        }

    }
}
