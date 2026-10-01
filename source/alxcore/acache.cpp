/*****************************************************************/ /**
 * \file   acache.cpp
 * \brief  Cache management (LRU and other caching strategies)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acache.h"
#include "afactory.h"

using namespace alx;

typedef factory<cache, uint_64> cache_factory;
template <typename IMPL>
using cache_product = product<IMPL, cache, uint_64>;

cache* alx::cache::create(const std::string& _type, uint_64 _capacity) {
    return cache_factory::create(_type, _capacity);
}

// Nothing to release here: pool is a safe_map, whose destructor deletes every entity it owns.
alx::cache_pool::~cache_pool() {
}

bool alx::cache_pool::create_entity(const std::string& _key, const std::string& _type, uint_64 _capacity) {
    cache* entity = cache_factory::create(_type, _capacity);
    if (nullptr != entity) pool.insert(_key, entity);
    return nullptr != entity;
}

bool alx::cache_pool::copy_entity(const std::string& _key, const std::string& _new) {
    cache* entity{nullptr};
    // The lock dies with this block: insert() below takes the write lock of this same pool.
    {
        R_THREAD_SAFE(pool);
        auto iter = pool.data().find(_key);
        if (iter == pool.data().end()) return false;
        entity = iter->second->copy_entity();
    }
    pool.insert(_new, entity);
    return true;
}

void alx::cache_pool::add_entity(const std::string& _key, cache* _entity) {
    return pool.insert(_key, _entity);
}

bool alx::cache_pool::new_entity(const std::string& _key, const std::string& _new, uint_64 _capacity) {
    cache* entity{nullptr};
    // Block-scoped read lock: insert() below takes the write lock of this same pool.
    {
        R_THREAD_SAFE(pool);
        auto iter = pool.data().find(_key);
        if (iter == pool.data().end()) return false;
        entity = iter->second->new_entity(_capacity);
    }
    pool.insert(_new, entity);
    return true;
}

bool alx::cache_pool::contain_entity(const std::string& _key) const {
    return pool.contain(_key);
}

void alx::cache_pool::remove_entity(const std::string& _key) {
    return pool.remove(_key);
}

void alx::cache_pool::clear_entity(const std::string& _key) {
    return pool.call(_key, &cache::clear);
}

std::list<std::string> alx::cache_pool::list_entitys() const {
    return pool.keys();
}

std::list<std::string> alx::cache_pool::list_types() const {
    std::list<std::string> result;
    for (const auto& it : cache_factory::type_map()) result.push_back(it.first);
    return result;
}

bool alx::cache_pool::put(const std::string& _entity, const std::string& _key, const variant& _value) {
    bool success{false};
    pool.call<void, const std::string&, const variant&>(_entity, success, &cache::put, _key, _value);
    return success;
}

bool alx::cache_pool::get(const std::string& _entity, const std::string& _key, variant& _out, std::function<variant()> _new) {
    R_THREAD_SAFE(pool);
    auto it = pool.data().find(_entity);
    if (it == pool.data().end()) return false;
    if (it->second->get(_key, _out)) return true;
    else return nullptr == _new ? false : (it->second->put(_key, _out = _new()), true);
}

void alx::cache_pool::remove(const std::string& _entity, const std::string& _key) {
    return pool.call<void, const std::string&>(_entity, &cache::remove, _key);
}

uint_64 alx::cache_pool::size(const std::string& _entity) const {
    return pool.call<uint_64>(_entity, &cache::size);
}

uint_64 alx::cache_pool::capacity(const std::string& _entity) const {
    return pool.call<uint_64>(_entity, &cache::capacity);
}

class cache_fifo
    : public cache_product<cache_fifo>,
      public thread_safe_mutex<void> {
public:
    cache_fifo(uint_64 _size) : cache_product<cache_fifo>(_size) {}
    virtual ~cache_fifo() = default;

public:
    virtual void put(const std::string& _key, const variant& _value) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            m_fifo.erase(it->second.fifo_iter);
            m_fifo.push_back(_key);
            it->second = {_value, --m_fifo.end()};
        } else {
            if (m_cache.size() >= m_capacity) {
                std::string last = m_fifo.front();
                m_fifo.pop_front();
                m_cache.erase(last);
            }
            m_fifo.push_back(_key);
            m_cache.emplace(_key, cache_entry{_value, --m_fifo.end()});
        }
    }
    virtual bool get(const std::string& _key, variant& _out) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it == m_cache.end()) return false;
        _out = it->second.value;
        return true;
    }
    virtual void remove(const std::string& _key) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            m_fifo.erase(it->second.fifo_iter);
            m_cache.erase(it);
        }
    }
    virtual void clear() override {
        THREAD_SAFE(*this);
        m_fifo.clear();
        m_cache.clear();
    }
    virtual uint_64 size() const override { return m_cache.size(); }

public:
    virtual const char* type_name() const override { return "cache_fifo"; }
    virtual cache* copy_entity() const override {
        THREAD_SAFE(*this);
        cache_fifo* entity = new cache_fifo(m_capacity);
        entity->m_cache = m_cache;
        entity->m_fifo = m_fifo;
        entity->fix_iter();
        return entity;
    }
    virtual cache* new_entity(uint_64 _capacity) const override { return new cache_fifo(_capacity); }

private:
    struct cache_entry {
        variant value;
        std::list<std::string>::iterator fifo_iter;
    };
    // The copied entries carry iterators into the source's list; re-point them into this one.
    void fix_iter() {
        for (std::list<std::string>::iterator iter = m_fifo.begin(); iter != m_fifo.end();) {
            auto it = m_cache.find(*iter);
            if (it != m_cache.end()) {
                it->second.fifo_iter = iter;
                ++iter;
            } else iter = m_fifo.erase(iter);
        }
    }
    std::list<std::string> m_fifo;
    std::unordered_map<std::string, cache_entry> m_cache;
};

class cache_lru
    : public cache_product<cache_lru>,
      public thread_safe_mutex<void> {
public:
    cache_lru(uint_64 _size) : cache_product<cache_lru>(_size) {}
    virtual ~cache_lru() = default;

public:
    virtual void put(const std::string& _key, const variant& _value) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            m_lru.erase(it->second.lru_iter);
            m_lru.push_front(_key);
            it->second = {_value, m_lru.begin()};
        } else {
            if (m_cache.size() >= m_capacity) {
                std::string last = m_lru.back();
                m_lru.pop_back();
                m_cache.erase(last);
            }
            m_lru.push_front(_key);
            m_cache.emplace(_key, cache_entry{_value, m_lru.begin()});
        }
    }
    virtual bool get(const std::string& _key, variant& _out) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it == m_cache.end()) return false;

        m_lru.erase(it->second.lru_iter);
        m_lru.push_front(_key);
        it->second.lru_iter = m_lru.begin();

        _out = it->second.value;
        return true;
    }
    virtual void remove(const std::string& _key) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            m_lru.erase(it->second.lru_iter);
            m_cache.erase(it);
        }
    }
    virtual void clear() override {
        THREAD_SAFE(*this);
        m_lru.clear();
        m_cache.clear();
    }
    virtual uint_64 size() const override { return m_cache.size(); }

public:
    virtual const char* type_name() const override { return "cache_lru"; }
    virtual cache* copy_entity() const override {
        THREAD_SAFE(*this);
        cache_lru* entity = new cache_lru(m_capacity);
        entity->m_cache = m_cache;
        entity->m_lru = m_lru;
        entity->fix_iter();
        return entity;
    }
    virtual cache* new_entity(uint_64 _capacity) const override { return new cache_lru(_capacity); }

private:
    struct cache_entry {
        variant value;
        std::list<std::string>::iterator lru_iter;
    };
    // The copied entries carry iterators into the source's list; re-point them into this one.
    void fix_iter() {
        for (std::list<std::string>::iterator iter = m_lru.begin(); iter != m_lru.end();) {
            auto it = m_cache.find(*iter);
            if (it != m_cache.end()) {
                it->second.lru_iter = iter;
                ++iter;
            } else iter = m_lru.erase(iter);
        }
    }
    std::list<std::string> m_lru;
    std::unordered_map<std::string, cache_entry> m_cache;
};

class cache_lfu
    : public cache_product<cache_lfu>,
      public thread_safe_mutex<void> {
public:
    cache_lfu(uint_64 _size) : cache_product<cache_lfu>(_size) {}
    virtual ~cache_lfu() = default;

public:
    virtual void put(const std::string& _key, const variant& _value) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            it->second.value = _value;
            freq_up(_key, it->second);
        } else {
            if (m_cache.size() >= m_capacity) {
                auto& min_list = m_freq_map[m_min_freq];
                std::string last = min_list.back();
                min_list.pop_back();
                if (min_list.empty()) m_freq_map.erase(m_min_freq);
                m_cache.erase(last);
            }
            m_min_freq = 1;
            m_freq_map[1].push_front(_key);
            m_cache.emplace(_key, cache_entry(_value, 1, m_freq_map[1].begin()));
        }
    }
    virtual bool get(const std::string& _key, variant& _out) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it == m_cache.end()) return false;
        _out = it->second.value;
        freq_up(_key, it->second);
        return true;
    }
    virtual void remove(const std::string& _key) override {
        THREAD_SAFE(*this);
        auto it = m_cache.find(_key);
        if (it != m_cache.end()) {
            auto& flist = m_freq_map[it->second.freq];
            flist.erase(it->second.freq_iter);
            if (flist.empty()) {
                m_freq_map.erase(it->second.freq);
                if (it->second.freq == m_min_freq) update_min_freq();
            }
            m_cache.erase(it);
        }
    }
    virtual void clear() override {
        THREAD_SAFE(*this);
        m_freq_map.clear();
        m_cache.clear();
        m_min_freq = 0;
    }
    virtual uint_64 size() const override { return m_cache.size(); }

public:
    virtual const char* type_name() const override { return "cache_lfu"; }
    virtual cache* copy_entity() const override {
        THREAD_SAFE(*this);
        cache_lfu* entity = new cache_lfu(m_capacity);
        entity->m_freq_map = m_freq_map;
        entity->m_cache = m_cache;
        entity->m_min_freq = m_min_freq;
        entity->fix_iter();
        return entity;
    }
    virtual cache* new_entity(uint_64 _capacity) const override { return new cache_lfu(_capacity); }

private:
    struct cache_entry {
        variant value;
        uint_64 freq{1};
        std::list<std::string>::iterator freq_iter;

        cache_entry(const variant& v, uint_64 f, std::list<std::string>::iterator it)
            : value(v), freq(f), freq_iter(it) {}
    };

    void freq_up(const std::string& _key, cache_entry& _entry) {
        auto& old_list = m_freq_map[_entry.freq];
        old_list.erase(_entry.freq_iter);
        if (old_list.empty()) {
            m_freq_map.erase(_entry.freq);
            // The entry itself moves up to freq+1, so the emptied floor rises by exactly one.
            if (_entry.freq == m_min_freq) ++m_min_freq;
        }
        ++_entry.freq;
        auto& new_list = m_freq_map[_entry.freq];
        new_list.push_front(_key);
        _entry.freq_iter = new_list.begin();
    }
    // The copied entries carry iterators into the source's freq lists; re-point them into these.
    void fix_iter() {
        for (auto& pair : m_freq_map) {
            auto& flist = pair.second;
            for (auto iter = flist.begin(); iter != flist.end();) {
                auto it = m_cache.find(*iter);
                if (it != m_cache.end()) {
                    it->second.freq_iter = iter;
                    ++iter;
                } else iter = flist.erase(iter);
            }
        }
    }
    // remove() retires the key instead of promoting it, so the floor has to be rescanned.
    void update_min_freq() {
        m_min_freq = 0;
        for (const auto& pair : m_freq_map) {
            auto& freq = pair.first;
            auto& flist = pair.second;
            if (!flist.empty() && (0 == m_min_freq || freq < m_min_freq)) m_min_freq = freq;
        }
    }
    // 0 means an empty cache: a live entry's frequency is always at least 1.
    uint_64 m_min_freq{0};
    std::map<uint_64, std::list<std::string>> m_freq_map;
    std::unordered_map<std::string, cache_entry> m_cache;
};
