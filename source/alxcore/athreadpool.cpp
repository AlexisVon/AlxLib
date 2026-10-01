/*****************************************************************/ /**
 * \file   athreadpool.cpp
 * \brief  Thread pool implementation
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "athreadpool.h"

alx::threadpool::threadpool(uint_64 _threads, uint_64 _queue_limit)
    : pool_size(0 == _threads ? 1 : _threads), max_queue_size(0 == _queue_limit ? 1 : _queue_limit), in_flight(0) {
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    for (uint_64 i = 0; i != pool_size; ++i)
        start_worker(i, lock);
}

alx::threadpool::~threadpool() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
    pool_size = 0;
    condition_consumers.notify_all();
    condition_producers.notify_all();
    // the workers detach rather than get joined; this wait keeps the pool alive until the last one has returned
    condition_consumers.wait(lock, [this] { return this->workers.empty(); });
}

void alx::threadpool::wait_until_empty() {
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    this->condition_producers.wait(lock, [this] { return this->tasks.empty(); });
}

void alx::threadpool::wait_until_nothing_in_flight() {
    std::unique_lock<std::mutex> lock(this->in_flight_mutex);
    this->in_flight_condition.wait(lock, [this] { return 0 == this->in_flight; });
}

void alx::threadpool::set_pool_size(uint_64 _limit) {
    if (stop) return;
    if (_limit < 1) _limit = 1;
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    uint_64 const old_size = pool_size;
    pool_size = _limit;
    if (pool_size > old_size)
        for (uint_64 i = old_size; i != pool_size; ++i) start_worker(i, lock);
    else if (pool_size < old_size) this->condition_consumers.notify_all();
}

void alx::threadpool::set_queue_size_limit(uint_64 _limit) {
    if (stop) return;
    if (_limit < 1) _limit = 1;
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    uint_64 const old_limit = max_queue_size;
    max_queue_size = _limit;
    if (old_limit < max_queue_size) {
        condition_producers.notify_all();
    }
}

// _lock is unused here -- the caller holds queue_mutex, and the new worker needs it before it can read the pool
void alx::threadpool::start_worker(uint_64 _worker_number, std::unique_lock<std::mutex> const& _lock) {
    auto worker_place = std::bind(worker_func, this, _worker_number);
    if (_worker_number < this->workers.size()) {
        std::thread& worker = this->workers[_worker_number];
        if (!worker.joinable()) worker = std::thread(worker_place);
    } else this->workers.push_back(std::thread(worker_place));
}

void alx::threadpool::worker_func(threadpool* _this, const uint_64 _worker_number) {
    while (true) {
        std::function<void()> task;
        bool notify;
        // queue_mutex is dropped before task() runs, so a task may itself enqueue or wait on the pool
        {
            std::unique_lock<std::mutex> lock(_this->queue_mutex);
            // last clause: pool_size now sits past this worker's index, i.e. set_pool_size() retired it
            _this->condition_consumers.wait(lock, [_this, _worker_number] {
                return _this->stop || !_this->tasks.empty() || _this->pool_size < _worker_number + 1;
            });

            if ((_this->stop && _this->tasks.empty()) ||
                (!_this->stop && _this->pool_size < _worker_number + 1)) {
                // detach self first: a non-joinable slot is what start_worker() reuses, and only then drop the dead tail
                _this->workers[_worker_number].detach();
                while (_this->workers.size() > _this->pool_size &&
                       !_this->workers.back().joinable()) {
                    _this->workers.pop_back();
                }
                if (_this->workers.empty()) {
                    _this->condition_consumers.notify_all();
                }
                return;
            } else if (!_this->tasks.empty()) {
                task = std::move(_this->tasks.front());
                _this->tasks.pop();
                // size+1 is the pre-pop depth (a producer waits only on a full queue); wait_until_empty() listens here too
                notify = _this->tasks.size() + 1 == _this->max_queue_size || _this->tasks.empty();
            } else {
                continue;
            }
        }
        handle_in_flight_decrement guard(*_this);

        if (notify) {
            std::unique_lock<std::mutex> lock(_this->queue_mutex);
            _this->condition_producers.notify_all();
        }
        task();
    }
}
