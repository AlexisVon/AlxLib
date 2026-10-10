/*****************************************************************/ /**
 * \file   ascript_base.h
 * \brief  Script base types — shared by walk, resmng, fwrap
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_BASE_H_
#define _ALEXIS_SCRIPT_BASE_H_

#include "abase.h"
#include "arefcount.h"
#include "ascript.h"
#include "ascript_enum.h"
#include <atomic>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace alx {
    namespace script {

        struct link_area;

        // the union member in use is picked by m_is_def; m_def points into the def's AST, never owned here
        class call_able {
        public:
            call_able() = default;
            call_able(const varvec* def) : m_is_def(true), m_def(def) {}
            call_able(native_func fn) : m_is_def(false), m_fn(fn) {}

            bool is_def() const { return m_is_def; }
            bool is_native() const { return !m_is_def; }

            bool m_is_def = true;
            union {
                const varvec* m_def = nullptr;
                native_func m_fn;
            };
            link_area* m_area = nullptr;
        };

        struct link_area {
            anyptr m_object;
            // each value is an anyptr holding a call_able, keyed by native method name
            std::unordered_map<std::string, variant> m_natives;

            link_area() = default;
            /// A copy's natives re-point at this copy: a cloned store never reaches the original's host object
            link_area(const link_area& _o) : m_object(_o.m_object), m_natives(_o.m_natives) { rebind(); }
            link_area& operator=(const link_area& _o) {
                if (this == &_o) return *this;
                m_object = _o.m_object;
                m_natives = _o.m_natives;
                rebind();
                return *this;
            }

        private:
            void rebind() {
                for (auto& n : m_natives) {
                    if (!n.second.is<anyptr>()) continue;
                    if (auto* ca = anyptr_ex<call_able>::as(n.second.to<anyptr>()))
                        ca->m_area = this;
                }
            }
        };

        struct data_store {
            std::vector<variant> m_data;
            std::unordered_map<std::string, size_t> m_map;
            std::vector<size_t> m_free;

            void clear() {
                m_data.clear();
                m_map.clear();
                m_free.clear();
            }

            variant* find(const std::string& _name) {
                auto it = m_map.find(_name);
                if (it == m_map.end()) return nullptr;
                return (it->second < m_data.size()) ? &m_data[it->second] : nullptr;
            }

            variant* store(const std::string& _name, const variant& _val) {
                auto it = m_map.find(_name);
                if (it != m_map.end())
                    throw script_exception{error_type::NameError,
                                           std::string("duplicate variable: ") + _name};
                size_t idx;
                if (!m_free.empty()) {
                    idx = m_free.back();
                    m_free.pop_back();
                    m_data[idx] = _val;
                } else {
                    idx = m_data.size();
                    m_data.push_back(_val);
                }
                m_map[_name] = idx;
                return &m_data[idx];
            }

            variant* store(const std::string& _name, variant&& _val) {
                auto it = m_map.find(_name);
                if (it != m_map.end())
                    throw script_exception{error_type::NameError,
                                           std::string("duplicate variable: ") + _name};
                size_t idx;
                if (!m_free.empty()) {
                    idx = m_free.back();
                    m_free.pop_back();
                    m_data[idx] = std::move(_val);
                } else {
                    idx = m_data.size();
                    m_data.push_back(std::move(_val));
                }
                m_map[_name] = idx;
                return &m_data[idx];
            }

            bool contain(const std::string& _name) const {
                return m_map.find(_name) != m_map.end();
            }

            // freed slots are recycled by a later store(), so a variant* held across both calls can alias another variable
            bool remove(const std::string& _name) {
                auto it = m_map.find(_name);
                if (it == m_map.end()) return false;
                m_data[it->second] = variant();
                m_free.push_back(it->second);
                m_map.erase(it);
                return true;
            }
        };

        class res_mng;
        class mod_mng;
        class fly_import;
        class fly_link;
        class impl_import;
        class impl_link;
        struct ast_resource;
        struct link_resource;

        class impl_import {
        public:
            fly_import* m_fly = nullptr;
            mod_mng* m_mng = nullptr;
            data_store m_store;
            std::vector<std::string> m_env_paths;

            impl_import* m_parent = nullptr;
            std::string m_alias;

            impl_import() = default;

            impl_import(impl_import&&) noexcept = default;
            impl_import& operator=(impl_import&&) noexcept = default;

            // deep copy takes its own fly reference, released by the dtor
            impl_import(const impl_import& _o);
            impl_import& operator=(const impl_import& _o);

            ~impl_import();
        };

        class impl_link {
        public:
            fly_link* m_fly = nullptr;
            mod_mng* m_mng = nullptr;
            data_store m_store;

            impl_import* m_parent = nullptr;
            std::string m_alias;

            impl_link() = default;

            impl_link(impl_link&&) noexcept = default;
            impl_link& operator=(impl_link&&) noexcept = default;

            // deep copy takes its own fly reference, released by the dtor
            impl_link(const impl_link& _o);
            impl_link& operator=(const impl_link& _o);

            ~impl_link();
        };

        class fly_import {
        public:
            uint_64 m_id;
            std::string m_path;
            ast_resource* m_ast = nullptr;
            // the template root: the init walk runs directly on it, so what that walk binds as a parent
            // (a nested import / link) stays valid for as long as the fly is pooled
            impl_import m_root;

            ref_count m_ref;

            // init gate: written only by the owning engine, deliberately not atomic
            bool m_init_done = false;
            bool m_bad = false;
            error_type m_err_type = error_type::UnknownError;
            std::string m_err_value;
        };

        class fly_link {
        public:
            uint_64 m_id;
            std::string m_path;
            link_resource* m_link = nullptr;
            data_store m_store;

            ref_count m_ref;

            // init gate: written only by the owning engine, deliberately not atomic
            bool m_init_done = false;
            bool m_bad = false;
            error_type m_err_type = error_type::UnknownError;
            std::string m_err_value;
        };

        // a module handle belongs to the entity that holds it: every binding site stamps the slot it just wrote
        inline void bind_owner(const variant& _val, impl_import* _owner) {
            if (!_owner || !_val.is<anyptr>()) return;
            const anyptr& ap = _val.to<anyptr>();
            if (auto* imp = anyptr_ex<impl_import>::as(ap)) imp->m_parent = _owner;
            else if (auto* lnk = anyptr_ex<impl_link>::as(ap)) lnk->m_parent = _owner;
        }

        class walker;

        // pop_frame clears only the flags a frame owns: loops take BREAK|CONT, eval and function frames all four, blocks none
        enum frame_flag : uint_8 {
            FF_NONE = 0,
            FF_RET = 1 << 0,
            FF_BREAK = 1 << 1,
            FF_CONT = 1 << 2,
            FF_TAIL = 1 << 3,
        };

        // the hash pointer itself is the promoted flag: once promoted the slot array is dead, and clear() keeps it dead
        template <uint_64 SLOT_SIZE>
        class slot_map {
        public:
            using hash_type = std::unordered_map<std::string, uint_64>;

            static_assert(SLOT_SIZE > 0 && SLOT_SIZE <= 8, "SLOT_SIZE invalid!");

            struct slot_array {
                static constexpr uint_64 size = SLOT_SIZE;
                std::string key[SLOT_SIZE];
                uint_64 val[SLOT_SIZE];
                slot_array() {
                    for (uint_64 i = 0; i < SLOT_SIZE; i++) val[i] = uint_64_npos;
                }
            };

            slot_map() = default;
            ~slot_map() { delete hash; }

            slot_map(const slot_map& _o) : slot(_o.slot), hash(_o.hash ? new hash_type(*_o.hash) : nullptr) {}
            slot_map& operator=(const slot_map& _o) {
                if (this != &_o) {
                    slot = _o.slot;
                    delete hash;
                    hash = _o.hash ? new hash_type(*_o.hash) : nullptr;
                }
                return *this;
            }
            slot_map(slot_map&& _o) noexcept : slot(std::move(_o.slot)), hash(_o.hash) { _o.hash = nullptr; }
            slot_map& operator=(slot_map&& _o) noexcept {
                if (this != &_o) {
                    slot = std::move(_o.slot);
                    delete hash;
                    hash = _o.hash;
                    _o.hash = nullptr;
                }
                return *this;
            }

            void set(const std::string& _name, uint_64 _idx) {
                if (hash) {
                    (*hash)[_name] = _idx;
                    return;
                }
                uint_64 empty = SLOT_SIZE;
                for (uint_64 i = 0; i < SLOT_SIZE; i++) {
                    if (slot.val[i] == uint_64_npos) {
                        if (empty == SLOT_SIZE) empty = i;
                        continue;
                    }
                    if (slot.key[i] == _name) {
                        slot.val[i] = _idx;
                        return;
                    }
                }
                if (empty < SLOT_SIZE) {
                    slot.key[empty] = _name;
                    slot.val[empty] = _idx;
                    return;
                }
                hash = new hash_type();
                for (uint_64 i = 0; i < SLOT_SIZE; i++) (*hash)[slot.key[i]] = slot.val[i];
                (*hash)[_name] = _idx;
            }

            uint_64 get(const std::string& _name) const {
                if (hash) {
                    auto it = hash->find(_name);
                    return it == hash->end() ? uint_64_npos : it->second;
                }
                for (uint_64 i = 0; i < SLOT_SIZE; i++)
                    if (slot.val[i] != uint_64_npos && slot.key[i] == _name) return slot.val[i];
                return uint_64_npos;
            }

            bool has(const std::string& _name) const { return get(_name) != uint_64_npos; }

            uint_64 take(const std::string& _name) {
                if (hash) {
                    auto it = hash->find(_name);
                    if (it == hash->end()) return uint_64_npos;
                    uint_64 idx = it->second;
                    hash->erase(it);
                    return idx;
                }
                for (uint_64 i = 0; i < SLOT_SIZE; i++)
                    if (slot.val[i] != uint_64_npos && slot.key[i] == _name) {
                        uint_64 idx = slot.val[i];
                        slot.val[i] = uint_64_npos;
                        return idx;
                    }
                return uint_64_npos;
            }

            void clear() {
                if (hash) {
                    hash->clear();
                    return;
                }
                for (uint_64 i = 0; i < SLOT_SIZE; i++) slot.val[i] = uint_64_npos;
            }

            bool is_promoted() const { return hash != nullptr; }
            const slot_array& get_slot() const { return slot; }
            const hash_type& get_hash() const { return *hash; }

        private:
            slot_array slot;
            hash_type* hash = nullptr;
        };

        // row/col are 1-based; row 0 means no position has been recorded
        struct src_pos {
            uint_32 row = 0;
            uint_32 col = 0;
            uint_32 ofst = 0;
        };

        struct scope_frame {
            impl_import* ent = nullptr;
            size_t base = 0;
            // free slot offsets relative to base (data_store::m_free holds absolute indices)
            std::vector<size_t> free;
            slot_map<4> var_map;
            const varvec* def = nullptr;
            // names this layer may not delete: the for/foreach head snapshot
            const slot_map<4>* prot = nullptr;
            uint_8 flags = FF_NONE;
            src_pos pos;

            scope_frame() = default;
            scope_frame(const scope_frame&) = delete;
            scope_frame& operator=(const scope_frame&) = delete;

            explicit scope_frame(impl_import* _ent)
                : ent(_ent) {
                if (!ent) return;
                base = ent->m_store.m_data.size();
            }

            // _other.ent is nulled so the moved-from frame's dtor does not truncate the store under the new owner
            scope_frame(scope_frame&& _other) noexcept
                : ent(_other.ent), base(_other.base),
                  free(std::move(_other.free)),
                  var_map(std::move(_other.var_map)),
                  def(_other.def), prot(_other.prot), flags(_other.flags), pos(_other.pos) {
                _other.ent = nullptr;
            }

            scope_frame& operator=(scope_frame&& _other) noexcept {
                if (this != &_other) {
                    ent = _other.ent;
                    base = _other.base;
                    free = std::move(_other.free);
                    var_map = std::move(_other.var_map);
                    def = _other.def;
                    prot = _other.prot;
                    flags = _other.flags;
                    pos = _other.pos;
                    _other.ent = nullptr;
                }
                return *this;
            }

            // the frame owns ent->m_store.m_data[base..size), so the dtor truncates the store back to base
            ~scope_frame() {
                if (!ent) return;
                ent->m_store.m_data.resize(base);
            }

            variant* find(const std::string& _name, size_t* _idx = nullptr) const {
                if (!ent) return nullptr;
                uint_64 vi = var_map.get(_name);
                if (vi == uint_64_npos || vi >= ent->m_store.m_data.size()) return nullptr;
                if (_idx) *_idx = vi;
                return const_cast<variant*>(&ent->m_store.m_data[vi]);
            }

            // takes the var_map entry even when the variable is an outer frame's, then reports false without freeing it
            bool remove(const std::string& _name) {
                if (prot && prot->has(_name)) return false;
                uint_64 vi = var_map.take(_name);
                if (vi == uint_64_npos) return false;
                if (vi < base) return false;
                free.push_back(vi - base);
                return true;
            }

            variant* store(const std::string& _name, const variant& _val) {
                if (!ent) return nullptr;
                return raw_store(_name, variant(_val));
            }

            variant* store(const std::string& _name, variant&& _val) {
                if (!ent) return nullptr;
                return raw_store(_name, std::move(_val));
            }

            bool contain(const std::string& _name) const {
                return var_map.has(_name);
            }

        private:
            variant* raw_store(const std::string& _name, variant&& _val) {
                auto& ds = ent->m_store;
                size_t idx;
                if (!free.empty()) {
                    size_t offset = free.back();
                    free.pop_back();
                    idx = base + offset;
                    ds.m_data[idx] = std::move(_val);
                } else {
                    ds.m_data.push_back(std::move(_val));
                    idx = ds.m_data.size() - 1;
                }
                var_map.set(_name, idx);
                bind_owner(ds.m_data[idx], ent);
                return &ds.m_data[idx];
            }

        public:
        };

        struct walk_state {
            const varvec* code = nullptr;

            impl_import* root = nullptr;
            impl_import* root_entity = nullptr;

            std::deque<scope_frame> frames;

            impl_import* current = nullptr;

            src_pos top_pos;

            bool break_flag = false;
            bool cont_flag = false;
            bool ret_flag = false;
            bool tail_flag = false;

            const varmap* compiled_modules = nullptr;

            class engine* m_engine = nullptr;
            const std::list<std::string>* m_search_paths = nullptr;

            const alx::signal<const std::string&>* on_cerr = nullptr;
            const alx::signal<uint_64, const std::string&>* on_csys = nullptr;

            const engine_config* m_cfg = nullptr;

            walk_state()
                : root(nullptr), current(nullptr) {}
            ~walk_state() { clear(); }
            void clear() {
                frames.clear();
                current = nullptr;
                root = nullptr;
                root_entity = nullptr;
                clear_pos();
            }

            src_pos& pos_slot() { return frames.empty() ? top_pos : frames.back().pos; }
            const src_pos& pos_slot() const { return frames.empty() ? top_pos : frames.back().pos; }

            void clear_pos() { top_pos = src_pos(); }

            scope_frame& push_frame(impl_import* _ent = nullptr, uint_8 _flags = FF_NONE) {
                if (m_cfg && m_cfg->max_stack > 0 && frames.size() >= m_cfg->max_stack)
                    throw script_exception{error_type::StackError, std::string("stack overflow")};
                frames.emplace_back(_ent ? _ent : current);
                frames.back().flags = _flags;
                return frames.back();
            }
            // popping the last frame leaves current on the popped entity until the caller restores it
            void pop_frame() {
                if (!frames.empty()) {

                    uint_8 f = frames.back().flags;
                    if (f & FF_RET) ret_flag = false;
                    if (f & FF_BREAK) break_flag = false;
                    if (f & FF_CONT) cont_flag = false;
                    if (f & FF_TAIL) tail_flag = false;
                    frames.pop_back();
                    if (!frames.empty()) current = frames.back().ent;
                }
            }

            // only frames of the current entity are searched (entity barrier); a hit in an outer frame is cached into the top frame
            variant* var_ptr(const std::string& _name) {

                size_t depth = 0;
                for (auto it = frames.rbegin(); it != frames.rend(); ++it, ++depth) {
                    if (it->ent != current) break;
                    size_t idx;
                    variant* p = it->find(_name, &idx);
                    if (p) {
                        if (depth > 0) frames.back().var_map.set(_name, idx);
                        return p;
                    }
                }

                if (!root) {

                    return nullptr;
                }
                if (current) return current->m_store.find(_name);
                return nullptr;
            }
        };

    }
}

#endif
