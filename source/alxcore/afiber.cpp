/*****************************************************************/ /**
 * \file   afiber.cpp
 * \brief  Fiber support, lightweight coroutines
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afiber.h"

#include "alogger.h"

#ifdef _WIN32
#    include <Windows.h>
#else
#    include <cstdlib>
#    include <sys/mman.h>
#    include <ucontext.h>
#    include <unistd.h>
#endif

using namespace alx;

thread_local fiber* main_fiber_ = nullptr;
thread_local fiber* current_fiber_ = nullptr;

#ifndef _WIN32
namespace {

    // create(_func) with no explicit size -- Windows lets the OS pick, POSIX has to name one
    constexpr uint_64 default_stack_size = 1024 * 1024;
}
#endif

#ifdef _WIN32

fiber::fiber()
    : context_(ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH)), state_(FIBER_RUNNING), func_(nullptr) {
}

fiber::fiber(std::function<void()> _func, uint_64 _commit_size, uint_64 _reserve_size)
    : context_(CreateFiberEx(_commit_size, _reserve_size, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE) fiber_entry, this)), state_(FIBER_READY), func_(_func) {
}

fiber::~fiber() {
    if (this == main_fiber_) ConvertFiberToThread();
    else DeleteFiber(context_);
}

void fiber::initialize() {
    if (nullptr == main_fiber_) {
        main_fiber_ = new fiber();
        current_fiber_ = main_fiber_;
    }
}

void fiber::deinitialize() {
    delete main_fiber_;
    main_fiber_ = nullptr;
    current_fiber_ = nullptr;
}

fiber* fiber::create(std::function<void()> _func, uint_64 _commit_size, uint_64 _reserve_size) {
    if (_commit_size > _reserve_size) {
        log_error() << "fiber _commit_size > _reserve_size";
        return nullptr;
    }
    if (nullptr == main_fiber_) initialize();
    return new fiber(_func, _commit_size, _reserve_size);
}

void fiber::yield() {
    if (nullptr == main_fiber_ || main_fiber_ == current_fiber_) return;

    if (FIBER_FINISHED != current_fiber_->state_) current_fiber_->state_ = FIBER_SUSPENDED;

    main_fiber_->state_ = FIBER_RUNNING;
    current_fiber_ = main_fiber_;

    // SwitchToFiber saves the caller into its own fiber object, so there is no context to save here
    SwitchToFiber(main_fiber_->context_);
}

void fiber::yield(fiber* _fiber) {
    if (nullptr == _fiber || _fiber == current_fiber_) return;
    if (nullptr == main_fiber_) initialize();

    if (FIBER_FINISHED == _fiber->state_) _fiber = main_fiber_;
    if (FIBER_FINISHED != current_fiber_->state_) current_fiber_->state_ = FIBER_SUSPENDED;

    _fiber->state_ = FIBER_RUNNING;
    current_fiber_ = _fiber;

    SwitchToFiber(_fiber->context_);
}

bool fiber::is_fiber() {
    return IsThreadAFiber() == TRUE;
}

void fiber::fiber_entry() {
    fiber* current = (fiber*) GetFiberData();
    if (nullptr != current && nullptr != current->func_) {
        try {
            current->func_();
        } catch (...) {
            log_error() << "fiber function threw an exception";
        }
        current->state_ = FIBER_FINISHED;
    }
    // a fiber function must not return -- that ends the thread; park here instead
    while (true) yield();
}

#else

fiber::fiber()
    : stack_ptr_(nullptr), stack_size_(0), state_(FIBER_RUNNING), func_(nullptr) {
    // main's ctx_ needs a value before it can switch out; the first switch out re-saves the resume point
    getcontext(&ctx_);
}

fiber::fiber(std::function<void()> _func, uint_64 _commit_size, uint_64 _reserve_size)
    : stack_ptr_(nullptr), stack_size_(0), state_(FIBER_READY), func_(_func) {

    stack_size_ = (_commit_size > 0) ? _commit_size : default_stack_size;

    const uint_64 page = (uint_64) sysconf(_SC_PAGESIZE);
    // mmap, not malloc: the guard page belongs to the same mapping, and the one munmap below frees both
    void* raw = mmap(nullptr, stack_size_ + page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    stack_ptr_ = raw;
    mprotect(stack_ptr_, (size_t) page, PROT_NONE);

    getcontext(&ctx_);
    ctx_.uc_stack.ss_sp = (uint_8*) stack_ptr_ + page;
    ctx_.uc_stack.ss_size = stack_size_;
    // never fires: fiber_entry never returns, it parks in a yield loop
    ctx_.uc_link = &main_fiber_->ctx_;

    makecontext(&ctx_, (void (*)()) fiber_entry, 0);
}

fiber::~fiber() {
    if (stack_ptr_) {
        munmap(stack_ptr_, stack_size_ + (uint_64) sysconf(_SC_PAGESIZE));
        stack_ptr_ = nullptr;
    }
}

void fiber::initialize() {
    if (nullptr == main_fiber_) {
        main_fiber_ = new fiber();
        current_fiber_ = main_fiber_;
    }
}

void fiber::deinitialize() {
    delete main_fiber_;
    main_fiber_ = nullptr;
    current_fiber_ = nullptr;
}

fiber* fiber::create(std::function<void()> _func, uint_64 _commit_size, uint_64 _reserve_size) {
    if (_commit_size > _reserve_size) {
        log_error() << "fiber _commit_size > _reserve_size";
        return nullptr;
    }
    if (nullptr == main_fiber_) initialize();
    return new fiber(_func, _commit_size, _reserve_size);
}

void fiber::yield() {
    if (nullptr == main_fiber_ || main_fiber_ == current_fiber_) return;

    if (FIBER_FINISHED != current_fiber_->state_) current_fiber_->state_ = FIBER_SUSPENDED;

    main_fiber_->state_ = FIBER_RUNNING;
    fiber* old_fiber = current_fiber_;
    current_fiber_ = main_fiber_;

    swapcontext(&old_fiber->ctx_, &main_fiber_->ctx_);
}

void fiber::yield(fiber* _fiber) {
    if (nullptr == _fiber || _fiber == current_fiber_) return;
    if (nullptr == main_fiber_) initialize();

    if (FIBER_FINISHED == _fiber->state_) _fiber = main_fiber_;
    if (FIBER_FINISHED != current_fiber_->state_) current_fiber_->state_ = FIBER_SUSPENDED;

    _fiber->state_ = FIBER_RUNNING;
    fiber* old_fiber = current_fiber_;
    current_fiber_ = _fiber;

    swapcontext(&old_fiber->ctx_, &_fiber->ctx_);
}

bool fiber::is_fiber() {
    return main_fiber_ != nullptr;
}

void fiber::fiber_entry() {
    fiber* current = current_fiber_;

    if (nullptr != current && nullptr != current->func_) {
        try {
            current->func_();
        } catch (...) {
            log_error() << "fiber function threw an exception";
        }
        current->state_ = FIBER_FINISHED;
    }

    // returning would resume uc_link with current_fiber_ still pointing here; park and let yield() switch
    while (true) yield();
}

#endif

fiber* alx::fiber::main() {
    return main_fiber_;
}

fiber* alx::fiber::current() {
    return current_fiber_;
}