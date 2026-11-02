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
#include "ascript_utils.h"

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
                case O_ACT:
                    r.act_base = _s.act_parent(r.act_base);
                    return;
                default: return;
                }
            }

            if (!elem.is<std::string>())
                throw script_exception{error_type::TypeError,
                                       std::string("expected key name in entity context")};

            std::string key = elem.to<std::string>();

            variant* v = nullptr;
            bool in_frame = false;
            ParentKind pk = ParentKind::Entity;
            // the frame chain is consulted only while the chain still stands in the running activation's entity
            if (_s.current && !_s.frames->empty() && _s.frames->back().ent == _s.current) {
                v = _s.var_ptr(key, &in_frame, r.act_base);
                if (v) pk = in_frame ? ParentKind::Frame : ParentKind::Entity;
            } else if (_s.current) {
                v = _s.current->m_store.find(key);
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
                    r.act_base = -1;
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

        DotTerminal classify_dot(const dot_resolved& _r, const variant* _found) {
            DotTerminal t;
            t.value = _found;
            switch (_r.kind) {
            case TerminalKind::T_Callable:
                t.where = _r.parent_kind == ParentKind::Link   ? DotWhere::LinkTable
                          : _r.parent_kind == ParentKind::Area ? DotWhere::AreaTable
                                                               : DotWhere::Slot;
                t.what = DotWhat::Callable;
                break;
            case TerminalKind::T_Variant:
                if (_r.link_owner) {
                    t.where = DotWhere::LinkTable;
                    t.what = DotWhat::Opaque;
                } else {
                    t.where = _r.parent ? DotWhere::Element : DotWhere::None;
                }
                break;
            case TerminalKind::T_Slot:
            default:
                t.where = _r.parent_kind == ParentKind::Link   ? DotWhere::LinkTable
                          : _r.parent_kind == ParentKind::Area ? DotWhere::AreaTable
                                                               : DotWhere::Slot;
                if (t.value && t.value->is<anyptr>()) {
                    const anyptr& ap = t.value->to<anyptr>();
                    if (anyptr_ex<call_able>::as(ap)) t.what = DotWhat::Callable;
                    else if (anyptr_ex<impl_import>::as(ap) || anyptr_ex<impl_link>::as(ap) ||
                             anyptr_ex<link_area>::as(ap))
                        t.what = DotWhat::Resource;
                }
                break;
            }
            return t;
        }

        DotMiss dot_miss(walk_state& _s, const std::string& _name) {
            if (_s.var_in_frame(_name)) return DotMiss::FrameLocal;
            if (_s.var_ptr(_name)) return DotMiss::CallersModule;
            return DotMiss::None;
        }

        bool dot_failure(DotOp _op, const dot_resolved& _r, const DotTerminal& _t, DotMiss _miss,
                         error_type& _type, std::string& _msg) {
            const std::string k = _r.key.is<std::string>() ? _r.key.to<std::string>() : std::string();
            const bool have = _t.value != nullptr;
            const bool is_map = _r.parent && _r.parent->is<varmap>();
            const bool is_seq = _r.parent && (_r.parent->is<varvec>() || _r.parent->is<varlst>());

            switch (_op) {
            case DotOp::Read:
                if (_t.where == DotWhere::LinkTable) {
                    _type = error_type::TypeError;
                    _msg = "link namespace cannot be read as variable";
                    return true;
                }
                if (_r.kind == TerminalKind::T_Slot || _r.kind == TerminalKind::T_Callable) {
                    if (have) return false;
                    _type = error_type::NameError;
                    _msg = "Undefined: " + k;
                    return true;
                }
                if (_r.kind != TerminalKind::T_Variant || !_r.parent) return false;
                if (_r.key.null()) {
                    _type = error_type::TypeError;
                    _msg = "type does not support [null] index";
                    return true;
                }
                if (_r.key.is<int_64>()) {
                    if (is_seq) {
                        _type = error_type::IndexError;
                        _msg = "vec index out of range";
                    } else {
                        _type = error_type::TypeError;
                        _msg = "type does not support []";
                    }
                    return true;
                }
                if (_r.key.is<std::string>() && !is_map) {
                    _type = error_type::TypeError;
                    _msg = "not a map";
                    return true;
                }
                return false;

            case DotOp::Write:
                if (_t.where == DotWhere::LinkTable) {
                    _type = error_type::TypeError;
                    _msg = "cannot write to link module variable";
                    return true;
                }
                if (_t.where == DotWhere::AreaTable) {
                    _type = error_type::NameError;
                    _msg = "Cannot assign to function: " + k;
                    return true;
                }
                if (_r.kind != TerminalKind::T_Variant || !_r.parent) {
                    if (!have) {
                        _type = error_type::NameError;
                        _msg = "Undefined: " + k;
                        return true;
                    }
                    return false;
                }
                if (_r.key.null()) {
                    _type = error_type::TypeError;
                    if (_r.parent->is<std::string>()) _msg = "string does not support index write";
                    else if (is_map) _msg = "map does not support [null] write";
                    else _msg = "type does not support [null] write";
                    return true;
                }
                if (_r.key.is<int_64>()) {
                    if (is_seq) {
                        _type = error_type::IndexError;
                        _msg = "vec index out of range";
                    } else if (_r.parent->is<std::string>()) {
                        _type = error_type::TypeError;
                        _msg = "string does not support index write";
                    } else {
                        _type = error_type::TypeError;
                        _msg = "type does not support []";
                    }
                    return true;
                }
                if (_r.key.is<std::string>() && !is_map) {
                    _type = error_type::TypeError;
                    _msg = "not a map";
                    return true;
                }
                return false;

            case DotOp::Call:
                if (have) {
                    if (_t.what == DotWhat::Callable) return false;
                    if (_t.what == DotWhat::Data) {
                        _type = error_type::NameError;
                        _msg = std::string("not a function: ") + type_name_script(*_t.value);
                        return true;
                    }
                    _type = error_type::NameError;
                    _msg = "Undefined function: " + k;
                    return true;
                }
                if (_miss == DotMiss::FrameLocal) {
                    _type = error_type::TypeError;
                    _msg = "not a member of the entity: " + k + " is a frame local, call it unqualified";
                    return true;
                }
                _type = error_type::NameError;
                _msg = _miss == DotMiss::CallersModule
                           ? "Undefined function: " + k + " (it belongs to the caller's module, not to that entity)"
                           : "Undefined function: " + k;
                return true;

            case DotOp::Delete:
                if (_t.what == DotWhat::Callable) {
                    _type = error_type::TypeError;
                    _msg = _t.where == DotWhere::LinkTable || _t.where == DotWhere::AreaTable
                               ? "cannot delete function: " + k
                               : "cannot delete callable via dot";
                    return true;
                }
                if (_r.key.null()) {
                    _type = error_type::ConvError;
                    _msg = "Cannot delete with null index";
                    return true;
                }
                if (_t.where == DotWhere::LinkTable) {
                    _type = error_type::TypeError;
                    _msg = "cannot delete link data variable" + (k.empty() ? std::string() : ": " + k);
                    return true;
                }
                if (_t.where == DotWhere::AreaTable) {
                    _type = error_type::TypeError;
                    _msg = "cannot delete function: " + k;
                    return true;
                }
                if (_t.where == DotWhere::Slot && have) {
                    _type = error_type::TypeError;
                    _msg = "Type does not support delete ." + k;
                    return true;
                }
                if (_t.where == DotWhere::Element && _r.parent && !is_map && !is_seq) {
                    _type = error_type::TypeError;
                    _msg = "Type does not support delete [i]";
                    return true;
                }
                return false;
            }
            return false;
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

            // a callable reached before the terminal key: the chain asked to walk past a function
            if (r.kind == TerminalKind::T_Callable)
                throw script_exception{error_type::TypeError,
                                       std::string("cannot navigate beyond a callable")};

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
