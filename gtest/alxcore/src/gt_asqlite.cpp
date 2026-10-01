/*****************************************************************/ /**
 * \file   gt_asqlite.cpp
 * \brief  Unit tests for SQLite3 wrapper (in-memory database)
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "asqlite.h"
#ifdef __linux__
#    include <dirent.h>
#endif
#include <cstdio>
#include <gtest/gtest.h>

using namespace alx;

#ifdef __linux__
namespace {

    size_t count_open_fds() {
        size_t cnt = 0;
        DIR* dir = opendir("/proc/self/fd");
        if (nullptr == dir) return 0;
        while (nullptr != readdir(dir)) cnt++;
        closedir(dir);
        return cnt;
    }
}
#endif

TEST(gt_asqlite, open_memory) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    EXPECT_TRUE(db.is_open());
    db.close();
    EXPECT_FALSE(db.is_open());
}

TEST(gt_asqlite, exec_create_insert_select) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));

    ASSERT_TRUE(db.exec("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)"));
    ASSERT_TRUE(db.exec("INSERT INTO test (name) VALUES ('alice')"));
    ASSERT_TRUE(db.exec("INSERT INTO test (name) VALUES ('bob')"));

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT * FROM test ORDER BY id", rst));

    ASSERT_EQ(rst.size(), 2);
    auto it = rst.begin();
    EXPECT_EQ(it->front().value(), "1");
    EXPECT_EQ((++it)->front().value(), "2");
}

TEST(gt_asqlite, transaction_commit) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE t (val INTEGER)"));

    ASSERT_TRUE(db.start_transaction());
    ASSERT_TRUE(db.isin_transaction());
    ASSERT_TRUE(db.exec("INSERT INTO t VALUES (42)"));
    ASSERT_TRUE(db.commit_transaction());
    EXPECT_FALSE(db.isin_transaction());

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT val FROM t", rst));
    ASSERT_EQ(rst.size(), 1);
    EXPECT_EQ(rst.front().front().value(), "42");
}

TEST(gt_asqlite, transaction_rollback) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE t (val INTEGER)"));

    ASSERT_TRUE(db.start_transaction());
    ASSERT_TRUE(db.exec("INSERT INTO t VALUES (99)"));
    ASSERT_TRUE(db.rollback_transaction());

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT COUNT(*) FROM t", rst));
    EXPECT_EQ(rst.front().front().value(), "0");
}

TEST(gt_asqlite, prepared_stmt_bind) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL)"));
    ASSERT_TRUE(db.exec("INSERT INTO items (name, price) VALUES ('widget', 9.99)"));

    sqlite::stmt st = db.prepare("INSERT INTO items (name, price) VALUES (?, ?)");
    ASSERT_TRUE(st.valid());
    ASSERT_TRUE(st.bind(1, "gadget"));
    ASSERT_TRUE(st.bind(2, 19.99));
    ASSERT_EQ(st.exec(), sqlite::stmt::done);

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT name, price FROM items WHERE price > 10", rst));
    ASSERT_EQ(rst.size(), 1);
}

TEST(gt_asqlite, exec_invalid_sql) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    EXPECT_FALSE(db.exec("THIS IS NOT VALID SQL"));
    EXPECT_NE(db.last_error(), nullptr);
}

TEST(gt_asqlite, stmt_bind_blob) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE blobs (id INTEGER PRIMARY KEY, data BLOB)"));

    sqlite::stmt st = db.prepare("INSERT INTO blobs (data) VALUES (?)");
    ASSERT_TRUE(st.valid());
    uint_8 blob_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    ASSERT_TRUE(st.bind(1, blob_data, sizeof(blob_data), nullptr));
    ASSERT_EQ(st.exec(), sqlite::stmt::done);

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT data FROM blobs", rst));
    ASSERT_EQ(rst.size(), 1);
}

TEST(gt_asqlite, stmt_bind_null) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE t (val TEXT)"));
    ASSERT_TRUE(db.exec("INSERT INTO t VALUES ('exists')"));

    sqlite::stmt st = db.prepare("INSERT INTO t VALUES (?)");
    ASSERT_TRUE(st.valid());
    ASSERT_TRUE(st.bind(1));
    ASSERT_EQ(st.exec(), sqlite::stmt::done);
}

TEST(gt_asqlite, stmt_column_access) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE cols (a INTEGER, b TEXT, c REAL)"));
    ASSERT_TRUE(db.exec("INSERT INTO cols VALUES (1, 'hello', 3.14)"));

    sqlite::stmt st = db.prepare("SELECT a, b, c FROM cols");
    ASSERT_TRUE(st.valid());
    ASSERT_EQ(st.exec(), sqlite::stmt::data);
    EXPECT_EQ(st.column_count(), 3);
    EXPECT_EQ(st.column_name(0), "a");
    EXPECT_EQ(st.column_name(1), "b");
    EXPECT_EQ(st.column_name(2), "c");
    EXPECT_EQ(st.data_count(), 3);
}

TEST(gt_asqlite, stmt_get_intg_text_real) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE types (i INTEGER, t TEXT, r REAL)"));
    ASSERT_TRUE(db.exec("INSERT INTO types VALUES (42, 'world', 2.718)"));

    sqlite::stmt st = db.prepare("SELECT i, t, r FROM types");
    ASSERT_TRUE(st.valid());
    ASSERT_EQ(st.exec(), sqlite::stmt::data);
    EXPECT_EQ(st.get_intg(0), 42);
    EXPECT_EQ(st.get_text(1), "world");
    EXPECT_DOUBLE_EQ(st.get_real(2), 2.718);
}

TEST(gt_asqlite, stmt_reset_reuse) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE counter (n INTEGER)"));

    sqlite::stmt st = db.prepare("INSERT INTO counter VALUES (?)");
    ASSERT_TRUE(st.valid());

    st.bind(1, 100);
    ASSERT_EQ(st.exec(), sqlite::stmt::done);

    ASSERT_TRUE(st.reset());
    st.bind(1, 200);
    ASSERT_EQ(st.exec(), sqlite::stmt::done);

    sqlite::result rst;
    ASSERT_TRUE(db.exec("SELECT n FROM counter ORDER BY n", rst));
    ASSERT_EQ(rst.size(), 2);
}

TEST(gt_asqlite, move_construct) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE m (x INTEGER)"));

    sqlite db2(std::move(db));
    EXPECT_FALSE(db.is_open());
    EXPECT_TRUE(db2.is_open());
    ASSERT_TRUE(db2.exec("INSERT INTO m VALUES (1)"));
}

TEST(gt_asqlite, stmt_get_text_null_column) {
    sqlite db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.exec("CREATE TABLE n (a)"));
    ASSERT_TRUE(db.exec("INSERT INTO n VALUES (NULL)"));

    sqlite::stmt st = db.prepare("SELECT a FROM n");
    ASSERT_TRUE(st.valid());
    ASSERT_EQ(st.exec(), sqlite::stmt::data);
    EXPECT_EQ(st.data_type(0), sqlite::stmt::type::_null_);
    EXPECT_TRUE(st.get_text(0).empty());
}

TEST(gt_asqlite, close_releases_connection_with_live_stmt) {
#ifndef __linux__
    GTEST_SKIP() << "fd counting requires /proc";
#else
    const std::string path = "tmp-alxcore-gtest/alx_gt_sqlite_close.db";
    std::remove(path.c_str());

    {
        sqlite warm;
        ASSERT_TRUE(warm.open(path));
        ASSERT_TRUE(warm.exec("CREATE TABLE t (v INTEGER)"));
    }

    const size_t before = count_open_fds();
    {
        sqlite db;
        ASSERT_TRUE(db.open(path));
        ASSERT_TRUE(db.exec("INSERT INTO t VALUES (1)"));
        sqlite::stmt st = db.prepare("SELECT v FROM t");
        ASSERT_TRUE(st.valid());

        db.close();
        EXPECT_FALSE(db.is_open());
    }
    EXPECT_EQ(count_open_fds(), before);

    std::remove(path.c_str());
#endif
}
