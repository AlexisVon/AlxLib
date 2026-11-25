/*****************************************************************/ /**
 * \file   amap.h
 * \brief  Ordered B-tree map, two-zone hash map and read-optimized sparse hash map:
 *         map_btree / map_hash / map_read
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_MAP_H_
#define _ALEXIS_MAP_H_

#include "abase.h"
#include "ahash.h"

#include <cstddef>
#include <type_traits>
#include <utility>

#if defined(_MSC_VER)
#    define ALX_AMAP_INL __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#    define ALX_AMAP_INL inline __attribute__((always_inline))
#else
#    define ALX_AMAP_INL inline
#endif

namespace alx {

    /**
     * \brief Ordered map on a high-fanout B-tree
     *
     * A subset of the std::map interface, not a drop-in replacement. insert() gives
     * insert_or_assign semantics: an existing key is overwritten and an insert never fails.
     * value_type keeps the key non-const, and the caller must not modify it once inserted.
     * Inserting a new entry or erasing shifts entries inside a node and moves them between
     * nodes on a split or merge, invalidating iterators and any reference or pointer into an
     * entry; overwriting a known key leaves the entries in place (an owning map deletes the
     * replaced pointee). Key and mapped type must be default constructible, copyable and
     * movable; Key is ordered by operator<. No internal synchronization.
     *
     * IsOwnPtr: when true, Value must be a pointer and the map owns the pointee -- erase,
     * clear and destruction delete it, a copy of the map deep-copies it (the pointee must be
     * copy-constructible; a null pointer stays null), insert/emplace delete the pointer an
     * overwritten key held, try_emplace leaves a known key entirely alone, and take() removes
     * an entry while handing its pointer to the caller undeleted. Whether insert clones or adopts follows the value
     * argument's category: an lvalue Value or entry is copied (an owning map clones the pointee and
     * the caller keeps its own), an rvalue one is adopted and its source pointer is left null. The
     * Value& operator[] returns is the caller's to manage: assigning a pointer through it does
     * not delete the old pointee.
     */
    template <typename Key, typename Value, bool IsOwnPtr = false>
    class map_btree {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<Key, Value>;
        using size_type = size_t;

    private:
        static_assert(!IsOwnPtr || std::is_pointer<Value>::value, "map_btree: IsOwnPtr requires a pointer value type");
        using pointee_type = typename std::remove_pointer<Value>::type;
        /// Node size the fanout aims at; measured against std::map, 512 B beats 256 B on wide slots
        static constexpr size_t NODE_TARGET = 512;
        /// The three head fields, padded up to the alignment of an entry
        static constexpr size_t NODE_HEAD = (sizeof(void*) + sizeof(uint_16) + sizeof(bool) + alignof(value_type) - 1) / alignof(value_type) * alignof(value_type);
        /// Entries that fit the target, leaving room for the one transient overflow entry
        static constexpr size_t NODE_FIT = NODE_TARGET > NODE_HEAD + sizeof(value_type) + 2 * sizeof(void*) ? (NODE_TARGET - NODE_HEAD - sizeof(value_type) - 2 * sizeof(void*)) / (sizeof(value_type) + sizeof(void*)) : 0;
        /// Stable capacity of a node; the arrays carry one more entry and one more child
        static constexpr size_t CAPACITY = NODE_FIT < 4 ? 4 : NODE_FIT;
        /// Entries a non-root node keeps at least, so a merge never overflows
        static constexpr uint_16 MIN = (uint_16) (CAPACITY / 2);

        struct node {
            node* parent;
            uint_16 n;
            bool leaf;
            value_type e[CAPACITY + 1];
            node* child[CAPACITY + 2];

            node(node* _parent, bool _leaf) : parent(_parent), n(0), leaf(_leaf), e(), child() {}
        };

        /// Release the value stored in a slot; a plain map has nothing to release
        static void drop_value(Value&, std::false_type) {}
        static void drop_value(Value& _v, std::true_type) { delete _v; }
        static void drop_value(Value& _v) { drop_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// The value moved out of a slot that keeps no claim on it
        static void detach_value(Value&, std::false_type) {}
        static void detach_value(Value& _v, std::true_type) { _v = nullptr; }
        static void detach_value(Value& _v) { detach_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy an entry into a fresh slot; an owning map clones the pointee
        static void copy_entry(value_type& _slot, const value_type& _v, std::false_type) { _slot = _v; }
        static void copy_entry(value_type& _slot, const value_type& _v, std::true_type) {
            _slot.first = _v.first;
            _slot.second = _v.second ? new pointee_type(*_v.second) : nullptr;
        }
        static void copy_entry(value_type& _slot, const value_type& _v) { copy_entry(_slot, _v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy a value; an owning map clones the pointee
        static Value copy_value(const Value& _v, std::false_type) { return _v; }
        static Value copy_value(const Value& _v, std::true_type) { return _v ? new pointee_type(*_v) : nullptr; }
        static Value copy_value(const Value& _v) { return copy_value(_v, std::integral_constant<bool, IsOwnPtr>()); }

    public:
        class const_iterator;

        /// Mutable iterator over the entries, in key order; {nullptr, 0} is the end
        class iterator {
            friend class map_btree;
            friend class const_iterator;

            node* m_node;
            uint_16 m_idx;

        public:
            using value_type = typename map_btree::value_type;

            /// Wrap a node position
            iterator(node* _node, uint_16 _idx) : m_node(_node), m_idx(_idx) {}
            /// The current entry; its key must not be modified
            value_type& operator*() const { return m_node->e[m_idx]; }
            /// Pointer to the current entry
            value_type* operator->() const { return &m_node->e[m_idx]; }
            /// Advance to the next entry, in key order
            iterator& operator++() {
                m_node = const_cast<node*>(successor(m_node, m_idx));
                return *this;
            }
            /// Advance, returning a copy of the previous position
            iterator operator++(int) {
                iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const iterator& _it) const { return m_node == _it.m_node && m_idx == _it.m_idx; }
            /// Compare two positions
            bool operator!=(const iterator& _it) const { return !(*this == _it); }
        };

        /// Const iterator over the entries, in key order; {nullptr, 0} is the end
        class const_iterator {
            friend class map_btree;

            const node* m_node;
            uint_16 m_idx;

        public:
            using value_type = typename map_btree::value_type;

            /// Wrap a node position
            const_iterator(const node* _node, uint_16 _idx) : m_node(_node), m_idx(_idx) {}
            /// Convert a mutable iterator
            const_iterator(const iterator& _it) : m_node(_it.m_node), m_idx(_it.m_idx) {}
            /// The current entry
            const value_type& operator*() const { return m_node->e[m_idx]; }
            /// Pointer to the current entry
            const value_type* operator->() const { return &m_node->e[m_idx]; }
            /// Advance to the next entry, in key order
            const_iterator& operator++() {
                m_node = successor(m_node, m_idx);
                return *this;
            }
            /// Advance, returning a copy of the previous position
            const_iterator operator++(int) {
                const_iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const const_iterator& _it) const { return m_node == _it.m_node && m_idx == _it.m_idx; }
            /// Compare two positions
            bool operator!=(const const_iterator& _it) const { return !(*this == _it); }
        };

    public:
        /// Construct an empty map
        map_btree() noexcept = default;
        /// Copy; the sorted entries are appended in one pass, O(n)
        map_btree(const map_btree& _v) {
            try {
                node* leaf = nullptr;
                for (const_iterator it = _v.cbegin(); it != _v.cend(); ++it) leaf = append_entry(leaf, *it);
            } catch (...) {
                clear();
                throw;
            }
        }
        /// Take over the entries of _v, which is left empty
        map_btree(map_btree&& _v) noexcept {
            m_root = _v.m_root;
            m_size = _v.m_size;
            _v.m_root = nullptr;
            _v.m_size = 0;
        }
        /// Copy; the previous contents are released first
        map_btree& operator=(const map_btree& _v) {
            if (this != &_v) {
                map_btree temp(_v);
                swap(temp);
            }
            return *this;
        }
        /// Take over the entries of _v, which is left empty
        map_btree& operator=(map_btree&& _v) noexcept {
            if (this != &_v) {
                clear();
                m_root = _v.m_root;
                m_size = _v.m_size;
                _v.m_root = nullptr;
                _v.m_size = 0;
            }
            return *this;
        }
        /// Release every node
        ~map_btree() { clear(); }

    public:
        /// Number of entries
        size_type size() const noexcept { return m_size; }
        /// True while the map holds no entry
        bool empty() const noexcept { return 0 == m_size; }
        /// Drop every entry
        void clear() {
            destroy(m_root);
            m_root = nullptr;
            m_size = 0;
        }
        /// Exchange the contents with _v
        void swap(map_btree& _v) noexcept {
            std::swap(m_root, _v.m_root);
            std::swap(m_size, _v.m_size);
        }

    public:
        /// Look up _key, yielding end() when it is absent
        iterator find(const Key& _key) {
            uint_16 idx = 0;
            node* c = locate_node<node>(m_root, _key, idx);
            return c ? iterator(c, idx) : end();
        }
        /// Look up _key, yielding cend() when it is absent
        const_iterator find(const Key& _key) const {
            uint_16 idx = 0;
            const node* c = locate_node<const node>(m_root, _key, idx);
            return c ? const_iterator(c, idx) : cend();
        }
        /// True when _key is present
        bool contains(const Key& _key) const { return find(_key) != cend(); }
        /// 1 when _key is present, 0 otherwise
        size_type count(const Key& _key) const { return find(_key) == cend() ? 0 : 1; }
        /// The value stored under _key, or the shared default when it is absent; nothing is inserted
        const Value& value(const Key& _key, const Value& _def = def_value()) const {
            const_iterator it = find(_key);
            return it == cend() ? _def : it->second;
        }
        /// The shared default value() answers, so a reference kept from an absent key stays valid
        static const Value& def_value() {
            static const Value s_val = Value();
            return s_val;
        }
        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL const Value* peek(const Key& _key) const {
            uint_16 idx = 0;
            const node* c = locate_node<const node>(m_root, _key, idx);
            return c ? &c->e[idx].second : nullptr;
        }
        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL Value* peek(const Key& _key) {
            uint_16 idx = 0;
            node* c = locate_node<node>(m_root, _key, idx);
            return c ? &c->e[idx].second : nullptr;
        }

        /// Insert a copy of _val under _key; an owning map clones the pointee
        iterator insert(const Key& _key, const Value& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { Value fresh = copy_value(_val); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_key, copy_value(_val)); })
                .first;
        }
        /// Insert by moving the value under a copied key; the source value is left null
        iterator insert(const Key& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(_key, std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert by moving key and value; the source value is left null
        iterator insert(Key&& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(std::move(_key), std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert a copy of an entry; an owning map clones the pointee
        iterator insert(const value_type& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { Value fresh = copy_value(_v.second); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_v.first, copy_value(_v.second)); })
                .first;
        }
        /// Insert by moving an entry; the source pointer is left null (do not move an entry out of a map)
        iterator insert(value_type&& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_v.second); detach_value(_v.second); },
                       [&](value_type& _slot) { _slot = std::move(_v); detach_value(_v.second); })
                .first;
        }
        /// Construct an entry from _args and insert it; a known key still pays the construction
        template <typename... Args>
        iterator emplace(Args&&... _args) {
            value_type v(std::forward<Args>(_args)...);
            return insert_impl(
                       v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(v.second); },
                       [&](value_type& _slot) { _slot = std::move(v); })
                .first;
        }
        /// Insert a value built from _args only when _key is absent; a known key neither constructs nor moves them
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(const Key& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(_key, Value(std::forward<Args>(_args)...)); });
        }
        /// As try_emplace(), moving the key on the insert path only
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(Key&& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(std::move(_key), Value(std::forward<Args>(_args)...)); });
        }
        /// The value stored under _key, default-constructing and inserting one when absent
        Value& operator[](const Key& _key) {
            iterator it = find(_key);
            if (it == end()) it = insert(_key, Value());
            return it->second;
        }

    public:
        /// Erase the entry of _key, returning 1 when it was there
        size_type erase(const Key& _key) {
            iterator it = find(_key);
            if (it == end()) return 0;
            erase_at(it.m_node, it.m_idx);
            return 1;
        }
        /// Erase the entry _it points at, returning its successor
        iterator erase(iterator _it) {
            iterator next = _it;
            ++next;
            const bool has_next = next != end();
            Key next_key;
            if (has_next) next_key = next->first;
            erase_at(_it.m_node, _it.m_idx);
            return has_next ? find(next_key) : end();
        }
        /**
         * \brief Remove the entry of _key, handing its value over without releasing it
         *
         * An owning map transfers the pointer to the caller; a plain map returns a copy. The
         * answer is Value() when the key is absent -- under IsOwnPtr that is a null pointer,
         * indistinguishable from a stored null, so ask contains() first when it matters.
         */
        Value take(const Key& _key) {
            iterator it = find(_key);
            if (it == end()) return Value();
            Value out = it->second;
            detach_value(it->second);
            erase_at(it.m_node, it.m_idx);
            return out;
        }

    public:
        /// Iterator to the first entry, in key order
        iterator begin() noexcept {
            if (!m_root) return end();
            return iterator(const_cast<node*>(leftmost_leaf(m_root)), 0);
        }
        /// One past the last entry
        iterator end() noexcept { return iterator(nullptr, 0); }
        /// Const iterator to the first entry, in key order
        const_iterator begin() const noexcept {
            if (!m_root) return cend();
            return const_iterator(leftmost_leaf(m_root), 0);
        }
        /// Const one past the last entry
        const_iterator end() const noexcept { return const_iterator(nullptr, 0); }
        /// Const iterator to the first entry, in key order
        const_iterator cbegin() const noexcept { return begin(); }
        /// Const one past the last entry
        const_iterator cend() const noexcept { return end(); }

    private:
        /// First position in _n whose key is not less than _key; the shared step of find() and insert()
        /// The node and index holding _key, or nullptr when it is absent
        template <typename Node>
        ALX_AMAP_INL static Node* locate_node(Node* _root, const Key& _key, uint_16& _idx) {
            Node* c = _root;
            while (c) {
                const uint_16 i = node_lower_bound(c, _key);
                if (i < c->n && !(_key < c->e[i].first)) {
                    _idx = i;
                    return c;
                }
                if (c->leaf) break;
                c = c->child[i];
            }
            return nullptr;
        }

        static uint_16 node_lower_bound(const node* _n, const Key& _key) {
            uint_16 i = 0;
            uint_16 j = _n->n;
            while (i < j) {
                const uint_16 mid = (uint_16) ((i + j) / 2);
                if (_n->e[mid].first < _key) i = (uint_16) (mid + 1);
                else j = mid;
            }
            return i;
        }

        /**
         * \brief The entry after the one at (_c, _idx), advancing _idx
         *
         * Same node, then the leftmost entry of the right subtree, then up the parent chain;
         * nullptr is the end. Shared by both iterators.
         */
        static const node* successor(const node* _c, uint_16& _idx) {
            if (!_c->leaf) {
                const node* d = leftmost_leaf(_c->child[_idx + 1]);
                _idx = 0;
                return d;
            }
            if (_idx + 1 < _c->n) {
                ++_idx;
                return _c;
            }
            const node* c = _c;
            while (c->parent) {
                const node* p = c->parent;
                uint_16 k = 0;
                while (p->child[k] != c) ++k;
                if (k < p->n) {
                    _idx = k;
                    return p;
                }
                c = p;
            }
            _idx = 0;
            return nullptr;
        }

        /// Position of _c in the child array of its parent
        static uint_16 child_index(const node* _p, const node* _c) {
            uint_16 k = 0;
            while (_p->child[k] != _c) ++k;
            return k;
        }

        /// Descend to the leaf that holds the first key
        static const node* leftmost_leaf(const node* _n) {
            while (!_n->leaf) _n = _n->child[0];
            return _n;
        }

        /// Descend to the leaf that holds the last key
        static node* rightmost_leaf(node* _n) {
            while (!_n->leaf) _n = _n->child[_n->n];
            return _n;
        }

        /// Release _n and every node below it
        static void destroy(node* _n) {
            if (!_n) return;
            for (uint_16 k = 0; k < _n->n; ++k) drop_value(_n->e[k].second);
            if (!_n->leaf) {
                for (uint_16 k = 0; k <= _n->n; ++k) destroy(_n->child[k]);
            }
            delete _n;
        }

        /**
         * \brief Descend by _key, then _assign the stored value or _place a fresh entry
         *
         * A known key only touches the stored value, so the insert(key, value) forms never
         * copy or move the key on that path. The second member of the answer says whether the
         * entry was created; the fresh entry is tracked through the splits so the returned
         * iterator stays valid.
         */
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_impl(const Key& _key, Assign _assign, Place _place) {
            if (!m_root) {
                m_root = new node(nullptr, true);
                _place(m_root->e[0]);
                m_root->n = 1;
                ++m_size;
                return {iterator(m_root, 0), true};
            }

            node* c = m_root;
            uint_16 i = 0;
            while (true) {
                i = node_lower_bound(c, _key);
                if (i < c->n && !(_key < c->e[i].first)) {
                    _assign(c->e[i].second);
                    return {iterator(c, i), false};
                }
                if (c->leaf) break;
                c = c->child[i];
            }

            for (uint_16 k = c->n; k > i; --k) c->e[k] = std::move(c->e[k - 1]);
            _place(c->e[i]);
            ++c->n;
            ++m_size;

            node* tracked = c;
            uint_16 tracked_idx = i;
            while (c->n > CAPACITY) {
                node* right = nullptr;
                uint_16 mid_pos = 0;
                const uint_16 mid = (uint_16) (CAPACITY / 2);
                node* p = split(c, right, mid_pos);
                if (tracked == c) {
                    if (tracked_idx == mid) {
                        tracked = p;
                        tracked_idx = mid_pos;
                    } else if (tracked_idx > mid) {
                        tracked = right;
                        tracked_idx = (uint_16) (tracked_idx - mid - 1);
                    }
                }
                c = p;
            }
            return {iterator(tracked, tracked_idx), true};
        }

        /**
         * \brief Split the overfull node _c, moving its median into the parent
         *
         * The upper half becomes a fresh right sibling returned through _right; the median
         * lands at _mid_pos of the returned parent, which is a fresh root when _c was the root.
         */
        node* split(node* _c, node*& _right, uint_16& _mid_pos) {
            const uint_16 mid = (uint_16) (CAPACITY / 2);
            _right = new node(_c->parent, _c->leaf);
            for (uint_16 k = mid + 1; k <= CAPACITY; ++k) _right->e[k - mid - 1] = std::move(_c->e[k]);
            _right->n = (uint_16) (CAPACITY - mid);
            if (!_c->leaf) {
                for (uint_16 k = mid + 1; k <= CAPACITY + 1; ++k) {
                    _right->child[k - mid - 1] = _c->child[k];
                    _right->child[k - mid - 1]->parent = _right;
                }
            }
            _c->n = mid;

            node* p = _c->parent;
            if (!p) {
                p = new node(nullptr, false);
                p->e[0] = std::move(_c->e[mid]);
                p->n = 1;
                p->child[0] = _c;
                p->child[1] = _right;
                _c->parent = p;
                _right->parent = p;
                m_root = p;
                _mid_pos = 0;
                return p;
            }

            const uint_16 pos = child_index(p, _c);
            for (uint_16 k = p->n; k > pos; --k) p->e[k] = std::move(p->e[k - 1]);
            for (uint_16 k = p->n + 1; k > pos + 1; --k) p->child[k] = p->child[k - 1];
            p->e[pos] = std::move(_c->e[mid]);
            p->child[pos + 1] = _right;
            _right->parent = p;
            ++p->n;
            _mid_pos = pos;
            return p;
        }

        /// Erase the entry at (_c, _i); an internal entry is replaced by its predecessor first
        void erase_at(node* _c, uint_16 _i) {
            if (!_c->leaf) {
                node* t = rightmost_leaf(_c->child[_i]);
                drop_value(_c->e[_i].second);
                _c->e[_i] = std::move(t->e[t->n - 1]);
                detach_value(t->e[t->n - 1].second);
                _c = t;
                _i = (uint_16) (t->n - 1);
            }
            drop_value(_c->e[_i].second);
            for (uint_16 k = _i; k + 1 < _c->n; ++k) _c->e[k] = std::move(_c->e[k + 1]);
            --_c->n;
            --m_size;
            rebalance(_c);
        }

        /// Restore the fill of _c after an erasure; collapses an emptied root
        void rebalance(node* _c) {
            while (_c != m_root && _c->n < MIN) {
                node* p = _c->parent;
                const uint_16 pos = child_index(p, _c);
                if (pos > 0 && p->child[pos - 1]->n > MIN) {
                    borrow_left(p, pos);
                    return;
                }
                if (pos < p->n && p->child[pos + 1]->n > MIN) {
                    borrow_right(p, pos);
                    return;
                }
                if (pos > 0) merge(p, (uint_16) (pos - 1));
                else merge(p, pos);
                _c = p;
            }
            if (0 == m_root->n) {
                if (m_root->leaf) {
                    delete m_root;
                    m_root = nullptr;
                } else {
                    node* old = m_root;
                    m_root = old->child[0];
                    m_root->parent = nullptr;
                    delete old;
                }
            }
        }

        /// Move one entry from the left sibling of child _pos through the parent
        void borrow_left(node* _p, uint_16 _pos) {
            node* c = _p->child[_pos];
            node* l = _p->child[_pos - 1];
            for (uint_16 k = c->n; k > 0; --k) c->e[k] = std::move(c->e[k - 1]);
            if (!c->leaf) {
                for (uint_16 k = c->n + 1; k > 0; --k) c->child[k] = c->child[k - 1];
                c->child[0] = l->child[l->n];
                c->child[0]->parent = c;
            }
            c->e[0] = std::move(_p->e[_pos - 1]);
            _p->e[_pos - 1] = std::move(l->e[l->n - 1]);
            ++c->n;
            --l->n;
        }

        /// Move one entry from the right sibling of child _pos through the parent
        void borrow_right(node* _p, uint_16 _pos) {
            node* c = _p->child[_pos];
            node* r = _p->child[_pos + 1];
            c->e[c->n] = std::move(_p->e[_pos]);
            _p->e[_pos] = std::move(r->e[0]);
            for (uint_16 k = 0; k + 1 < r->n; ++k) r->e[k] = std::move(r->e[k + 1]);
            if (!r->leaf) {
                c->child[c->n + 1] = r->child[0];
                c->child[c->n + 1]->parent = c;
                for (uint_16 k = 0; k < r->n; ++k) r->child[k] = r->child[k + 1];
            }
            ++c->n;
            --r->n;
        }

        /// Merge children _pos and _pos+1 of _p around the separator _p->e[_pos]
        void merge(node* _p, uint_16 _pos) {
            node* l = _p->child[_pos];
            node* r = _p->child[_pos + 1];
            l->e[l->n] = std::move(_p->e[_pos]);
            for (uint_16 k = 0; k < r->n; ++k) l->e[l->n + 1 + k] = std::move(r->e[k]);
            if (!l->leaf) {
                for (uint_16 k = 0; k <= r->n; ++k) {
                    l->child[l->n + 1 + k] = r->child[k];
                    r->child[k]->parent = l;
                }
            }
            l->n = (uint_16) (l->n + 1 + r->n);
            for (uint_16 k = _pos; k + 1 < _p->n; ++k) _p->e[k] = std::move(_p->e[k + 1]);
            for (uint_16 k = _pos + 1; k < _p->n; ++k) _p->child[k] = _p->child[k + 1];
            --_p->n;
            delete r;
        }

        /**
         * \brief Append an entry greater than every stored key
         *
         * The copy constructor feeds the source in key order through this, so the descent
         * runs once per node rather than once per entry; _leaf caches the rightmost leaf.
         */
        node* append_entry(node* _leaf, const value_type& _v) {
            node* c = _leaf;
            if (!c) {
                if (!m_root) m_root = new node(nullptr, true);
                c = rightmost_leaf(m_root);
                _leaf = c;
            }
            copy_entry(c->e[c->n], _v);
            ++c->n;
            ++m_size;
            while (c->n > CAPACITY) {
                node* right = nullptr;
                uint_16 mid_pos = 0;
                node* p = split(c, right, mid_pos);
                if (c->leaf) _leaf = right;
                c = p;
            }
            return _leaf;
        }

    private:
        node* m_root = nullptr;
        size_type m_size = 0;
    };

    /**
     * \brief Unordered map: a linear block, then four-slot buckets, then two zone arrays
     *
     * A subset of the std::unordered_map interface, not a drop-in replacement. A map starts as
     * four slots scanned in order with no hash; the fifth entry hands over to a bucket table
     * where a hash picks a bucket of four slots, and a full bucket doubles the table. Past a
     * bucket cap the table becomes two zone arrays: the low hash half picks an L0 slot, an
     * occupied home marks the slot demoted and moves the entry to the L1 slot picked by the
     * high half, then to that slot's chain of at most four entries. The linear block, a bucket
     * and a chain each bound a scan to four key comparisons. insert() gives insert_or_assign
     * semantics: an existing key is overwritten and an insert never fails. value_type keeps the
     * key non-const, and the caller must not modify it once inserted. Any insert or erase
     * invalidates iterators and references into entries; a pointer Value's pointee is stable.
     * The iteration order is an implementation detail. Key must be default constructible,
     * copyable, movable and comparable with operator==; a pointer key is stored, compared and
     * hashed by address, so a const char* key is almost never what is meant -- use std::string;
     * a floating-point NaN key never equals itself and cannot be found. Hash must answer
     * hash::func<Hash, uint_64> and keep its low and high 32-bit halves independent. No internal
     * synchronization.
     *
     * IsOwnPtr: when true, Value must be a pointer and the map owns the pointee, under the same
     * contract as map_btree's.
     */
    template <typename Key, typename Value, bool IsOwnPtr = false, typename Hash = hash::auto_64>
    class map_hash {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<Key, Value>;
        using size_type = size_t;

    private:
        static_assert(!IsOwnPtr || std::is_pointer<Value>::value, "map_hash: IsOwnPtr requires a pointer value type");
        using pointee_type = typename std::remove_pointer<Value>::type;
        using hasher = hash::func<Hash, uint_64>;

        /// Slots in the linear block and in a bucket; both bound a scan to four key comparisons
        static constexpr size_t LINE_SLOTS = 4;
        static constexpr size_t BUCKET_SLOTS = 4;
        /// Entries in a chain block; a full chain forces a rehash, so a live chain is one block
        static constexpr size_t CHAIN_BLOCK = 4;
        /// A bucket table stops doubling here and hands over to the two-zone arrays
        static constexpr size_t BUCKET_MAX = 16;
        /// Smallest slot pair, built on the first insertion or the first reserve
        static constexpr size_t MIN_L0 = 11;
        static constexpr size_t MIN_L1 = 7;
        /// Per-slot flags: occupied, and demoted (entries homed here also live in L1 or a chain)
        static constexpr uint_8 SLOT_USED = 1;
        static constexpr uint_8 SLOT_DEMOTED = 2;
        /// Bits 2-7 of a slot's flag byte hold a six-bit fragment of its occupant's hash
        static constexpr uint_8 SLOT_FRAG = 0xFC;
        /// Iterator zones: the linear block, the bucket slab, the L0 slots, the L1 slots, a
        /// chained block, and the end
        static constexpr uint_8 ZONE_LINE = 0;
        static constexpr uint_8 ZONE_BUCKET = 1;
        static constexpr uint_8 ZONE_L0 = 2;
        static constexpr uint_8 ZONE_L1 = 3;
        static constexpr uint_8 ZONE_CHAIN = 4;
        static constexpr uint_8 ZONE_END = 5;

        /// Storage shapes, in promotion order
        enum mode_t : uint_8 { MODE_LAZY = 0,
                               MODE_LINE,
                               MODE_BUCKET,
                               MODE_ZONE };

        mode_t mode() const {
            if (!m_line) return MODE_LAZY;
            if (0 == m_p) return MODE_LINE;
            return m_q ? MODE_ZONE : MODE_BUCKET;
        }

        /// A chain block; next links recycled blocks only
        struct block {
            block* next;
            uint_16 count;
            value_type e[CHAIN_BLOCK];
        };

        /// Two 32-bit moduli through precomputed reciprocals: one correction suffices
        struct barrett {
            uint_64 m0;
            uint_64 m1;
            uint_32 p0;
            uint_32 p1;

            barrett() : m0(0), m1(0), p0(1), p1(1) {}
            barrett(uint_32 _p0, uint_32 _p1)
                : m0((uint_64(1) << 32) / _p0), m1((uint_64(1) << 32) / _p1), p0(_p0), p1(_p1) {}

            uint_32 mod0(uint_32 _x) const {
                const uint_32 q = (uint_32) (((uint_64) _x * m0) >> 32);
                const uint_32 r = _x - q * p0;
                return r >= p0 ? (uint_32) (r - p0) : r;
            }
            uint_32 mod1(uint_32 _x) const {
                const uint_32 q = (uint_32) (((uint_64) _x * m1) >> 32);
                const uint_32 r = _x - q * p1;
                return r >= p1 ? (uint_32) (r - p1) : r;
            }
        };

        /// Release the value stored in a slot; a plain map has nothing to release
        static void drop_value(Value&, std::false_type) {}
        static void drop_value(Value& _v, std::true_type) { delete _v; }
        static void drop_value(Value& _v) { drop_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// The value moved out of a slot that keeps no claim on it
        static void detach_value(Value&, std::false_type) {}
        static void detach_value(Value& _v, std::true_type) { _v = nullptr; }
        static void detach_value(Value& _v) { detach_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy an entry into a fresh slot; an owning map clones the pointee
        static void copy_entry(value_type& _slot, const value_type& _v, std::false_type) { _slot = _v; }
        static void copy_entry(value_type& _slot, const value_type& _v, std::true_type) {
            _slot.first = _v.first;
            _slot.second = _v.second ? new pointee_type(*_v.second) : nullptr;
        }
        static void copy_entry(value_type& _slot, const value_type& _v) { copy_entry(_slot, _v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy a value; an owning map clones the pointee
        static Value copy_value(const Value& _v, std::false_type) { return _v; }
        static Value copy_value(const Value& _v, std::true_type) { return _v ? new pointee_type(*_v) : nullptr; }
        static Value copy_value(const Value& _v) { return copy_value(_v, std::integral_constant<bool, IsOwnPtr>()); }

    public:
        class const_iterator;

        /// Mutable iterator; any insert or erase may invalidate it
        class iterator {
            friend class map_hash;
            friend class const_iterator;

        public:
            using value_type = typename map_hash::value_type;

        private:
            map_hash* m_map;
            uint_8 m_zone;
            size_t m_idx;
            block* m_blk;
            uint_16 m_bidx;

            value_type& entry() const {
                if (ZONE_CHAIN == m_zone) return m_blk->e[m_bidx];
                if (ZONE_LINE == m_zone) return m_map->m_line[m_idx];
                if (ZONE_BUCKET == m_zone) return m_map->m_l0[m_idx];
                return ZONE_L0 == m_zone ? m_map->m_l0[m_idx] : m_map->m_l1[m_idx];
            }

        public:
            /// Wrap a position
            iterator(map_hash* _map, uint_8 _zone, size_t _idx, block* _blk, uint_16 _bidx)
                : m_map(_map), m_zone(_zone), m_idx(_idx), m_blk(_blk), m_bidx(_bidx) {}
            /// The current entry; its key must not be modified
            value_type& operator*() const { return entry(); }
            /// Pointer to the current entry
            value_type* operator->() const { return &entry(); }
            /// Advance to the next entry in iteration order
            iterator& operator++() {
                step(m_map, m_zone, m_idx, m_blk, m_bidx);
                return *this;
            }
            /// Advance, returning a copy of the previous position
            iterator operator++(int) {
                iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const iterator& _it) const { return m_zone == _it.m_zone && m_idx == _it.m_idx && m_blk == _it.m_blk && m_bidx == _it.m_bidx; }
            /// Compare two positions
            bool operator!=(const iterator& _it) const { return !(*this == _it); }
        };

        /// Const iterator over the entries, in iteration order
        class const_iterator {
            friend class map_hash;

        public:
            using value_type = typename map_hash::value_type;

        private:
            const map_hash* m_map;
            uint_8 m_zone;
            size_t m_idx;
            block* m_blk;
            uint_16 m_bidx;

            const value_type& entry() const {
                if (ZONE_CHAIN == m_zone) return m_blk->e[m_bidx];
                if (ZONE_LINE == m_zone) return m_map->m_line[m_idx];
                if (ZONE_BUCKET == m_zone) return m_map->m_l0[m_idx];
                return ZONE_L0 == m_zone ? m_map->m_l0[m_idx] : m_map->m_l1[m_idx];
            }

        public:
            /// Wrap a position
            const_iterator(const map_hash* _map, uint_8 _zone, size_t _idx, block* _blk, uint_16 _bidx)
                : m_map(_map), m_zone(_zone), m_idx(_idx), m_blk(_blk), m_bidx(_bidx) {}
            /// Convert a mutable iterator
            const_iterator(const iterator& _it) : m_map(_it.m_map), m_zone(_it.m_zone), m_idx(_it.m_idx), m_blk(_it.m_blk), m_bidx(_it.m_bidx) {}
            /// The current entry
            const value_type& operator*() const { return entry(); }
            /// Pointer to the current entry
            const value_type* operator->() const { return &entry(); }
            /// Advance to the next entry in iteration order
            const_iterator& operator++() {
                step(m_map, m_zone, m_idx, m_blk, m_bidx);
                return *this;
            }
            /// Advance, returning a copy of the previous position
            const_iterator operator++(int) {
                const_iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const const_iterator& _it) const { return m_zone == _it.m_zone && m_idx == _it.m_idx && m_blk == _it.m_blk && m_bidx == _it.m_bidx; }
            /// Compare two positions
            bool operator!=(const const_iterator& _it) const { return !(*this == _it); }
        };

    public:
        /// Construct an empty map; the slot arrays are built on the first insertion
        map_hash() noexcept = default;
        /// Copy; the entries are cloned position by position in one pass
        map_hash(const map_hash& _v) {
            try {
                clone(_v);
            } catch (...) {
                destroy();
                throw;
            }
        }
        /// Take over the entries of _v, which is left empty
        map_hash(map_hash&& _v) noexcept { steal(_v); }
        /// Copy; the previous contents are released first
        map_hash& operator=(const map_hash& _v) {
            if (this != &_v) {
                map_hash temp(_v);
                swap(temp);
            }
            return *this;
        }
        /// Take over the entries of _v, which is left empty
        map_hash& operator=(map_hash&& _v) noexcept {
            if (this != &_v) {
                destroy();
                steal(_v);
            }
            return *this;
        }
        /// Release every entry
        ~map_hash() { destroy(); }

    public:
        /// Number of entries
        size_type size() const noexcept { return m_size; }
        /// True while the map holds no entry
        bool empty() const noexcept { return 0 == m_size; }
        /// Drop every entry; the linear block stays, so a refilling frame pays no allocation
        void clear() { shed(); }
        /// Exchange the contents with _v
        void swap(map_hash& _v) noexcept {
            std::swap(m_line, _v.m_line);
            std::swap(m_l0, _v.m_l0);
            std::swap(m_l1, _v.m_l1);
            std::swap(m_f0, _v.m_f0);
            std::swap(m_f1, _v.m_f1);
            std::swap(m_chain, _v.m_chain);
            std::swap(m_free, _v.m_free);
            std::swap(m_p, _v.m_p);
            std::swap(m_q, _v.m_q);
            std::swap(m_l0_n, _v.m_l0_n);
            std::swap(m_chain_n, _v.m_chain_n);
            std::swap(m_size, _v.m_size);
            std::swap(m_b, _v.m_b);
        }
        /// Build enough storage that _n insertions do not trigger a rehash; beyond the linear
        /// block this is the two-zone shape, whose geometry is the only one that can promise it
        void reserve(size_type _n) {
            if (0 == _n) return;
            if (!m_line) m_line = new value_type[LINE_SLOTS];
            if (_n <= LINE_SLOTS) return;
            size_t np = 0;
            size_t nq = 0;
            const size_t target = geometry_for(_n, np, nq);
            if (MODE_ZONE == mode()) {
                if ((size_t) m_p + m_q >= target) return;
                rehash_zone(np, nq);
                return;
            }
            if (MODE_BUCKET == mode()) bucket_to_zone(np, nq);
            else line_to_zone(np, nq);
        }

    public:
        /// Look up _key, yielding end() when it is absent
        iterator find(const Key& _key) {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (locate(_key, zone, idx, blk, bidx)) return iterator(this, zone, idx, blk, bidx);
            return end();
        }
        /// Look up _key, yielding cend() when it is absent
        const_iterator find(const Key& _key) const {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (locate(_key, zone, idx, blk, bidx)) return const_iterator(this, zone, idx, blk, bidx);
            return cend();
        }
        /// True only when _key is certainly absent; false means "not proven absent", never "present",
        /// so ask contains() when the answer matters. The linear block compares keys; the bucket
        /// and the two-zone shape answer from slot flags and fragments, with no key comparison.
        bool cannot_contain(const Key& _key) const {
            switch (mode()) {
            case MODE_LAZY:
                return true;
            case MODE_LINE:
                for (size_t i = 0; i < m_size; ++i) {
                    if (m_line[i].first == _key) return false;
                }
                return true;
            case MODE_BUCKET: {
                const uint_64 h = key_hash(_key);
                const uint_8 frag_h = frag(h);
                const size_t base = bucket_base(h);
                for (size_t k = 0; k < BUCKET_SLOTS; ++k) {
                    const uint_8 f = m_f0[base + k];
                    if ((f & SLOT_USED) && (f & SLOT_FRAG) == frag_h) return false;
                }
                return true;
            }
            default: {
                const uint_64 h = key_hash(_key);
                const size_t i0 = m_b.mod0((uint_32) h);
                const uint_8 f0 = m_f0[i0];
                if ((f0 & SLOT_USED) && (f0 & SLOT_FRAG) == frag(h)) return false;
                if (!(f0 & SLOT_DEMOTED)) return true;
                const size_t i1 = m_b.mod1((uint_32) (h >> 32));
                return !((m_f1[i1] & SLOT_USED) || m_chain[i1]);
            }
            }
        }
        /// True when _key is present
        bool contains(const Key& _key) const { return find(_key) != cend(); }
        /// 1 when _key is present, 0 otherwise
        size_type count(const Key& _key) const { return find(_key) == cend() ? 0 : 1; }
        /// The value stored under _key, or the shared default when it is absent; nothing is inserted
        const Value& value(const Key& _key, const Value& _def = def_value()) const {
            const_iterator it = find(_key);
            return it == cend() ? _def : it->second;
        }
        /// The shared default value() answers, so a reference kept from an absent key stays valid
        static const Value& def_value() {
            static const Value s_val = Value();
            return s_val;
        }

        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL const Value* peek(const Key& _key) const {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (!locate(_key, zone, idx, blk, bidx)) return nullptr;
            return &entry_at(zone, idx, blk, bidx).second;
        }
        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL Value* peek(const Key& _key) {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (!locate(_key, zone, idx, blk, bidx)) return nullptr;
            return &entry_at(zone, idx, blk, bidx).second;
        }

        /// Insert a copy of _val under _key; an owning map clones the pointee
        iterator insert(const Key& _key, const Value& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { Value fresh = copy_value(_val); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_key, copy_value(_val)); })
                .first;
        }
        /// Insert by moving the value under a copied key; the source value is left null
        iterator insert(const Key& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(_key, std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert by moving key and value; the source value is left null
        iterator insert(Key&& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(std::move(_key), std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert a copy of an entry; an owning map clones the pointee
        iterator insert(const value_type& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { Value fresh = copy_value(_v.second); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_v.first, copy_value(_v.second)); })
                .first;
        }
        /// Insert by moving an entry; the source pointer is left null (do not move an entry out of a map)
        iterator insert(value_type&& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_v.second); detach_value(_v.second); },
                       [&](value_type& _slot) { _slot = std::move(_v); detach_value(_v.second); })
                .first;
        }
        /// Construct an entry from _args and insert it; a known key still pays the construction
        template <typename... Args>
        iterator emplace(Args&&... _args) {
            value_type v(std::forward<Args>(_args)...);
            return insert_impl(
                       v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(v.second); },
                       [&](value_type& _slot) { _slot = std::move(v); })
                .first;
        }
        /// Insert a value built from _args only when _key is absent; a known key neither constructs nor moves them
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(const Key& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(_key, Value(std::forward<Args>(_args)...)); });
        }
        /// As try_emplace(), moving the key on the insert path only
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(Key&& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(std::move(_key), Value(std::forward<Args>(_args)...)); });
        }
        /// The value stored under _key, default-constructing and inserting one when absent
        Value& operator[](const Key& _key) {
            iterator it = find(_key);
            if (it == end()) it = insert(_key, Value());
            return it->second;
        }

    public:
        /// Erase the entry of _key, returning 1 when it was there
        size_type erase(const Key& _key) {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (!locate(_key, zone, idx, blk, bidx)) return 0;
            erase_at(zone, idx, blk, bidx);
            return 1;
        }
        /// Erase the entry _it points at, returning its successor
        iterator erase(iterator _it) {
            erase_at(_it.m_zone, _it.m_idx, _it.m_blk, _it.m_bidx);
            return iterator(this, _it.m_zone, _it.m_idx, _it.m_blk, _it.m_bidx);
        }
        /// Remove the entry of _key, handing its value over without releasing it; the answer is Value() when absent
        Value take(const Key& _key) {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            if (!locate(_key, zone, idx, blk, bidx)) return Value();
            Value& slot = ZONE_CHAIN == zone ? blk->e[bidx].second
                                             : (ZONE_LINE == zone ? m_line[idx].second
                                                                  : (ZONE_BUCKET == zone || ZONE_L0 == zone ? m_l0[idx].second : m_l1[idx].second));
            Value out = std::move(slot);
            detach_value(slot);
            erase_at(zone, idx, blk, bidx);
            return out;
        }

    public:
        /// Iterator to the first entry, in iteration order
        iterator begin() {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            first_pos(this, zone, idx, blk, bidx);
            return iterator(this, zone, idx, blk, bidx);
        }
        /// One past the last entry
        iterator end() { return iterator(this, ZONE_END, 0, nullptr, 0); }
        /// Const iterator to the first entry, in iteration order
        const_iterator begin() const {
            uint_8 zone = ZONE_END;
            size_t idx = 0;
            block* blk = nullptr;
            uint_16 bidx = 0;
            first_pos(this, zone, idx, blk, bidx);
            return const_iterator(this, zone, idx, blk, bidx);
        }
        /// Const one past the last entry
        const_iterator end() const { return const_iterator(this, ZONE_END, 0, nullptr, 0); }
        /// Const iterator to the first entry, in iteration order
        const_iterator cbegin() const { return begin(); }
        /// Const one past the last entry
        const_iterator cend() const { return end(); }

    private:
        /// Hash a key; a pointer key is hashed by its address
        static uint_64 key_hash(const Key& _key) {
            return key_hash(_key, std::integral_constant<bool, std::is_pointer<Key>::value>());
        }
        static uint_64 key_hash(const Key& _key, std::false_type) { return hasher::eval(_key); }
        static uint_64 key_hash(const Key& _key, std::true_type) { return hasher::eval(reinterpret_cast<uint_64>(_key)); }

        /// The six high hash bits, placed at bits 2-7 of a flag byte; an equal hash always answers equal
        static uint_8 frag(uint_64 _h) { return (uint_8) ((_h >> 58) << 2); }

        /// The entry at a position, shared by the iterators and peek
        ALX_AMAP_INL value_type& entry_at(uint_8 _zone, size_t _idx, block* _blk, uint_16 _bidx) const {
            if (ZONE_CHAIN == _zone) return _blk->e[_bidx];
            if (ZONE_LINE == _zone) return m_line[_idx];
            if (ZONE_BUCKET == _zone) return m_l0[_idx];
            return ZONE_L0 == _zone ? m_l0[_idx] : m_l1[_idx];
        }

        /// Position of _key; false when absent. The demoted bit alone decides whether L1 and chains are reached
        ALX_AMAP_INL bool locate(const Key& _key, uint_8& _zone, size_t& _idx, block*& _blk, uint_16& _bidx) const {
            switch (mode()) {
            case MODE_LAZY:
                return false;
            case MODE_LINE:
                for (size_t i = 0; i < m_size; ++i) {
                    if (m_line[i].first == _key) {
                        _zone = ZONE_LINE;
                        _idx = i;
                        _blk = nullptr;
                        _bidx = 0;
                        return true;
                    }
                }
                return false;
            case MODE_BUCKET: {
                const uint_64 h = key_hash(_key);
                const uint_8 frag_h = frag(h);
                const size_t base = bucket_base(h);
                for (size_t k = 0; k < BUCKET_SLOTS; ++k) {
                    const size_t slot = base + k;
                    const uint_8 f = m_f0[slot];
                    if ((f & SLOT_USED) && (f & SLOT_FRAG) == frag_h && m_l0[slot].first == _key) {
                        _zone = ZONE_BUCKET;
                        _idx = slot;
                        _blk = nullptr;
                        _bidx = 0;
                        return true;
                    }
                }
                return false;
            }
            default:
                break;
            }
            const uint_64 h = key_hash(_key);
            const size_t i0 = m_b.mod0((uint_32) h);
            const uint_8 f0 = m_f0[i0];
            if ((f0 & SLOT_USED) && (f0 & SLOT_FRAG) == frag(h) && m_l0[i0].first == _key) {
                _zone = ZONE_L0;
                _idx = i0;
                _blk = nullptr;
                _bidx = 0;
                return true;
            }
            if (!(f0 & SLOT_DEMOTED)) return false;
            const size_t i1 = m_b.mod1((uint_32) (h >> 32));
            if ((m_f1[i1] & SLOT_USED) && m_l1[i1].first == _key) {
                _zone = ZONE_L1;
                _idx = i1;
                _blk = nullptr;
                _bidx = 0;
                return true;
            }
            block* b = m_chain[i1];
            if (b) {
                for (uint_16 k = 0; k < b->count; ++k) {
                    if (b->e[k].first == _key) {
                        _zone = ZONE_CHAIN;
                        _idx = i1;
                        _blk = b;
                        _bidx = k;
                        return true;
                    }
                }
            }
            return false;
        }

        /// Resume the L1 scan at slot _from: the slot, then its chain; ZONE_END when nothing follows
        template <typename MapPtr>
        static void scan_l1(MapPtr _m, uint_8& _zone, size_t& _idx, block*& _blk, uint_16& _bidx, size_t _from) {
            for (size_t j = _from; j < _m->m_q; ++j) {
                if (_m->m_f1[j] & SLOT_USED) {
                    _zone = ZONE_L1;
                    _idx = j;
                    _blk = nullptr;
                    _bidx = 0;
                    return;
                }
                if (_m->m_chain[j]) {
                    _zone = ZONE_CHAIN;
                    _idx = j;
                    _blk = _m->m_chain[j];
                    _bidx = 0;
                    return;
                }
            }
            _zone = ZONE_END;
            _idx = 0;
            _blk = nullptr;
            _bidx = 0;
        }

        /// Advance a position by one, shared by both iterators and erase
        template <typename MapPtr>
        static void step(MapPtr _m, uint_8& _zone, size_t& _idx, block*& _blk, uint_16& _bidx) {
            if (ZONE_LINE == _zone) {
                if (++_idx < _m->m_size) return;
                _zone = ZONE_END;
                _idx = 0;
                _blk = nullptr;
                _bidx = 0;
                return;
            }
            if (ZONE_BUCKET == _zone) {
                for (size_t i = _idx + 1; i < _m->m_p * BUCKET_SLOTS; ++i) {
                    if (_m->m_f0[i] & SLOT_USED) {
                        _idx = i;
                        return;
                    }
                }
                _zone = ZONE_END;
                _idx = 0;
                _blk = nullptr;
                _bidx = 0;
                return;
            }
            if (ZONE_L0 == _zone) {
                for (size_t i = _idx + 1; i < _m->m_p; ++i) {
                    if (_m->m_f0[i] & SLOT_USED) {
                        _idx = i;
                        return;
                    }
                }
                scan_l1(_m, _zone, _idx, _blk, _bidx, 0);
                return;
            }
            if (ZONE_L1 == _zone) {
                if (_m->m_chain[_idx]) {
                    _zone = ZONE_CHAIN;
                    _blk = _m->m_chain[_idx];
                    _bidx = 0;
                    return;
                }
                scan_l1(_m, _zone, _idx, _blk, _bidx, _idx + 1);
                return;
            }
            if ((size_t) _bidx + 1 < _blk->count) {
                ++_bidx;
                return;
            }
            scan_l1(_m, _zone, _idx, _blk, _bidx, _idx + 1);
        }

        /// Fill the position with the first entry; ZONE_END when the map is empty
        template <typename MapPtr>
        static void first_pos(MapPtr _m, uint_8& _zone, size_t& _idx, block*& _blk, uint_16& _bidx) {
            if (MODE_LINE == _m->mode()) {
                _zone = _m->m_size ? ZONE_LINE : ZONE_END;
                _idx = 0;
                _blk = nullptr;
                _bidx = 0;
                return;
            }
            if (MODE_BUCKET == _m->mode()) {
                for (size_t i = 0; i < _m->m_p * BUCKET_SLOTS; ++i) {
                    if (_m->m_f0[i] & SLOT_USED) {
                        _zone = ZONE_BUCKET;
                        _idx = i;
                        _blk = nullptr;
                        _bidx = 0;
                        return;
                    }
                }
                _zone = ZONE_END;
                _idx = 0;
                _blk = nullptr;
                _bidx = 0;
                return;
            }
            _zone = ZONE_END;
            _idx = 0;
            _blk = nullptr;
            _bidx = 0;
            if (MODE_LAZY == _m->mode()) return;
            for (size_t i = 0; i < _m->m_p; ++i) {
                if (_m->m_f0[i] & SLOT_USED) {
                    _zone = ZONE_L0;
                    _idx = i;
                    return;
                }
            }
            scan_l1(_m, _zone, _idx, _blk, _bidx, 0);
        }

        /**
         * \brief Probe for _key, then _assign the stored value or _place a fresh entry
         *
         * The answer's second member says whether the entry was created. Every growth trigger is
         * tested before the store that would feed it, so a rehash never invalidates the answer.
         */
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_impl(const Key& _key, Assign _assign, Place _place) {
            if (!m_line) m_line = new value_type[LINE_SLOTS];
            if (MODE_LINE == mode()) return insert_line(_key, _assign, _place);
            if (MODE_BUCKET == mode()) return insert_bucket(_key, _assign, _place);
            return insert_zone(_key, _assign, _place);
        }

        /// Probe the linear block and append there; the fifth entry promotes the map
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_line(const Key& _key, Assign& _assign, Place& _place) {
            for (size_t i = 0; i < m_size; ++i) {
                if (m_line[i].first == _key) {
                    _assign(m_line[i].second);
                    return {iterator(this, ZONE_LINE, i, nullptr, 0), false};
                }
            }
            if (m_size < LINE_SLOTS) {
                _place(m_line[m_size]);
                ++m_size;
                return {iterator(this, ZONE_LINE, m_size - 1, nullptr, 0), true};
            }
            promote_line();
            return insert_bucket(_key, _assign, _place);
        }

        /// Probe _key's bucket; a full bucket doubles the table or hands over to the two zones
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_bucket(const Key& _key, Assign& _assign, Place& _place) {
            while (MODE_BUCKET == mode()) {
                const uint_64 h = key_hash(_key);
                const uint_8 frag_h = frag(h);
                const size_t base = bucket_base(h);
                for (size_t k = 0; k < BUCKET_SLOTS; ++k) {
                    const size_t slot = base + k;
                    const uint_8 f = m_f0[slot];
                    if ((f & SLOT_USED) && (f & SLOT_FRAG) == frag_h && m_l0[slot].first == _key) {
                        _assign(m_l0[slot].second);
                        return {iterator(this, ZONE_BUCKET, slot, nullptr, 0), false};
                    }
                }
                for (size_t k = 0; k < BUCKET_SLOTS; ++k) {
                    const size_t slot = base + k;
                    if (!(m_f0[slot] & SLOT_USED)) {
                        _place(m_l0[slot]);
                        m_f0[slot] = (uint_8) (SLOT_USED | frag_h);
                        ++m_size;
                        return {iterator(this, ZONE_BUCKET, slot, nullptr, 0), true};
                    }
                }
                if (m_p < BUCKET_MAX) rehash_bucket(m_p * 2);
                else promote_bucket();
            }
            return insert_zone(_key, _assign, _place);
        }

        /// Probe the two zones and their chains; a growth trigger rehashes and retries
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_zone(const Key& _key, Assign& _assign, Place& _place) {
            const uint_64 h = key_hash(_key);
            const uint_32 lo = (uint_32) h;
            const uint_32 hi = (uint_32) (h >> 32);
            const uint_8 frag_h = frag(h);
            while (true) {
                const size_t i0 = m_b.mod0(lo);
                const uint_8 f0 = m_f0[i0];
                if ((f0 & SLOT_USED) && (f0 & SLOT_FRAG) == frag_h && m_l0[i0].first == _key) {
                    _assign(m_l0[i0].second);
                    return {iterator(this, ZONE_L0, i0, nullptr, 0), false};
                }
                const bool demoted = 0 != (f0 & SLOT_DEMOTED);
                size_t i1 = 0;
                if (demoted) {
                    i1 = m_b.mod1(hi);
                    if ((m_f1[i1] & SLOT_USED) && m_l1[i1].first == _key) {
                        _assign(m_l1[i1].second);
                        return {iterator(this, ZONE_L1, i1, nullptr, 0), false};
                    }
                    block* b = m_chain[i1];
                    if (b) {
                        for (uint_16 k = 0; k < b->count; ++k) {
                            if (b->e[k].first == _key) {
                                _assign(b->e[k].second);
                                return {iterator(this, ZONE_CHAIN, i1, b, k), false};
                            }
                        }
                    }
                }
                if ((size_t) m_chain_n + 1 > (size_t) m_q * 3 / 4) {
                    grow_zone();
                    continue;
                }
                if (!(f0 & SLOT_USED)) {
                    if (((size_t) m_l0_n + 1) * 8 > (size_t) m_p * 7) {
                        grow_zone();
                        continue;
                    }
                    _place(m_l0[i0]);
                    m_f0[i0] = (uint_8) ((f0 & SLOT_DEMOTED) | SLOT_USED | frag_h);
                    ++m_l0_n;
                    ++m_size;
                    return {iterator(this, ZONE_L0, i0, nullptr, 0), true};
                }
                if (!demoted) i1 = m_b.mod1(hi);
                if (!(m_f1[i1] & SLOT_USED)) {
                    _place(m_l1[i1]);
                    m_f1[i1] |= SLOT_USED;
                    m_f0[i0] |= SLOT_DEMOTED;
                    ++m_size;
                    return {iterator(this, ZONE_L1, i1, nullptr, 0), true};
                }
                block* b = m_chain[i1];
                if ((b ? b->count : 0) >= CHAIN_BLOCK) {
                    grow_zone();
                    continue;
                }
                if (!b) {
                    block* fresh = block_get();
                    try {
                        _place(fresh->e[0]);
                    } catch (...) {
                        block_put(fresh);
                        throw;
                    }
                    fresh->count = 1;
                    m_chain[i1] = fresh;
                    b = fresh;
                } else {
                    _place(b->e[b->count]);
                    ++b->count;
                }
                ++m_chain_n;
                m_f0[i0] |= SLOT_DEMOTED;
                ++m_size;
                return {iterator(this, ZONE_CHAIN, i1, b, (uint_16) (b->count - 1)), true};
            }
        }

        /// Bucket base index from the mixed hash halves: a weak low half on its own must not
        /// collapse every key into one bucket
        size_t bucket_base(uint_64 _h) const {
            const uint_32 mix = (uint_32) _h ^ (uint_32) (_h >> 32);
            return (size_t) (mix & (uint_32) (m_p - 1)) * BUCKET_SLOTS;
        }

        /// Move the linear block's entries into a fresh two-bucket table; the block itself stays
        void promote_line() {
            value_type* src = m_line;
            const size_t n = m_size;
            make_buckets(2);
            m_size = 0;
            for (size_t i = 0; i < n; ++i) absorb(std::move(src[i]));
        }

        /// Rebuild the bucket table as two zones, keeping every entry
        void promote_bucket() {
            size_t np = 0;
            size_t nq = 0;
            geometry_for(m_size + 1, np, nq);
            bucket_to_zone(np, nq);
        }

        /// Rebuild the bucket table with _b buckets, keeping every entry
        void rehash_bucket(size_t _b) {
            value_type* slab = m_l0;
            uint_8* flags = m_f0;
            const size_t cap = m_p * BUCKET_SLOTS;
            make_buckets(_b);
            m_size = 0;
            for (size_t i = 0; i < cap; ++i) {
                if (flags[i] & SLOT_USED) absorb(std::move(slab[i]));
            }
            delete[] slab;
            delete[] flags;
        }

        /// Replay every entry into two zones of the given geometry; a full chain grows again
        void rehash_zone(size_t _p, size_t _q) {
            value_type* l0 = m_l0;
            value_type* l1 = m_l1;
            uint_8* f0 = m_f0;
            uint_8* f1 = m_f1;
            block** ch = m_chain;
            const size_t op = m_p;
            const size_t oq = m_q;
            make_table(_p, _q);
            m_size = 0;
            for (size_t i = 0; i < op; ++i) {
                if (f0[i] & SLOT_USED) absorb(std::move(l0[i]));
            }
            for (size_t i = 0; i < oq; ++i) {
                if (f1[i] & SLOT_USED) absorb(std::move(l1[i]));
                block* b = ch[i];
                if (b) {
                    for (uint_16 k = 0; k < b->count; ++k) absorb(std::move(b->e[k]));
                    delete b;
                }
            }
            delete[] l0;
            delete[] l1;
            delete[] f0;
            delete[] f1;
            delete[] ch;
        }

        /// Move every linear entry into two zones of the given geometry
        void line_to_zone(size_t _p, size_t _q) {
            value_type* src = m_line;
            const size_t n = m_size;
            make_table(_p, _q);
            m_size = 0;
            for (size_t i = 0; i < n; ++i) absorb(std::move(src[i]));
        }

        /// Move every bucket entry into two zones of the given geometry
        void bucket_to_zone(size_t _p, size_t _q) {
            value_type* slab = m_l0;
            uint_8* flags = m_f0;
            const size_t cap = m_p * BUCKET_SLOTS;
            make_table(_p, _q);
            m_size = 0;
            for (size_t i = 0; i < cap; ++i) {
                if (flags[i] & SLOT_USED) absorb(std::move(slab[i]));
            }
            delete[] slab;
            delete[] flags;
        }

        /// Store an entry known to be absent into the current shape, growing until it fits
        void absorb(value_type&& _v) {
            while (!(MODE_BUCKET == mode() ? place_bucket(std::move(_v)) : place_zone(std::move(_v)))) grow();
        }

        /// Store an entry known to be absent into its bucket; false when the bucket is full
        bool place_bucket(value_type&& _v) {
            const uint_64 h = key_hash(_v.first);
            const uint_8 frag_h = frag(h);
            const size_t base = bucket_base(h);
            for (size_t k = 0; k < BUCKET_SLOTS; ++k) {
                if (!(m_f0[base + k] & SLOT_USED)) {
                    m_l0[base + k] = std::move(_v);
                    m_f0[base + k] = (uint_8) (SLOT_USED | frag_h);
                    ++m_size;
                    return true;
                }
            }
            return false;
        }

        /// Store an entry known to be absent into the two zones; false when its chain is full
        bool place_zone(value_type&& _v) {
            const uint_64 h = key_hash(_v.first);
            const size_t i0 = m_b.mod0((uint_32) h);
            if (!(m_f0[i0] & SLOT_USED)) {
                m_l0[i0] = std::move(_v);
                m_f0[i0] = (uint_8) ((m_f0[i0] & SLOT_DEMOTED) | SLOT_USED | frag(h));
                ++m_l0_n;
                ++m_size;
                return true;
            }
            m_f0[i0] |= SLOT_DEMOTED;
            const size_t i1 = m_b.mod1((uint_32) (h >> 32));
            if (!(m_f1[i1] & SLOT_USED)) {
                m_l1[i1] = std::move(_v);
                m_f1[i1] |= SLOT_USED;
                ++m_size;
                return true;
            }
            block* b = m_chain[i1];
            if (!b) {
                b = block_get();
                b->e[0] = std::move(_v);
                b->count = 1;
                m_chain[i1] = b;
                ++m_chain_n;
                ++m_size;
                return true;
            }
            if (b->count >= CHAIN_BLOCK) return false;
            b->e[b->count] = std::move(_v);
            ++b->count;
            ++m_chain_n;
            ++m_size;
            return true;
        }

        /// Grow whatever shape is current
        void grow() {
            if (MODE_BUCKET == mode()) {
                if (m_p < BUCKET_MAX) rehash_bucket(m_p * 2);
                else promote_bucket();
                return;
            }
            grow_zone();
        }

        /// Slot geometry that holds _n insertions; answers the target slot count
        static size_t geometry_for(size_t _n, size_t& _p, size_t& _q) {
            const size_t target = _n * 20 / 13;
            const size_t half = target / 2;
            const size_t nq = prev_prime(half < MIN_L1 ? MIN_L1 + 1 : half + 1);
            _p = next_prime(nq + 1);
            _q = nq;
            return target;
        }

        /// Allocate an empty bucket table of _b buckets
        void make_buckets(size_t _b) {
            const size_t cap = _b * BUCKET_SLOTS;
            value_type* slab = new value_type[cap];
            uint_8* flags = nullptr;
            try {
                flags = new uint_8[cap]();
            } catch (...) {
                delete[] slab;
                throw;
            }
            m_l0 = slab;
            m_f0 = flags;
            m_p = (uint_32) _b;
            m_q = 0;
            m_l0_n = 0;
            m_chain_n = 0;
        }

        /// Remove the entry at a position and turn the position into its successor
        void erase_at(uint_8& _zone, size_t& _idx, block*& _blk, uint_16& _bidx) {
            if (ZONE_LINE == _zone) {
                drop_value(m_line[_idx].second);
                for (size_t i = _idx; (size_t) (i + 1) < m_size; ++i) m_line[i] = std::move(m_line[i + 1]);
                --m_size;
                if (_idx < m_size) return;
                _zone = ZONE_END;
                _idx = 0;
                _blk = nullptr;
                _bidx = 0;
                return;
            }
            if (ZONE_BUCKET == _zone) {
                drop_value(m_l0[_idx].second);
                m_f0[_idx] &= (uint_8) ~SLOT_USED;
                --m_size;
                step(this, _zone, _idx, _blk, _bidx);
                return;
            }
            if (ZONE_CHAIN == _zone) {
                drop_value(_blk->e[_bidx].second);
                for (uint_16 k = _bidx; (size_t) (k + 1) < _blk->count; ++k) _blk->e[k] = std::move(_blk->e[k + 1]);
                --_blk->count;
                --m_chain_n;
                --m_size;
                if (_blk->count) {
                    if ((size_t) _bidx >= _blk->count) step(this, _zone, _idx, _blk, _bidx);
                    return;
                }
                const size_t slot = _idx;
                block* emptied = _blk;
                step(this, _zone, _idx, _blk, _bidx);
                m_chain[slot] = nullptr;
                block_put(emptied);
                return;
            }
            if (ZONE_L0 == _zone) {
                drop_value(m_l0[_idx].second);
                m_f0[_idx] &= (uint_8) ~SLOT_USED;
                --m_l0_n;
            } else {
                drop_value(m_l1[_idx].second);
                m_f1[_idx] &= (uint_8) ~SLOT_USED;
            }
            --m_size;
            step(this, _zone, _idx, _blk, _bidx);
        }

        /// An empty block, from the free list when one was recycled
        block* block_get() {
            if (!m_free) return new block();
            block* b = m_free;
            m_free = b->next;
            b->next = nullptr;
            b->count = 0;
            return b;
        }

        /// Recycle a chain block
        void block_put(block* _b) {
            _b->next = m_free;
            m_free = _b;
        }

        /// Rebuild the two-zone table at twice the slots below 64K, one and a half above
        void grow_zone() {
            const size_t target = m_p < 65536 ? (size_t) m_p * 2 : (size_t) m_p * 3 / 2;
            const size_t np = next_prime(target);
            rehash_zone(np, prev_prime(np));
        }

        /// Allocate empty arrays of the given geometry, keeping m_size
        void make_table(size_t _p, size_t _q) {
            value_type* l0 = nullptr;
            value_type* l1 = nullptr;
            uint_8* f0 = nullptr;
            uint_8* f1 = nullptr;
            block** ch = nullptr;
            try {
                l0 = new value_type[_p];
                l1 = new value_type[_q];
                f0 = new uint_8[_p]();
                f1 = new uint_8[_q]();
                ch = new block*[_q]();
            } catch (...) {
                delete[] l0;
                delete[] l1;
                delete[] f0;
                delete[] f1;
                delete[] ch;
                throw;
            }
            m_l0 = l0;
            m_l1 = l1;
            m_f0 = f0;
            m_f1 = f1;
            m_chain = ch;
            m_p = (uint_32) _p;
            m_q = (uint_32) _q;
            m_l0_n = 0;
            m_chain_n = 0;
            m_b = barrett((uint_32) _p, (uint_32) _q);
        }

        /// Clone _v position by position; the hash is not consulted again
        void clone(const map_hash& _v) {
            if (!_v.m_line) return;
            m_line = new value_type[LINE_SLOTS];
            m_size = 0;
            if (MODE_LINE == _v.mode()) {
                for (size_t i = 0; i < _v.m_size; ++i) {
                    copy_entry(m_line[i], _v.m_line[i]);
                    ++m_size;
                }
                return;
            }
            if (MODE_BUCKET == _v.mode()) {
                make_buckets(_v.m_p);
                const size_t cap = _v.m_p * BUCKET_SLOTS;
                for (size_t i = 0; i < cap; ++i) {
                    if (_v.m_f0[i] & SLOT_USED) {
                        copy_entry(m_l0[i], _v.m_l0[i]);
                        ++m_size;
                    }
                    m_f0[i] = _v.m_f0[i];
                }
                return;
            }
            make_table(_v.m_p, _v.m_q);
            for (size_t i = 0; i < m_p; ++i) {
                if (_v.m_f0[i] & SLOT_USED) {
                    copy_entry(m_l0[i], _v.m_l0[i]);
                    ++m_l0_n;
                    ++m_size;
                }
                m_f0[i] = _v.m_f0[i];
            }
            for (size_t i = 0; i < m_q; ++i) {
                if (_v.m_f1[i] & SLOT_USED) {
                    copy_entry(m_l1[i], _v.m_l1[i]);
                    m_f1[i] = SLOT_USED;
                    ++m_size;
                }
                if (!_v.m_chain[i]) continue;
                block* b = block_get();
                m_chain[i] = b;
                const uint_16 n = _v.m_chain[i]->count;
                for (uint_16 k = 0; k < n; ++k) {
                    copy_entry(b->e[k], _v.m_chain[i]->e[k]);
                    ++b->count;
                    ++m_chain_n;
                    ++m_size;
                }
            }
        }

        /// Drop every entry and release the arrays of the current shape; the linear block stays
        void shed() {
            if (!m_line) return;
            if (MODE_LINE == mode()) {
                for (size_t i = 0; i < m_size; ++i) drop_value(m_line[i].second);
            } else if (MODE_BUCKET == mode()) {
                const size_t cap = m_p * BUCKET_SLOTS;
                for (size_t i = 0; i < cap; ++i) {
                    if (m_f0[i] & SLOT_USED) drop_value(m_l0[i].second);
                }
                delete[] m_l0;
                delete[] m_f0;
            } else {
                for (size_t i = 0; i < m_p; ++i) {
                    if (m_f0[i] & SLOT_USED) drop_value(m_l0[i].second);
                }
                for (size_t i = 0; i < m_q; ++i) {
                    if (m_f1[i] & SLOT_USED) drop_value(m_l1[i].second);
                    block* b = m_chain[i];
                    if (b) {
                        for (uint_16 k = 0; k < b->count; ++k) drop_value(b->e[k].second);
                        delete b;
                    }
                }
                delete[] m_l0;
                delete[] m_l1;
                delete[] m_f0;
                delete[] m_f1;
                delete[] m_chain;
            }
            while (m_free) {
                block* next = m_free->next;
                delete m_free;
                m_free = next;
            }
            m_l0 = nullptr;
            m_l1 = nullptr;
            m_f0 = nullptr;
            m_f1 = nullptr;
            m_chain = nullptr;
            m_free = nullptr;
            m_p = 0;
            m_q = 0;
            m_l0_n = 0;
            m_chain_n = 0;
            m_size = 0;
            m_b = barrett();
        }

        /// Release every entry and the linear block, returning to the lazy state
        void destroy() {
            shed();
            delete[] m_line;
            m_line = nullptr;
        }

        /// Take over the members of _v, leaving it empty
        void steal(map_hash& _v) noexcept {
            m_line = _v.m_line;
            m_l0 = _v.m_l0;
            m_l1 = _v.m_l1;
            m_f0 = _v.m_f0;
            m_f1 = _v.m_f1;
            m_chain = _v.m_chain;
            m_free = _v.m_free;
            m_p = _v.m_p;
            m_q = _v.m_q;
            m_l0_n = _v.m_l0_n;
            m_chain_n = _v.m_chain_n;
            m_size = _v.m_size;
            m_b = _v.m_b;
            _v.reset();
        }

        /// Return to the lazy state without touching any entry
        void reset() noexcept {
            m_line = nullptr;
            m_l0 = nullptr;
            m_l1 = nullptr;
            m_f0 = nullptr;
            m_f1 = nullptr;
            m_chain = nullptr;
            m_free = nullptr;
            m_p = 0;
            m_q = 0;
            m_l0_n = 0;
            m_chain_n = 0;
            m_size = 0;
            m_b = barrett();
        }

        /// Trial division by the odd numbers; only the rehash path calls it
        static bool is_prime(size_t _n) {
            if (_n < 2) return false;
            if (0 == _n % 2) return 2 == _n;
            for (size_t d = 3; d * d <= _n; d += 2) {
                if (0 == _n % d) return false;
            }
            return true;
        }
        static size_t next_prime(size_t _n) {
            if (_n <= 2) return 2;
            if (0 == _n % 2) ++_n;
            while (!is_prime(_n)) _n += 2;
            return _n;
        }
        static size_t prev_prime(size_t _n) {
            if (_n <= 2) return 2;
            _n = 0 == _n % 2 ? _n - 1 : _n - 2;
            while (_n > 2 && !is_prime(_n)) _n -= 2;
            return _n;
        }

    private:
        value_type* m_line = nullptr;
        value_type* m_l0 = nullptr;
        value_type* m_l1 = nullptr;
        uint_8* m_f0 = nullptr;
        uint_8* m_f1 = nullptr;
        block** m_chain = nullptr;
        block* m_free = nullptr;
        size_t m_size = 0;
        /// Counters and moduli stay 32-bit: a table cannot exceed the 32-bit moduli anyway
        uint_32 m_p = 0;
        uint_32 m_q = 0;
        uint_32 m_l0_n = 0;
        uint_32 m_chain_n = 0;
        barrett m_b;
    };

    /**
     * \brief Read-optimized sparse hash map: flat linear probing over a dense tag array
     *
     * The full map interface, deliberately lopsided towards lookup. A read hashes the key
     * once, touches one tag byte -- a seven-bit fingerprint -- and compares the key only on
     * a match; a miss usually stops at the first tag. Live entries plus tombstones are held
     * within [1/8, 1/4] of a power-of-two capacity, so most slots are empty and memory runs several times a dense
     * map's: the type is meant for a finite element set built once and then read. Writes are
     * plain open addressing: an erased slot keeps a tombstone that the probe skips and a
     * later insert reuses, so an erase costs one probe and erase(iterator) answers the next
     * occupied slot -- a conditional delete during iteration never sees an entry twice. The
     * table grows once live entries plus tombstones pass a quarter of the capacity, and a
     * rehash clears the tombstones once they outnumber the live entries; an iterator erase
     * never resizes, so a drained map keeps its slots for reuse until clear(). insert() gives
     * insert_or_assign semantics: an existing key is
     * overwritten and an insert never fails.
     * value_type keeps the key non-const, and the caller must not modify it once inserted.
     * Any insert or erase invalidates iterators and references into entries; a pointer
     * Value's pointee is stable. The iteration order is an implementation detail. Key must
     * be default constructible, copyable, movable and comparable with operator==; a pointer
     * key is stored, compared and hashed by address. Hash must answer hash::func<Hash,
     * uint_64> and keep its low and high halves independent. No internal synchronization.
     *
     * IsOwnPtr: when true, Value must be a pointer and the map owns the pointee, under the
     * same contract as map_btree's.
     */
    template <typename Key, typename Value, bool IsOwnPtr = false, typename Hash = hash::auto_64>
    class map_read {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<Key, Value>;
        using size_type = size_t;

    private:
        static_assert(!IsOwnPtr || std::is_pointer<Value>::value, "map_read: IsOwnPtr requires a pointer value type");
        using pointee_type = typename std::remove_pointer<Value>::type;
        using hasher = hash::func<Hash, uint_64>;

        /// Slots in the smallest table; the load stays within [1/8, 1/4] of the capacity
        static constexpr size_t MIN_CAP = 4;
        /// Zero marks an empty slot; every other value is a seven-bit fingerprint of the hash
        static constexpr uint_8 SLOT_EMPTY = 0;
        /// The fingerprint range never reaches this value, so it marks an erased, reusable slot
        static constexpr uint_8 SLOT_DELETED = 0xFF;

        /// True for a slot that holds an entry
        static bool live(uint_8 _t) { return SLOT_EMPTY != _t && SLOT_DELETED != _t; }

        /// Release the value stored in a slot; a plain map has nothing to release
        static void drop_value(Value&, std::false_type) {}
        static void drop_value(Value& _v, std::true_type) { delete _v; }
        static void drop_value(Value& _v) { drop_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// The value moved out of a slot that keeps no claim on it
        static void detach_value(Value&, std::false_type) {}
        static void detach_value(Value& _v, std::true_type) { _v = nullptr; }
        static void detach_value(Value& _v) { detach_value(_v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy an entry into a fresh slot; an owning map clones the pointee
        static void copy_entry(value_type& _slot, const value_type& _v, std::false_type) { _slot = _v; }
        static void copy_entry(value_type& _slot, const value_type& _v, std::true_type) {
            _slot.first = _v.first;
            _slot.second = _v.second ? new pointee_type(*_v.second) : nullptr;
        }
        static void copy_entry(value_type& _slot, const value_type& _v) { copy_entry(_slot, _v, std::integral_constant<bool, IsOwnPtr>()); }
        /// Copy a value; an owning map clones the pointee
        static Value copy_value(const Value& _v, std::false_type) { return _v; }
        static Value copy_value(const Value& _v, std::true_type) { return _v ? new pointee_type(*_v) : nullptr; }
        static Value copy_value(const Value& _v) { return copy_value(_v, std::integral_constant<bool, IsOwnPtr>()); }

    public:
        class const_iterator;

        /// Mutable iterator; any insert or erase may invalidate it
        class iterator {
            friend class map_read;
            friend class const_iterator;

            map_read* m_map;
            size_t m_idx;

        public:
            using value_type = typename map_read::value_type;

            /// Wrap a slot position
            iterator(map_read* _map, size_t _idx) : m_map(_map), m_idx(_idx) {}
            /// The current entry; its key must not be modified
            value_type& operator*() const { return m_map->m_slot[m_idx]; }
            /// Pointer to the current entry
            value_type* operator->() const { return &m_map->m_slot[m_idx]; }
            /// Advance to the next occupied slot
            iterator& operator++() {
                m_map->step(m_idx);
                return *this;
            }
            /// Advance, returning a copy of the previous position
            iterator operator++(int) {
                iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const iterator& _it) const { return m_map == _it.m_map && m_idx == _it.m_idx; }
            /// Compare two positions
            bool operator!=(const iterator& _it) const { return !(*this == _it); }
        };

        /// Const iterator over the entries
        class const_iterator {
            friend class map_read;

            const map_read* m_map;
            size_t m_idx;

        public:
            using value_type = typename map_read::value_type;

            /// Wrap a slot position
            const_iterator(const map_read* _map, size_t _idx) : m_map(_map), m_idx(_idx) {}
            /// Convert a mutable iterator
            const_iterator(const iterator& _it) : m_map(_it.m_map), m_idx(_it.m_idx) {}
            /// The current entry
            const value_type& operator*() const { return m_map->m_slot[m_idx]; }
            /// Pointer to the current entry
            const value_type* operator->() const { return &m_map->m_slot[m_idx]; }
            /// Advance to the next occupied slot
            const_iterator& operator++() {
                m_map->step(m_idx);
                return *this;
            }
            /// Advance, returning a copy of the previous position
            const_iterator operator++(int) {
                const_iterator temp(*this);
                ++*this;
                return temp;
            }
            /// Compare two positions
            bool operator==(const const_iterator& _it) const { return m_map == _it.m_map && m_idx == _it.m_idx; }
            /// Compare two positions
            bool operator!=(const const_iterator& _it) const { return !(*this == _it); }
        };

    public:
        /// Construct an empty map; the table is built on the first insertion
        map_read() noexcept = default;
        /// Copy; the entries are cloned slot by slot
        map_read(const map_read& _v) {
            try {
                clone(_v);
            } catch (...) {
                destroy();
                throw;
            }
        }
        /// Take over the entries of _v, which is left empty
        map_read(map_read&& _v) noexcept { steal(_v); }
        /// Copy; the previous contents are released first
        map_read& operator=(const map_read& _v) {
            if (this != &_v) {
                map_read temp(_v);
                swap(temp);
            }
            return *this;
        }
        /// Take over the entries of _v, which is left empty
        map_read& operator=(map_read&& _v) noexcept {
            if (this != &_v) {
                destroy();
                steal(_v);
            }
            return *this;
        }
        /// Release every entry
        ~map_read() { destroy(); }

    public:
        /// Number of entries
        size_type size() const noexcept { return m_size; }
        /// True while the map holds no entry
        bool empty() const noexcept { return 0 == m_size; }
        /// Drop every entry and release the table
        void clear() { destroy(); }
        /// Exchange the contents with _v
        void swap(map_read& _v) noexcept {
            std::swap(m_tag, _v.m_tag);
            std::swap(m_slot, _v.m_slot);
            std::swap(m_cap, _v.m_cap);
            std::swap(m_size, _v.m_size);
            std::swap(m_tomb, _v.m_tomb);
        }
        /// Build enough storage that _n insertions do not grow the table
        void reserve(size_type _n) {
            if (0 == _n) return;
            const size_t want = pow2_at_least(_n * 4);
            if (want > m_cap) rehash(want);
        }

    public:
        /// Look up _key, yielding end() when it is absent
        iterator find(const Key& _key) {
            size_t idx = 0;
            return locate(_key, idx) ? iterator(this, idx) : end();
        }
        /// Look up _key, yielding cend() when it is absent
        const_iterator find(const Key& _key) const {
            size_t idx = 0;
            return locate(_key, idx) ? const_iterator(this, idx) : cend();
        }
        /// True when _key is present
        bool contains(const Key& _key) const { return nullptr != peek(_key); }
        /// 1 when _key is present, 0 otherwise
        size_type count(const Key& _key) const { return nullptr != peek(_key) ? 1 : 0; }
        /// The value stored under _key, or the shared default when it is absent; nothing is inserted
        const Value& value(const Key& _key, const Value& _def = def_value()) const {
            const Value* v = peek(_key);
            return v ? *v : _def;
        }
        /// The shared default value() answers, so a reference kept from an absent key stays valid
        static const Value& def_value() {
            static const Value s_val = Value();
            return s_val;
        }
        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL const Value* peek(const Key& _key) const {
            size_t idx = 0;
            return locate(_key, idx) ? &m_slot[idx].second : nullptr;
        }
        /// The value stored under _key, or nullptr when it is absent
        ALX_AMAP_INL Value* peek(const Key& _key) {
            size_t idx = 0;
            return locate(_key, idx) ? &m_slot[idx].second : nullptr;
        }

        /// Insert a copy of _val under _key; an owning map clones the pointee
        iterator insert(const Key& _key, const Value& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { Value fresh = copy_value(_val); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_key, copy_value(_val)); })
                .first;
        }
        /// Insert by moving the value under a copied key; the source value is left null
        iterator insert(const Key& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(_key, std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert by moving key and value; the source value is left null
        iterator insert(Key&& _key, Value&& _val) {
            return insert_impl(
                       _key, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_val); detach_value(_val); },
                       [&](value_type& _slot) { _slot = value_type(std::move(_key), std::move(_val)); detach_value(_val); })
                .first;
        }
        /// Insert a copy of an entry; an owning map clones the pointee
        iterator insert(const value_type& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { Value fresh = copy_value(_v.second); drop_value(_slot); _slot = fresh; },
                       [&](value_type& _slot) { _slot = value_type(_v.first, copy_value(_v.second)); })
                .first;
        }
        /// Insert by moving an entry; the source pointer is left null (do not move an entry out of a map)
        iterator insert(value_type&& _v) {
            return insert_impl(
                       _v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(_v.second); detach_value(_v.second); },
                       [&](value_type& _slot) { _slot = std::move(_v); detach_value(_v.second); })
                .first;
        }
        /// Construct an entry from _args and insert it; a known key still pays the construction
        template <typename... Args>
        iterator emplace(Args&&... _args) {
            value_type v(std::forward<Args>(_args)...);
            return insert_impl(
                       v.first, [&](Value& _slot) { drop_value(_slot); _slot = std::move(v.second); },
                       [&](value_type& _slot) { _slot = std::move(v); })
                .first;
        }
        /// Insert a value built from _args only when _key is absent; a known key neither constructs nor moves them
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(const Key& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(_key, Value(std::forward<Args>(_args)...)); });
        }
        /// As try_emplace(), moving the key on the insert path only
        template <typename... Args>
        std::pair<iterator, bool> try_emplace(Key&& _key, Args&&... _args) {
            return insert_impl(
                _key, [](Value&) {},
                [&](value_type& _slot) { _slot = value_type(std::move(_key), Value(std::forward<Args>(_args)...)); });
        }
        /// The value stored under _key, default-constructing and inserting one when absent
        Value& operator[](const Key& _key) {
            iterator it = find(_key);
            if (it == end()) it = insert(_key, Value());
            return it->second;
        }

    public:
        /// Erase the entry of _key, returning 1 when it was there
        size_type erase(const Key& _key) {
            size_t idx = 0;
            if (!locate(_key, idx)) return 0;
            erase_pos(idx);
            shrink();
            return 1;
        }
        /// Erase the entry _it points at, returning the next occupied slot; never resizes --
        /// a map drained this way keeps its slots (and tombstones) for reuse until clear()
        iterator erase(iterator _it) {
            erase_pos(_it.m_idx);
            size_t j = _it.m_idx + 1;
            while (j < m_cap && !live(m_tag[j])) ++j;
            return iterator(this, j);
        }
        /// Remove the entry of _key, handing its value over without releasing it; the answer is Value() when absent
        Value take(const Key& _key) {
            size_t idx = 0;
            if (!locate(_key, idx)) return Value();
            Value out = std::move(m_slot[idx].second);
            detach_value(m_slot[idx].second);
            erase_pos(idx);
            shrink();
            return out;
        }

    public:
        /// Iterator to the first entry, in iteration order
        iterator begin() noexcept { return iterator(this, first()); }
        /// One past the last entry
        iterator end() noexcept { return iterator(this, m_cap); }
        /// Const iterator to the first entry, in iteration order
        const_iterator begin() const noexcept { return const_iterator(this, first()); }
        /// Const one past the last entry
        const_iterator end() const noexcept { return const_iterator(this, m_cap); }
        /// Const iterator to the first entry, in iteration order
        const_iterator cbegin() const noexcept { return begin(); }
        /// Const one past the last entry
        const_iterator cend() const noexcept { return end(); }

    private:
        /// Hash a key; a pointer key is hashed by its address
        static uint_64 key_hash(const Key& _key) {
            return key_hash(_key, std::integral_constant<bool, std::is_pointer<Key>::value>());
        }
        static uint_64 key_hash(const Key& _key, std::false_type) { return hasher::eval(_key); }
        static uint_64 key_hash(const Key& _key, std::true_type) { return hasher::eval(reinterpret_cast<uint_64>(_key)); }

        /// The seven high hash bits plus one; zero and 0xFF stay reserved for the slot states
        static uint_8 tag_of(uint_64 _h) { return (uint_8) ((_h >> 57) + 1); }

        /// Position of _key; false when absent. Every probe step touches one tag byte only
        ALX_AMAP_INL bool locate(const Key& _key, size_t& _idx) const {
            if (!m_cap) return false;
            const uint_64 h = key_hash(_key);
            const uint_8 t = tag_of(h);
            const size_t mask = m_cap - 1;
            size_t i = (size_t) h & mask;
            for (;;) {
                const uint_8 s = m_tag[i];
                if (SLOT_EMPTY == s) return false;
                if (s == t && m_slot[i].first == _key) {
                    _idx = i;
                    return true;
                }
                i = (i + 1) & mask;
            }
        }

        /// First occupied slot, or the capacity when none is
        size_t first() const {
            size_t i = 0;
            while (i < m_cap && !live(m_tag[i])) ++i;
            return i;
        }

        /// Advance a slot index to the next occupied slot or the capacity
        void step(size_t& _i) const {
            while (++_i < m_cap && !live(m_tag[_i])) {
            }
        }

        /// Mark the slot at _i erased; the tombstone keeps the probe chain intact and is
        /// reused by a later insert
        void erase_pos(size_t _i) {
            drop_value(m_slot[_i].second);
            m_tag[_i] = SLOT_DELETED;
            ++m_tomb;
            --m_size;
        }

        /// Clear the tombstones once they outnumber the live entries; with the growth bound
        /// this always lands back in the band
        void shrink() {
            if (m_tomb <= m_size) return;
            const size_t want = pow2_at_least(m_size * 4);
            if (want < m_cap) rehash(want);
        }

        /**
         * \brief Probe for _key, then _assign the stored value or _place a fresh entry
         *
         * The answer's second member says whether the entry was created; a growth trigger
         * rehashes before the store, so the returned iterator stays valid.
         */
        template <typename Assign, typename Place>
        std::pair<iterator, bool> insert_impl(const Key& _key, Assign _assign, Place _place) {
            if (!m_cap) rehash(MIN_CAP);
            const uint_64 h = key_hash(_key);
            const uint_8 t = tag_of(h);
            size_t mask = m_cap - 1;
            size_t i = (size_t) h & mask;
            size_t reused = m_cap;
            for (;;) {
                const uint_8 s = m_tag[i];
                if (SLOT_EMPTY == s) break;
                if (s == t && m_slot[i].first == _key) {
                    _assign(m_slot[i].second);
                    return {iterator(this, i), false};
                }
                if (SLOT_DELETED == s && m_cap == reused) reused = i;
                i = (i + 1) & mask;
            }
            if ((m_size + m_tomb + 1) * 4 > m_cap) {
                rehash(m_cap * 2);
                mask = m_cap - 1;
                i = (size_t) h & mask;
                while (SLOT_EMPTY != m_tag[i]) i = (i + 1) & mask;
                reused = m_cap;
            }
            const size_t pos = m_cap != reused ? reused : i;
            _place(m_slot[pos]);
            m_tag[pos] = t;
            if (pos != i) --m_tomb;
            ++m_size;
            return {iterator(this, pos), true};
        }

        /// Allocate a fresh table of _cap slots and replay every entry into it
        void rehash(size_t _cap) {
            uint_8* ptags = new uint_8[_cap]();
            value_type* pslots = nullptr;
            try {
                pslots = new value_type[_cap];
            } catch (...) {
                delete[] ptags;
                throw;
            }
            const size_t mask = _cap - 1;
            for (size_t i = 0; i < m_cap; ++i) {
                if (!live(m_tag[i])) continue;
                const uint_8 t = m_tag[i];
                size_t j = (size_t) key_hash(m_slot[i].first) & mask;
                while (SLOT_EMPTY != ptags[j]) j = (j + 1) & mask;
                pslots[j] = std::move(m_slot[i]);
                ptags[j] = t;
            }
            delete[] m_tag;
            delete[] m_slot;
            m_tag = ptags;
            m_slot = pslots;
            m_cap = _cap;
            m_tomb = 0;
        }

        /// Smallest power-of-two capacity that holds a load of _n / 4
        static size_t pow2_at_least(size_t _n) {
            size_t p = MIN_CAP;
            while (p < _n) p <<= 1;
            return p;
        }

        /// Clone _v slot by slot; the hash is not consulted again
        void clone(const map_read& _v) {
            if (!_v.m_cap) return;
            m_tag = new uint_8[_v.m_cap]();
            try {
                m_slot = new value_type[_v.m_cap];
            } catch (...) {
                delete[] m_tag;
                m_tag = nullptr;
                throw;
            }
            m_cap = _v.m_cap;
            for (size_t i = 0; i < m_cap; ++i) {
                const uint_8 t = _v.m_tag[i];
                if (SLOT_EMPTY == t) continue;
                if (SLOT_DELETED == t) {
                    m_tag[i] = SLOT_DELETED;
                    ++m_tomb;
                    continue;
                }
                copy_entry(m_slot[i], _v.m_slot[i]);
                m_tag[i] = t;
                ++m_size;
            }
        }

        /// Release every entry and the table
        void destroy() {
            for (size_t i = 0; i < m_cap; ++i) {
                if (live(m_tag[i])) drop_value(m_slot[i].second);
            }
            delete[] m_tag;
            delete[] m_slot;
            reset();
        }

        /// Take over the members of _v, leaving it empty
        void steal(map_read& _v) noexcept {
            m_tag = _v.m_tag;
            m_slot = _v.m_slot;
            m_cap = _v.m_cap;
            m_size = _v.m_size;
            m_tomb = _v.m_tomb;
            _v.reset();
        }

        /// Return to the lazy state without touching any entry
        void reset() noexcept {
            m_tag = nullptr;
            m_slot = nullptr;
            m_cap = 0;
            m_size = 0;
            m_tomb = 0;
        }

    private:
        uint_8* m_tag = nullptr;
        value_type* m_slot = nullptr;
        size_t m_cap = 0;
        size_t m_size = 0;
        size_t m_tomb = 0;
    };
}

#undef ALX_AMAP_INL

#endif
