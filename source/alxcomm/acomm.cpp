/*****************************************************************/ /**
 * \file   acomm.cpp
 * \brief  Communication layer on top of transmit (worker thread + message queue)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acomm.h"
#include "aplatform.h"
#include "astring.h"

#include <functional>

using namespace alx;

alx::comm::comm(transmit* _trans, const uint_64 _queue_limit)
    : m_trans(_trans), m_queue_limit(_queue_limit) {
    if (!is_valid()) return;
    m_trans->trans_mesg.connect(
        [this](const uint_64 _loc, bool _connect, const char* _mesg) {
            if (0 == _loc) mesg_prit.exec(_mesg);
            else {
                comm_flag.exec(_loc, _connect);
                mesg_prit.exec(strutil::format("%1 %2", transmit::to_strip(_loc), _mesg));
            }
            on_trans_event(_loc, _connect);
        });
    m_trans->bytes_recv.connect(std::bind(&comm::on_bytes_recv, this, std::placeholders::_1, std::placeholders::_2));
}

alx::comm::~comm() {
    close();
    delete m_trans;
}

// open()/close() are serialized by convention, not by a lock: the caller's guarantee is what covers the hand-over below
bool alx::comm::open() {
    if (is_open()) return true;
    if (!is_valid()) return false;
    // a previous close(0) left the queue destroyed; reset() is what lifts that
    m_queue.reset();
    // set before the thread starts: worker_loop reads it as its loop condition
    m_opened.store(true);
    m_thread = new std::thread(std::bind(&comm::worker_loop, this));
    return true;
}

void alx::comm::close(uint_64 _loc) {
    if (!is_open()) return;
    if (_loc == 0) {
        // the worker's loop condition is is_open(): clearing it first is what lets the join return
        m_opened.store(false);
        std::thread* thd = m_thread;
        m_thread = nullptr;
        thd->join();
        delete thd;
        m_queue.destroy();
    } else m_queue.block_push(std::bind(&comm::close_comm_impl, this, _loc), m_queue_limit);
}

void alx::comm::clear() {
    m_queue.clear();
}

bool alx::comm::bytes_send(const bytes_view& _data, uint_64 _loc) {
    return is_open() ? (m_queue.block_push(std::bind(&comm::bytes_send_impl, this, _data, _loc), m_queue_limit), true) : false;
}

void alx::comm::worker_loop() {
    // ms: the worker waits sleep_base << sleep_zoom, capped at 1024 ms (transport down) and 64 ms (no connection)
    constexpr uint_64 sleep_base{4};
    constexpr uint_8 zoom_open_fail_max{8};
    constexpr uint_8 zoom_none_comm_max{4};
    uint_8 sleep_zoom{1};
    while (is_open()) {
        if (!m_trans->is_open()) {
            if (m_trans->open()) sleep_zoom = 0;
            else sleep_zoom = alx::min_value<uint_8>(sleep_zoom + 1, zoom_open_fail_max);
        } else if (m_trans->connect_num() == 0) {
            sleep_zoom = alx::min_value<uint_8>(sleep_zoom + 1, zoom_none_comm_max);
        } else {
            pump_queue();
            continue;
        }

        // zoom 0 means the open just succeeded: retry at once instead of sleeping
        if (0 != sleep_zoom)
            high_precision_sleep(sleep_base << sleep_zoom);
        else continue;
    }
    if (m_trans->is_open()) m_trans->close();
}

void alx::comm::pump_queue() {
    std::function<void()> task;
    while (m_queue.block_pop(task, 10)) task();
}

void alx::comm::close_comm_impl(uint_64 _loc) {
    return m_trans->close(_loc);
}

void alx::comm::bytes_send_impl(const bytes_view _data, uint_64 _loc) {
    return m_trans->bytes_send(_data, _loc) ? void() : mesg_prit.exec("pack send failed");
}
