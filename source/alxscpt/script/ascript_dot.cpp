/*****************************************************************/ /**
 * \file   ascript_dot.cpp
 * \brief  Dot chain resolution — step + resolve_dot + get
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_dot.h"

namespace alx {
    namespace script {

        void step::ent(dot_resolved& r, const variant& elem, walk_state& _s) {
            if (elem.is<OPTYPE>()) {
                auto op = static_cast<op_enum>(elem.to<OPTYPE>());
                switch (op) {
                case O_CURRENT: return;
                case O_PARENT:
                    if (!_s.current->m_parent)
                        throw script_exception{error_type::NavError,
                                               std::string(".. exceeds entity tree depth")};
                    _s.current = _s.current->m_parent;
                    return;
                case O_ROOT:
                    _s.current = _s.root;
                    return;
                default: return;
                }
            }

            if (!elem.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("expected key name in entity context")};

            std::string key = elem.to<std::string>();

            variant* v = nullptr;
            ParentKind pk = ParentKind::Entity;
            // innermost frame first, and only frames of the current entity: a hit here is a block-local
            for (auto it = _s.frames.rbegin(); it != _s.frames.rend(); ++it) {
                if (it->ent != _s.current) break;
                size_t idx;
                v = it->find(key, &idx);
                if (v) {
                    pk = ParentKind::Frame;
                    break;
                }
            }
            if (!v && _s.current) {
                v = _s.current->m_store.find(key);
                pk = ParentKind::Entity;
            }
            if (!v)
                throw script_exception{error_type::NameError,
                                       std::string("Undefined: " + key)};

            if (v->is<anyptr>()) {
                const anyptr& ap = v->to<anyptr>();
                if (anyptr_ex<call_able>::as(ap)) {
                    r.kind = TerminalKind::T_Callable;
                    r.parent = v;
                    r.parent_kind = pk;
                    return;
                }
                if (anyptr_ex<impl_import>::as(ap)) {
                    _s.current = anyptr_ex<impl_import>::as(ap);
                    r.parent = nullptr;
                    r.kind = TerminalKind::T_Slot;
                    // the terminal key belongs to the entity just entered, not to the frame the alias was found in
                    r.parent_kind = ParentKind::Entity;
                    return;
                }
                if (anyptr_ex<impl_link>::as(ap)) {
                    r.parent = v;
                    r.kind = TerminalKind::T_Slot;
                    r.parent_kind = ParentKind::Link;
                    return;
                }
                if (anyptr_ex<link_area>::as(ap)) {
                    r.parent = v;
                    r.kind = TerminalKind::T_Slot;
                    r.parent_kind = ParentKind::Area;
                    return;
                }
            }

            r.kind = TerminalKind::T_Variant;
            r.parent = v;
            if (v->is<varmap>()) r.parent_kind = ParentKind::Map;
            else if (v->is<varvec>()) r.parent_kind = ParentKind::Vec;
            else if (v->is<varlst>()) r.parent_kind = ParentKind::Lst;
            else r.parent_kind = pk;
        }

        void step::link(dot_resolved& r, const variant& elem, walk_state&) {
            if (!elem.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("expected key name in link context")};

            std::string key = elem.to<std::string>();

            impl_link* link = nullptr;
            if (r.parent && r.parent->is<anyptr>())
                link = anyptr_ex<impl_link>::as(r.parent->to<anyptr>());

            if (!link)
                throw script_exception{error_type::TypeError,
                                       std::string("not a link context")};

            // callers write and delete through this store, not through the variant that parent points at
            r.link_owner = link;
            variant* v = link->m_store.find(key);
            if (!v)
                throw script_exception{error_type::NameError,
                                       std::string("Undefined: " + key)};

            if (v->is<anyptr>()) {
                const anyptr& ap = v->to<anyptr>();
                if (anyptr_ex<call_able>::as(ap)) {
                    r.kind = TerminalKind::T_Callable;
                    r.parent = v;
                    r.parent_kind = ParentKind::Link;
                    return;
                }
                if (anyptr_ex<link_area>::as(ap)) {
                    r.parent = v;
                    r.kind = TerminalKind::T_Slot;
                    r.parent_kind = ParentKind::Area;
                    return;
                }
            }

            r.kind = TerminalKind::T_Variant;
            r.parent = v;
            r.parent_kind = ParentKind::Link;
        }

        void step::area(dot_resolved& r, const variant& elem, walk_state&) {
            if (!elem.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("expected key name in area context")};

            std::string key = elem.to<std::string>();

            link_area* area = nullptr;
            if (r.parent && r.parent->is<anyptr>())
                area = anyptr_ex<link_area>::as(r.parent->to<anyptr>());

            if (!area)
                throw script_exception{error_type::TypeError,
                                       std::string("not an area context")};

            auto ni = area->m_natives.find(key);
            if (ni != area->m_natives.end()) {
                if (ni->second.is<anyptr>() && anyptr_ex<call_able>::as(ni->second.to<anyptr>())) {
                    r.kind = TerminalKind::T_Callable;
                    r.parent = &ni->second;
                    r.parent_kind = ParentKind::Area;
                    return;
                }
                r.kind = TerminalKind::T_Variant;
                r.parent = &ni->second;
                r.parent_kind = ParentKind::Area;
                return;
            }

            throw script_exception{error_type::NameError,
                                   std::string("Undefined: " + key)};
        }

        // must describe what parent points at, not the container it was read from -- del_dot dispatches on it
        static void set_parent_kind(dot_resolved& _r) {
            if (_r.parent->is<varmap>()) _r.parent_kind = ParentKind::Map;
            else if (_r.parent->is<varvec>()) _r.parent_kind = ParentKind::Vec;
            else if (_r.parent->is<varlst>()) _r.parent_kind = ParentKind::Lst;
            else _r.parent_kind = ParentKind::Value;
        }

        void step::field(dot_resolved& r, const variant& elem, walk_state&) {
            // an operator element has no meaning once the chain is on a value: skipped, so .. does not climb here
            if (elem.is<OPTYPE>()) return;

            if (elem.is<std::string>()) {
                if (!r.parent->is<varmap>())
                    throw script_exception{error_type::TypeError,
                                           std::string("not a map")};
                alx::varmap dummy;
                auto& map = r.parent->to(dummy);
                std::string key = elem.to<std::string>();
                if (!map.contain(key))
                    throw script_exception{error_type::KeyError,
                                           std::string("map key not found: " + key)};
                r.parent = &map[key];
                set_parent_kind(r);
                return;
            }

            if (elem.is<int_64>()) {
                int_64 i = elem.to<int_64>();
                if (r.parent->is<varvec>()) {
                    alx::varvec dummy;
                    auto& vec = r.parent->to(dummy);
                    if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
                    if (i < 0 || static_cast<size_t>(i) >= vec.size())
                        throw script_exception{error_type::IndexError,
                                               std::string("vec index out of range")};
                    r.parent = &vec[static_cast<size_t>(i)];
                    set_parent_kind(r);
                    return;
                }
                if (r.parent->is<varlst>()) {
                    alx::varlst dummy;
                    auto& lst = r.parent->to(dummy);
                    if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
                    if (i < 0 || static_cast<size_t>(i) >= lst.size())
                        throw script_exception{error_type::IndexError,
                                               std::string("lst index out of range")};
                    auto it = lst.begin();
                    for (int_64 n = 0; n < i; ++n) ++it;
                    r.parent = &(*it);
                    set_parent_kind(r);
                    return;
                }
                if (r.parent->is<std::string>())
                    throw script_exception{error_type::TypeError,
                                           std::string("cannot navigate into string character")};
                throw script_exception{error_type::TypeError,
                                       std::string("type does not support []")};
            }

            if (elem.null())
                throw script_exception{error_type::IndexError,
                                       std::string("null index only allowed as terminal")};

            throw script_exception{error_type::TypeError,
                                   std::string("unexpected element type in variant context")};
        }

        dot_resolved resolve_dot(const varvec& dot_ast, walk_state& _s) {
            dot_resolved r;
            r.kind = TerminalKind::T_Slot;
            r.parent = nullptr;

            // _s.current is repointed at each entity the walk enters; restored before returning
            impl_import* orig_current = _s.current;
            impl_import* nav_ent = _s.current;

            if (dot_ast.size() < 3) {
                r.slot_owner = nav_ent;
                r.key = dot_ast.size() > 1 ? dot_ast[1] : variant();
                return r;
            }

            for (size_t i = 1; i + 1 < dot_ast.size(); i++) {
                auto& elem = dot_ast[i];

                if (r.kind == TerminalKind::T_Variant) {
                    step::field(r, elem, _s);
                    continue;
                }

                if (r.kind == TerminalKind::T_Callable)
                    throw script_exception{error_type::TypeError,
                                           std::string("cannot navigate beyond a callable")};

                if (r.parent && r.parent->is<anyptr>()) {
                    const anyptr& ap = r.parent->to<anyptr>();
                    if (anyptr_ex<link_area>::as(ap)) {
                        step::area(r, elem, _s);
                        continue;
                    }
                    if (anyptr_ex<impl_link>::as(ap)) {
                        step::link(r, elem, _s);
                        continue;
                    }
                    if (anyptr_ex<impl_import>::as(ap)) {
                        nav_ent = anyptr_ex<impl_import>::as(ap);
                        r.parent = nullptr;
                        r.kind = TerminalKind::T_Slot;
                        continue;
                    }
                }

                _s.current = nav_ent;
                step::ent(r, elem, _s);
                nav_ent = _s.current;
            }

            // the last element is not walked: it becomes the terminal key, resolved against parent or slot_owner
            r.key = dot_ast.back();
            r.slot_owner = nav_ent;
            _s.current = orig_current;

            return r;
        }

        variant* dot_resolved::get(bool _readonly) const {
            // _readonly: the read path never creates (a miss or a null key becomes nullptr); the write path may insert or append
            switch (kind) {
            case TerminalKind::T_Variant: {
                if (!parent) return nullptr;

                if (key.is<std::string>()) {
                    if (!parent->is<varmap>()) return nullptr;
                    alx::varmap dummy;
                    auto& map = parent->to(dummy);
                    std::string k = key.to<std::string>();
                    if (!map.contain(k)) {
                        if (!_readonly) return &map[k];
                        return nullptr;
                    }
                    return &map[k];
                }

                if (key.is<int_64>()) {
                    int_64 i = key.to<int_64>();
                    if (parent->is<varvec>()) {
                        alx::varvec dummy;
                        auto& vec = parent->to(dummy);
                        if (i == -1) i = static_cast<int_64>(vec.size()) - 1;
                        if (i < 0 || static_cast<size_t>(i) >= vec.size()) return nullptr;
                        return &vec[static_cast<size_t>(i)];
                    }
                    if (parent->is<varlst>()) {
                        alx::varlst dummy;
                        auto& lst = parent->to(dummy);
                        if (i == -1) i = static_cast<int_64>(lst.size()) - 1;
                        if (i < 0 || static_cast<size_t>(i) >= lst.size()) return nullptr;
                        auto it = lst.begin();
                        for (int_64 n = 0; n < i; ++n) ++it;
                        return &(*it);
                    }
                    return nullptr;
                }

                // null key: the write path appends an empty element and returns it for the caller to fill
                if (key.null()) {
                    if (!_readonly) {
                        if (parent->is<varvec>()) {
                            alx::varvec dummy;
                            parent->to(dummy).push_back(variant());
                            return &parent->to(dummy).back();
                        }
                        if (parent->is<varlst>()) {
                            alx::varlst dummy;
                            parent->to(dummy).push_back(variant());
                            return &parent->to(dummy).back();
                        }
                    }
                    return nullptr;
                }
                return nullptr;
            }

            case TerminalKind::T_Slot:

                if (parent_kind == ParentKind::Area && parent && parent->is<anyptr>() && key.is<std::string>()) {
                    auto* area = anyptr_ex<link_area>::as(parent->to<anyptr>());
                    if (area) {
                        auto ni = area->m_natives.find(key.to<std::string>());
                        if (ni != area->m_natives.end())
                            return &ni->second;
                    }
                    return nullptr;
                }
                // not an area native: resolve like a callable -- parent if set, else the key in slot_owner's store
                [[fallthrough]];
            case TerminalKind::T_Callable:
                if (parent)
                    return parent;

                if (slot_owner && key.is<std::string>()) {
                    return slot_owner->m_store.find(key.to<std::string>());
                }
                return nullptr;

            default: return nullptr;
            }
        }

    }
}
