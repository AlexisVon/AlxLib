/*****************************************************************/ /**
 * \file   gt_acache.cpp
 * \brief  Unit tests for cache (FIFO, LRU, LFU, cache_pool)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acache.h"
#include <gtest/gtest.h>

using namespace alx;

static cache* make(const char* _type, uint_64 _cap) {
    cache* c = cache::create(_type, _cap);
    EXPECT_NE(c, nullptr);
    return c;
}

TEST(gt_cache, fifo_put_get) {
    cache* c = make("cache_fifo", 4);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 1);
    EXPECT_TRUE(c->get("b", out));
    EXPECT_EQ(out.to<int>(), 2);
    EXPECT_FALSE(c->get("x", out));
    delete c;
}

TEST(gt_cache, fifo_capacity_eviction) {
    cache* c = make("cache_fifo", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_FALSE(c->get("a", out));
    EXPECT_TRUE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, fifo_put_existing_moves_to_newest) {
    cache* c = make("cache_fifo", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("a", 10);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 10);
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, fifo_remove) {
    cache* c = make("cache_fifo", 3);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);
    c->remove("b");

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, fifo_remove_nonexistent) {
    cache* c = make("cache_fifo", 2);
    c->put("a", 1);
    c->remove("x");
    EXPECT_EQ(c->size(), 1u);
    delete c;
}

TEST(gt_cache, fifo_clear) {
    cache* c = make("cache_fifo", 3);
    c->put("a", 1);
    c->put("b", 2);
    c->clear();

    EXPECT_EQ(c->size(), 0u);
    variant out;
    EXPECT_FALSE(c->get("a", out));
    delete c;
}

TEST(gt_cache, fifo_capacity) {
    cache* c = make("cache_fifo", 10);
    EXPECT_EQ(c->capacity(), 10u);
    delete c;
}

TEST(gt_cache, fifo_type_name) {
    cache* c = make("cache_fifo", 4);
    EXPECT_STREQ(c->type_name(), "cache_fifo");
    delete c;
}

TEST(gt_cache, fifo_copy_entity) {
    cache* c = make("cache_fifo", 2);
    c->put("a", 1);
    c->put("b", 2);

    cache* cp = c->copy_entity();
    ASSERT_NE(cp, nullptr);
    EXPECT_EQ(cp->size(), 2u);
    EXPECT_NE(cp, c);

    variant out;
    EXPECT_TRUE(cp->get("a", out));
    EXPECT_EQ(out.to<int>(), 1);
    EXPECT_TRUE(cp->get("b", out));
    EXPECT_EQ(out.to<int>(), 2);

    cp->put("c", 3);
    EXPECT_EQ(cp->size(), 2u);
    EXPECT_FALSE(cp->get("a", out));
    EXPECT_TRUE(cp->get("b", out));
    EXPECT_TRUE(cp->get("c", out));

    delete cp;
    delete c;
}

TEST(gt_cache, fifo_new_entity) {
    cache* c = make("cache_fifo", 5);
    cache* n = c->new_entity(20);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->capacity(), 20u);
    EXPECT_EQ(n->size(), 0u);
    EXPECT_STREQ(n->type_name(), "cache_fifo");
    delete n;
    delete c;
}

TEST(gt_cache, lru_put_get) {
    cache* c = make("cache_lru", 4);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 1);
    EXPECT_FALSE(c->get("x", out));
    delete c;
}

TEST(gt_cache, lru_get_refreshes_position) {
    cache* c = make("cache_lru", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->get("a", out);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lru_put_existing_refreshes_position) {
    cache* c = make("cache_lru", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("a", 10);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 10);
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lru_evicts_lru_on_full) {
    cache* c = make("cache_lru", 3);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);
    c->get("a", out);
    c->get("c", out);
    c->put("d", 4);

    EXPECT_EQ(c->size(), 3u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    EXPECT_TRUE(c->get("d", out));
    delete c;
}

TEST(gt_cache, lru_remove) {
    cache* c = make("cache_lru", 3);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);
    c->remove("b");

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lru_clear) {
    cache* c = make("cache_lru", 3);
    c->put("a", 1);
    c->put("b", 2);
    c->clear();

    EXPECT_EQ(c->size(), 0u);
    variant out;
    EXPECT_FALSE(c->get("a", out));
    delete c;
}

TEST(gt_cache, lru_type_name) {
    cache* c = make("cache_lru", 4);
    EXPECT_STREQ(c->type_name(), "cache_lru");
    delete c;
}

TEST(gt_cache, lru_copy_entity) {
    cache* c = make("cache_lru", 2);
    c->put("a", 1);
    c->put("b", 2);

    cache* cp = c->copy_entity();
    ASSERT_NE(cp, nullptr);
    EXPECT_EQ(cp->size(), 2u);
    EXPECT_NE(cp, c);

    variant out;
    EXPECT_TRUE(cp->get("a", out));
    EXPECT_EQ(out.to<int>(), 1);

    cp->put("c", 3);
    EXPECT_EQ(cp->size(), 2u);
    EXPECT_TRUE(cp->get("a", out));
    EXPECT_FALSE(cp->get("b", out));
    EXPECT_TRUE(cp->get("c", out));

    delete cp;
    delete c;
}

TEST(gt_cache, lru_new_entity) {
    cache* c = make("cache_lru", 5);
    cache* n = c->new_entity(20);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->capacity(), 20u);
    EXPECT_EQ(n->size(), 0u);
    EXPECT_STREQ(n->type_name(), "cache_lru");
    delete n;
    delete c;
}

TEST(gt_cache, lfu_put_get) {
    cache* c = make("cache_lfu", 4);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 1);
    EXPECT_FALSE(c->get("x", out));
    delete c;
}

TEST(gt_cache, lfu_evicts_least_frequent) {
    cache* c = make("cache_lfu", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->get("b", out);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_FALSE(c->get("a", out));
    EXPECT_TRUE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lfu_evicts_lru_among_tied_freq) {
    cache* c = make("cache_lfu", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);

    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_FALSE(c->get("a", out));
    EXPECT_TRUE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lfu_put_existing_updates_freq) {
    cache* c = make("cache_lfu", 2);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("a", 10);
    c->put("c", 3);

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_EQ(out.to<int>(), 10);
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lfu_freq_accumulates) {
    cache* c = make("cache_lfu", 3);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);
    c->get("a", out);
    c->get("a", out);
    c->get("c", out);
    c->put("d", 4);

    EXPECT_EQ(c->size(), 3u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    EXPECT_TRUE(c->get("d", out));
    delete c;
}

TEST(gt_cache, lfu_remove) {
    cache* c = make("cache_lfu", 3);
    variant out;

    c->put("a", 1);
    c->put("b", 2);
    c->put("c", 3);
    c->remove("b");

    EXPECT_EQ(c->size(), 2u);
    EXPECT_TRUE(c->get("a", out));
    EXPECT_FALSE(c->get("b", out));
    EXPECT_TRUE(c->get("c", out));
    delete c;
}

TEST(gt_cache, lfu_clear) {
    cache* c = make("cache_lfu", 3);
    c->put("a", 1);
    c->put("b", 2);
    c->clear();

    EXPECT_EQ(c->size(), 0u);
    variant out;
    EXPECT_FALSE(c->get("a", out));
    delete c;
}

TEST(gt_cache, lfu_type_name) {
    cache* c = make("cache_lfu", 4);
    EXPECT_STREQ(c->type_name(), "cache_lfu");
    delete c;
}

TEST(gt_cache, lfu_copy_entity) {
    cache* c = make("cache_lfu", 2);
    c->put("a", 1);
    c->put("b", 2);
    variant dummy;
    c->get("b", dummy);

    cache* cp = c->copy_entity();
    ASSERT_NE(cp, nullptr);
    EXPECT_EQ(cp->size(), 2u);

    variant out;
    cp->put("c", 3);
    EXPECT_EQ(cp->size(), 2u);
    EXPECT_FALSE(cp->get("a", out));
    EXPECT_TRUE(cp->get("b", out));
    EXPECT_TRUE(cp->get("c", out));

    delete cp;
    delete c;
}

TEST(gt_cache, lfu_new_entity) {
    cache* c = make("cache_lfu", 5);
    cache* n = c->new_entity(20);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->capacity(), 20u);
    EXPECT_EQ(n->size(), 0u);
    EXPECT_STREQ(n->type_name(), "cache_lfu");
    delete n;
    delete c;
}

TEST(gt_cache, pool_create_entity) {
    auto& pool = *cache_pool::instance();
    EXPECT_TRUE(pool.create_entity("test_fifo", "cache_fifo", 10));
    EXPECT_TRUE(pool.contain_entity("test_fifo"));
    EXPECT_TRUE(pool.create_entity("test_lru", "cache_lru", 8));
    EXPECT_TRUE(pool.contain_entity("test_lru"));
    EXPECT_FALSE(pool.create_entity("bad_type", "nonexistent", 10));

    pool.remove_entity("test_fifo");
    pool.remove_entity("test_lru");
}

TEST(gt_cache, pool_put_get) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool1", "cache_fifo", 10);

    variant out;
    EXPECT_TRUE(pool.put("pool1", "k1", 42));
    EXPECT_TRUE(pool.get("pool1", "k1", out));
    EXPECT_EQ(out.to<int>(), 42);

    EXPECT_FALSE(pool.get("pool1", "nonexistent", out));
    EXPECT_FALSE(pool.get("nonexistent_entity", "k1", out));

    pool.remove_entity("pool1");
}

TEST(gt_cache, pool_get_with_factory) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_fac", "cache_fifo", 10);

    variant out;

    EXPECT_TRUE(pool.get("pool_fac", "new_key", out, []() -> variant { return 99; }));
    EXPECT_EQ(out.to<int>(), 99);

    EXPECT_TRUE(pool.get("pool_fac", "new_key", out));
    EXPECT_EQ(out.to<int>(), 99);

    pool.remove_entity("pool_fac");
}

TEST(gt_cache, pool_remove_key) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_rm", "cache_fifo", 10);

    pool.put("pool_rm", "k1", 1);
    pool.put("pool_rm", "k2", 2);
    pool.remove("pool_rm", "k1");

    variant out;
    EXPECT_FALSE(pool.get("pool_rm", "k1", out));
    EXPECT_TRUE(pool.get("pool_rm", "k2", out));

    pool.remove_entity("pool_rm");
}

TEST(gt_cache, pool_size_and_capacity) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_sc", "cache_lru", 5);

    EXPECT_EQ(pool.size("pool_sc"), 0u);
    EXPECT_EQ(pool.capacity("pool_sc"), 5u);

    pool.put("pool_sc", "a", 1);
    pool.put("pool_sc", "b", 2);
    EXPECT_EQ(pool.size("pool_sc"), 2u);

    pool.remove_entity("pool_sc");
}

TEST(gt_cache, pool_copy_entity) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_cp_src", "cache_fifo", 10);
    pool.put("pool_cp_src", "k1", 100);

    EXPECT_TRUE(pool.copy_entity("pool_cp_src", "pool_cp_dst"));
    EXPECT_TRUE(pool.contain_entity("pool_cp_dst"));

    variant out;
    EXPECT_TRUE(pool.get("pool_cp_dst", "k1", out));
    EXPECT_EQ(out.to<int>(), 100);

    pool.remove_entity("pool_cp_src");
    pool.remove_entity("pool_cp_dst");
}

TEST(gt_cache, pool_new_entity) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_ne_src", "cache_lru", 10);

    EXPECT_TRUE(pool.new_entity("pool_ne_src", "pool_ne_dst", 20));
    EXPECT_TRUE(pool.contain_entity("pool_ne_dst"));
    EXPECT_EQ(pool.capacity("pool_ne_dst"), 20u);
    EXPECT_EQ(pool.size("pool_ne_dst"), 0u);

    pool.remove_entity("pool_ne_src");
    pool.remove_entity("pool_ne_dst");
}

TEST(gt_cache, pool_list_entitys) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_ls_a", "cache_fifo", 4);
    pool.create_entity("pool_ls_b", "cache_lru", 4);

    auto keys = pool.list_entitys();
    EXPECT_GE(std::count(keys.begin(), keys.end(), "pool_ls_a"), 1);
    EXPECT_GE(std::count(keys.begin(), keys.end(), "pool_ls_b"), 1);

    pool.remove_entity("pool_ls_a");
    pool.remove_entity("pool_ls_b");
}

TEST(gt_cache, pool_list_types) {
    auto& pool = *cache_pool::instance();
    auto types = pool.list_types();

    EXPECT_GE(std::count(types.begin(), types.end(), "cache_fifo"), 1);
    EXPECT_GE(std::count(types.begin(), types.end(), "cache_lru"), 1);
}

TEST(gt_cache, pool_clear_entity) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_clr", "cache_fifo", 10);
    pool.put("pool_clr", "a", 1);
    pool.put("pool_clr", "b", 2);

    pool.clear_entity("pool_clr");
    EXPECT_EQ(pool.size("pool_clr"), 0u);

    pool.remove_entity("pool_clr");
}

TEST(gt_cache, pool_remove_entity) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_re", "cache_fifo", 10);

    EXPECT_TRUE(pool.contain_entity("pool_re"));
    pool.remove_entity("pool_re");
    EXPECT_FALSE(pool.contain_entity("pool_re"));
}

TEST(gt_cache, pool_duplicate_create_overwrites) {
    auto& pool = *cache_pool::instance();
    pool.create_entity("pool_dup", "cache_fifo", 10);
    pool.put("pool_dup", "k1", 1);

    EXPECT_TRUE(pool.create_entity("pool_dup", "cache_lru", 20));
    EXPECT_EQ(pool.capacity("pool_dup"), 20u);
    EXPECT_EQ(pool.size("pool_dup"), 0u);

    pool.remove_entity("pool_dup");
}
