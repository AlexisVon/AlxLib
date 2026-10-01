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
            // Frame/Entity: the key is a name, so deletion goes to frames.back() /
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

            bool is_callable() const { return kind == TerminalKind::T_Callable; }

            // A missing or non-indexable key gives nullptr, not an exception; with
            // _readonly = false a missing key is created (map key, or append on a null index).
            variant* get(bool _readonly = true) const;
        };

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
