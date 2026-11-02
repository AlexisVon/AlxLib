/*****************************************************************/ /**
 * \file   ascript_dot.h
 * \brief  Dot chain resolution — unified locate + access + terminal dispatch
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_DOT_H_
#define _ALEXIS_SCRIPT_DOT_H_

#include "ascript_base.h"

namespace alx {
    namespace script {

        // What `parent` is, which is how del_dot picks the erase path: Map/Vec/Lst mean
        // parent IS that container; Value means a leaf nothing can be indexed through.
        enum class ParentKind : uint_8 {
            Unknown,
            // Frame/Entity: the key is a name, so deletion goes to frames->back() /
            // slot_owner->m_store, and a non-null parent there is refused as a leaf.
            Frame,
            Entity,
            Link,
            Area,
            Map,
            Vec,
            Lst,
            Value,
        };

        // What the key resolved to: a container element, a named slot (var/import/link/area), or a callable.
        enum class TerminalKind : uint_8 {
            T_Variant,
            T_Slot,
            T_Callable
        };

        struct dot_resolved {
            TerminalKind kind = TerminalKind::T_Slot;
            // Terminal element: string key, int_64 index (-1 = last element), or null for none.
            variant key;
            // Entity the chain ended in; the terminal name is looked up here when parent is null.
            impl_import* slot_owner = nullptr;
            // Link the chain entered; an area's natives take their data store from it.
            impl_link* link_owner = nullptr;

            // The container/slot the key indexes; null = the key is a name in slot_owner instead.
            variant* parent = nullptr;
            ParentKind parent_kind = ParentKind::Unknown;

            // Frame index the chain's names resolve from; -1 = the running activation (set by O_ACT).
            int_32 act_base = -1;

            bool is_callable() const { return kind == TerminalKind::T_Callable; }
            // A terminal name that must resolve in the frame chain starting at act_base.
            bool is_act_name() const {
                return act_base >= 0 && kind == TerminalKind::T_Slot && !parent && key.is<std::string>();
            }

            // A missing or non-indexable key gives nullptr, not an exception; with
            // _readonly = false a missing key is created (map key, or append on a null index).
            variant* get(bool _readonly = true) const;
        };

        // Where the resolved terminal sits and what it is: the one classification every failure path shares.
        enum class DotWhere : uint_8 {
            None,
            Slot,
            Element,
            LinkTable,
            AreaTable,
        };

        enum class DotWhat : uint_8 {
            Data,
            Callable,
            Resource,
            Opaque,
        };

        struct DotTerminal {
            DotWhere where = DotWhere::None;
            DotWhat what = DotWhat::Data;
            // The resolved value when known (null = the name did not resolve).
            const variant* value = nullptr;
        };

        enum class DotOp : uint_8 { Read,
                                    Write,
                                    Call,
                                    Delete };

        // Why a qualified call missed: a frame local of the caller, a name in the caller's module store, or nowhere.
        enum class DotMiss : uint_8 { None,
                                      FrameLocal,
                                      CallersModule };

        // Classify a resolved terminal; _found is the value the caller looked up (null = the name did not resolve).
        DotTerminal classify_dot(const dot_resolved& _r, const variant* _found = nullptr);
        DotMiss dot_miss(walk_state& _s, const std::string& _name);
        // The single failure table: true when this op treats the situation as an error, with its type and message.
        // false = the table has no answer for this op; the port applies its own rule (a read answers null,
        // a write falls through to its not-a-target tail).
        bool dot_failure(DotOp _op, const dot_resolved& _r, const DotTerminal& _t, DotMiss _miss,
                         error_type& _type, std::string& _msg);

        // Navigates dot_ast[1 .. n-2] (element 0 is the O_DOT opcode) and leaves the
        // terminal element in r.key for the caller to dispatch; _s.current is restored.
        dot_resolved resolve_dot(const varvec& dot_ast, walk_state& _s);

        namespace step {
            void ent(dot_resolved& r, const variant& elem, walk_state& _s);
            void link(dot_resolved& r, const variant& elem, walk_state& _s);
            void area(dot_resolved& r, const variant& elem, walk_state& _s);
            void field(dot_resolved& r, const variant& elem, walk_state& _s);
        }

    }
}

#endif
