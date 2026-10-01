/*****************************************************************/ /**
 * \file   asqlite.cpp
 * \brief  SQLite3 database wrapper
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "asqlite.h"

#include <sqlite3.h>

using namespace alx;

#define DB_PTR ((sqlite3*) db)
#define DB_PPTR ((sqlite3**) &db)

bool sqlite::open(const std::string& _db, bool _readonly) {
    if (SQLITE_OK != sqlite3_open_v2(_db.c_str(), DB_PPTR,
                                     _readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr)) {
        // open_v2 stores a handle even on failure, so that one is closed here and db cleared for the check below
        sqlite3_close(DB_PTR);
        db = nullptr;
    }
    return nullptr != db;
}

void sqlite::close() {
    if (nullptr != db) {
        sqlite3_close_v2(DB_PTR);
        db = nullptr;
    }
}

const char* alx::sqlite::last_error() {
    // driver-owned text: the next call on this connection overwrites it
    return sqlite3_errmsg(DB_PTR);
}

bool alx::sqlite::isin_transaction() {
    return sqlite3_get_autocommit(DB_PTR) == 0;
}

bool alx::sqlite::start_transaction() {
    sqlite3_mutex_enter(sqlite3_db_mutex(DB_PTR));
    return exec("BEGIN;");
}

bool alx::sqlite::commit_transaction() {
    bool result = exec("COMMIT;");
    sqlite3_mutex_leave(sqlite3_db_mutex(DB_PTR));
    return result;
}

bool alx::sqlite::rollback_transaction() {
    bool result = exec("ROLLBACK;");
    sqlite3_mutex_leave(sqlite3_db_mutex(DB_PTR));
    return result;
}

bool sqlite::exec(const std::string& _sql) {
    return sqlite3_exec(DB_PTR, _sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool sqlite::exec(const std::string& _sql, result& _rst) {
    return sqlite3_exec(DB_PTR, _sql.c_str(), &sqlite::exec_callback, &_rst, nullptr) == SQLITE_OK;
}

sqlite::stmt sqlite::prepare(const std::string& _sql, uint_8 _flag) {
    sqlite3_stmt* rst{nullptr};
    const char* ptr{nullptr};
    sqlite3_prepare_v3(DB_PTR, _sql.c_str(), (int) _sql.length(),
                       ((_flag & prepare_flag::persistent) ? SQLITE_PREPARE_PERSISTENT : 0X00U) |
                           ((_flag & prepare_flag::normalize) ? SQLITE_PREPARE_NORMALIZE : 0X00U) |
                           ((_flag & prepare_flag::novtable) ? SQLITE_PREPARE_NO_VTAB : 0X00U) |
                           ((_flag & prepare_flag::dontlog) ? SQLITE_PREPARE_DONT_LOG : 0X00U),
                       &rst, &ptr);
    return stmt(this, rst);
}

int sqlite::exec_callback(void* _rst, int _cols, char** _content, char** _header) {
    result& rst = r_interpret<result>(_rst);
    std::vector<token> line;
    for (int i = 0; i < _cols; i++) {
        line.push_back(token(_header[i], _content[i]));
    }
    rst.push_back(std::move(line));
    // zero tells the driver to keep going; a non-zero return would abort the query
    return 0;
}

#define STMT_PTR ((sqlite3_stmt*) stmt_)
#define STMT_PPTR ((sqlite3_stmt**) &stmt_)

sqlite::stmt::~stmt() {
    finalize();
}

void sqlite::stmt::finalize() {
    sqlite3_finalize(STMT_PTR);
    stmt_ = nullptr;
}

bool alx::sqlite::stmt::bind(int _idx) {
    return sqlite3_bind_null(STMT_PTR, _idx) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, int _val) {
    return sqlite3_bind_int(STMT_PTR, _idx, _val) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, double _val) {
    return sqlite3_bind_double(STMT_PTR, _idx, _val) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, long long _val) {
    return sqlite3_bind_int64(STMT_PTR, _idx, _val) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, bool _val) {
    return sqlite3_bind_int(STMT_PTR, _idx, _val ? 1 : 0) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, const char* _val) {
    return sqlite3_bind_text(STMT_PTR, _idx, _val, -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, const std::string& _val) {
    return sqlite3_bind_text(STMT_PTR, _idx, _val.c_str(), (int) _val.length(), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, const bytes_view& _val) {
    // above int_32 bytes the int-sized entry point would take a negative length, hence the 64-bit twin
    return (_val.size() <= max_int_32 ? sqlite3_bind_blob(STMT_PTR, _idx, _val.cdata(), (int) _val.size(), SQLITE_TRANSIENT) : sqlite3_bind_blob64(STMT_PTR, _idx, _val.cdata(), _val.size(), SQLITE_TRANSIENT)) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind(int _idx, const void* _ptr, const uint_64 _len, void (*_del_func)(void*)) {
    return (_len <= max_int_32 ? sqlite3_bind_blob(STMT_PTR, _idx, _ptr, (int) _len, _del_func) : sqlite3_bind_blob64(STMT_PTR, _idx, _ptr, _len, _del_func)) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind_zeroblob(int _idx, uint_64 _size) {
    return (_size <= max_int_32 ? sqlite3_bind_zeroblob(STMT_PTR, _idx, (int) _size) : sqlite3_bind_zeroblob64(STMT_PTR, _idx, _size)) == SQLITE_OK;
}

bool alx::sqlite::stmt::bind_clear() {
    return sqlite3_clear_bindings(STMT_PTR) == SQLITE_OK;
}

int alx::sqlite::stmt::bind_count() {
    return sqlite3_bind_parameter_count(STMT_PTR);
}

const char* alx::sqlite::stmt::bind_name(int _idx) {
    return sqlite3_bind_parameter_name(STMT_PTR, _idx);
}

int alx::sqlite::stmt::bind_index(const char* _key) {
    return sqlite3_bind_parameter_index(STMT_PTR, _key);
}

sqlite::stmt::state alx::sqlite::stmt::exec() {
    switch (sqlite3_step(STMT_PTR)) {
    case SQLITE_DONE: return done;
    case SQLITE_ROW: return data;
    default: return fail;
    }
}

bool alx::sqlite::stmt::reset() {
    return sqlite3_reset(STMT_PTR) == SQLITE_OK;
}

int alx::sqlite::stmt::column_count() {
    return sqlite3_column_count(STMT_PTR);
}

std::string alx::sqlite::stmt::column_name(int _idx) {
    return sqlite3_column_name(STMT_PTR, _idx);
}

int alx::sqlite::stmt::data_count() {
    return sqlite3_data_count(STMT_PTR);
}

sqlite::stmt::type alx::sqlite::stmt::data_type(int _idx) {
    switch (sqlite3_column_type(STMT_PTR, _idx)) {
    case SQLITE_NULL: return _null_;
    case SQLITE_INTEGER: return _intg_;
    case SQLITE_FLOAT: return _real_;
    case SQLITE_TEXT: return _text_;
    case SQLITE_BLOB: return _blob_;
    default: return _null_;
    }
}

variant alx::sqlite::stmt::get_variant(int _idx) {
    switch (data_type(_idx)) {
    case sqlite::stmt::type::_intg_: return get_intg(_idx);
    case sqlite::stmt::type::_real_: return get_real(_idx);
    case sqlite::stmt::type::_text_: return get_text(_idx);
    case sqlite::stmt::type::_blob_: return get_blob(_idx);
    case sqlite::stmt::type::_null_:
    default: return variant();
    }
}

json_value alx::sqlite::stmt::get_jsonval(int _idx) {
    switch (data_type(_idx)) {
    case sqlite::stmt::type::_intg_: return get_intg(_idx);
    case sqlite::stmt::type::_real_: return get_real(_idx);
    case sqlite::stmt::type::_text_: return get_text(_idx);
    case sqlite::stmt::type::_blob_: return get_blob(_idx);
    case sqlite::stmt::type::_null_:
    default: return json_value();
    }
}

long long alx::sqlite::stmt::get_intg(int _idx) {
    return sqlite3_column_int64(STMT_PTR, _idx);
}

double alx::sqlite::stmt::get_real(int _idx) {
    return sqlite3_column_double(STMT_PTR, _idx);
}

std::string alx::sqlite::stmt::get_text(int _idx) {
    const unsigned char* text = sqlite3_column_text(STMT_PTR, _idx);
    if (nullptr == text) return std::string();
    return std::string((const char*) text, (size_t) sqlite3_column_bytes(STMT_PTR, _idx));
}

bytes alx::sqlite::stmt::get_blob(int _idx) {
    return bytes(sqlite3_column_blob(STMT_PTR, _idx), sqlite3_column_bytes(STMT_PTR, _idx));
}
