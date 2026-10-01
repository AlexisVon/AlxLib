/*****************************************************************/ /**
 * \file   alogger.cpp
 * \brief  Logging system (supports multiple log levels)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "alogger.h"
#include "adatetime.h"
#include "afactory.h"
#include "afile.h"
#include "aplatform.h"
#include "autility.h"

#include <atomic>
#include <iostream>

using namespace alx;

const std::unordered_map<std::string, log_level> log_level_str2value{
    {"info", log_level::info},
    {"debug", log_level::debug},
    {"warn", log_level::warn},
    {"error", log_level::error},
    {"crash", log_level::crash}};
// the trailing spaces are the five-character field width, not stray whitespace
const std::unordered_map<log_level, std::string> log_level_value2str{
    {log_level::info, "info "},
    {log_level::debug, "debug"},
    {log_level::warn, "warn "},
    {log_level::error, "error"},
    {log_level::crash, "crash"}};

class alx::log_property {
public:
    log_property(const json_object& _config)
        : prt_time(_config.value("prt_time").to_bool()), prt_thrd(_config.value("prt_thrd").to_bool()), prt_level(_config.value("prt_level").to_bool()), prt_head(_config.value("prt_head").to_bool()) {
    }

public:
    bool prt_time{false};
    bool prt_thrd{false};
    bool prt_level{false};
    bool prt_head{false};
};

class alx::log_appender {
public:
    log_appender(const json_object& _config) : __level__(alx::map_value(log_level_str2value, _config.value("level").to_string(), log_level::info)) {}
    virtual ~log_appender() {}
    virtual void append(const std::string& _mesg) = 0;
    const log_level __level__{log_level::info};
};

typedef factory<log_appender, const json_object&> log_appender_factory;
template <typename T>
using log_appender_product = product<T, log_appender, const json_object&>;

logger::logger(const json_object& _config)
    : m_property(new log_property(_config)) {
    for (const auto& it : _config) {
        if (!it.second->is_object()) continue;
        const json_object& obj = it.second->to_object();
        std::string type = "log_appender_" + obj.value("type").to_string();
        if (log_appender_factory::enable(type)) {
            log_appender* appender = log_appender_factory::create(type, obj);
            if (appender != nullptr) m_appenders.push_back(appender);
        }
    }
}

logger::~logger() {
    for (auto appender : m_appenders) delete appender;
    delete m_property;
}

std::vector<std::string> alx::logger::list() {
    std::vector<std::string> result;
    for (auto it : log_appender_factory::type_map()) result.push_back(it.first);
    return result;
}

#define PRINT_PROPERTY(PRINT, VALUE) PRINT.push_back('['), PRINT.append(VALUE), PRINT.push_back(']')

void logger::log(const std::string& _mesg, log_level _level) {
    std::string print;
    if (m_property->prt_time) PRINT_PROPERTY(print, datetime::current().to_string());
    if (m_property->prt_level) PRINT_PROPERTY(print, alx::map_value(log_level_value2str, _level, std::string("nolvl")));
    if (m_property->prt_thrd) PRINT_PROPERTY(print, std::to_string(alx::this_tid() & 0XFFFFU));
    if (m_property->prt_head) print.push_back('\n');
    print.append(_mesg);
    print.push_back('\n');
    for (auto appender : m_appenders)
        if (_level >= appender->__level__) appender->append(print);
}

// global scope is load-bearing: "log_appender_" + the config's type must equal this demangled name
class log_appender_cout
    : public log_appender_product<log_appender_cout> {
public:
    log_appender_cout(const json_object& _config)
        : log_appender_product<log_appender_cout>(_config), is_flush(_config.value("is_flush").to_bool()) {}
    ~log_appender_cout() {}

public:
    virtual void append(const std::string& _mesg) override {
        std::cout << _mesg;
        if (is_flush) std::cout << std::flush;
    }

private:
    bool is_flush{false};
};

#ifdef _WIN32
class log_appender_win32
    : public log_appender_product<log_appender_win32> {
public:
    log_appender_win32(const json_object& _config)
        : log_appender_product<log_appender_win32>(_config) {}
    ~log_appender_win32() {}

public:
    virtual void append(const std::string& _mesg) override { OutputDebugStringA(_mesg.c_str()); }
};
#endif

class log_appender_file
    : public log_appender_product<log_appender_file> {
public:
    log_appender_file(const json_object& _config)
        : log_appender_product<log_appender_file>(_config), is_flush(_config.value("is_flush").to_bool()), is_async(_config.value("is_async").to_bool()), queue_limit(_config.value("queue_limit").to_intg()) {
        // a zero limit would block every producer until the appender dies, so 0 takes the default
        if (0 == queue_limit) queue_limit = 1024;
        out_file.open(file_info(_config.value("file_path").to_string()),
                      _config.value("is_append").to_bool() ? file::APED : file::WRIT);
        // failed open: is_stop marks the appender dead for good, so append() drops every record
        if (!out_file.is_open()) {
            is_stop = true;
            return;
        }
        if (is_async) out_worker = new std::thread(&log_appender_file::async_handle, this);
    }
    ~log_appender_file() {
        // atomic, because append() tests it without taking the queue lock
        is_stop = true;
        cond_queue_in.notify_all();
        cond_queue_out.notify_all();
        if (nullptr != out_worker) {
            out_worker->join();
            delete out_worker;
        }
        out_file.close();
    }

public:
    virtual void append(const std::string& _mesg) override {
        if (is_stop) return;
        if (is_async && nullptr != out_worker) {
            std::unique_lock<std::mutex> lock(out_mutex);
            if (out_queue.size() >= queue_limit)
                cond_queue_in.wait(lock, [this] { return out_queue.size() < queue_limit || is_stop; });
            if (!is_stop) {
                out_queue.push_back(_mesg);
                cond_queue_out.notify_one();
            }
        } else {
            log(_mesg);
        }
    }

private:
    void log(const std::string& _mesg) {
        out_file.write(_mesg);
        if (is_flush) out_file.flush();
    }
    static void async_handle(log_appender_file* _obj) {
        while (!_obj->is_stop) {
            std::string mesg;
            {
                std::unique_lock<std::mutex> lock(_obj->out_mutex);
                _obj->cond_queue_out.wait(lock, [_obj] { return _obj->is_stop || !_obj->out_queue.empty(); });
                if (_obj->is_stop) break;
                else {
                    mesg = _obj->out_queue.front();
                    _obj->out_queue.pop_front();
                    _obj->cond_queue_in.notify_one();
                }
            }
            _obj->log(mesg);
        }

        // the tail is drained too, and the swap takes the lock: a producer may still be pushing here
        {
            std::list<std::string> last;
            {
                std::unique_lock<std::mutex> lock(_obj->out_mutex);
                last.swap(_obj->out_queue);
            }
            size_t total_size = 0;
            std::string mesg;
            for (const std::string& it : last) total_size += it.length();
            mesg.reserve(total_size);
            for (const std::string& it : last) mesg.append(it);
            _obj->log(mesg);
        }
    }

private:
    bool is_flush{false};
    bool is_async{false};
    std::atomic<bool> is_stop{false};
    uint_64 queue_limit{1024};
    file out_file;
    std::mutex out_mutex;
    std::list<std::string> out_queue;
    std::thread* out_worker{nullptr};
    std::condition_variable cond_queue_in;
    std::condition_variable cond_queue_out;
};
