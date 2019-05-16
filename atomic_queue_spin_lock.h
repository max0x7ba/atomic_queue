/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

#include "atomic_queue.h"
#include "spinlock.h"

#include <cassert>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Put the members into different cache lines, so that writers dont't contend with reader for head_/tail_.
struct AtomicQueueSpinlockBase {
    alignas(CACHE_LINE_SIZE) Spinlock lock_;
    alignas(CACHE_LINE_SIZE) unsigned head_ = 0;
    alignas(CACHE_LINE_SIZE) unsigned tail_ = 0;
};

// Intel Hardware Lock Elision favours minimum cache line number usage, hence, put all of these members into the same cache line.
struct AtomicQueueSpinlockHleBase {
    SpinlockHle lock_;
    unsigned head_ = 0;
    unsigned tail_ = 0;
};

template<class T, unsigned SIZE, class Base>
class AtomicQueueSpinlock_ : Base {
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

public:
    using value_type = T;

    template<class U>
    bool try_push(U&& element) noexcept {
        this->lock_.lock();
        if(this->head_ - this->tail_ < SIZE) {
            q_[this->head_ % SIZE] = std::forward<U>(element);
            ++this->head_;
            this->lock_.unlock();
            return true;
        }
        this->lock_.unlock();
        return false;
    }

    bool try_pop(T& element) noexcept {
        this->lock_.lock();
        if(this->head_ != this->tail_) {
            element = std::move(q_[this->tail_ % SIZE]);
            ++this->tail_;
            this->lock_.unlock();
            return true;
        }
        this->lock_.unlock();
        return false;
    }

    bool was_empty() const noexcept {
        return static_cast<int>(this->head_ - this->tail_) <= 0;
    }

    bool was_full() const noexcept {
        return static_cast<int>(this->head_ - this->tail_) >= static_cast<int>(SIZE);
    }
};

template<class T, unsigned SIZE>
using AtomicQueueSpinlock = AtomicQueueSpinlock_<T, SIZE, AtomicQueueSpinlockBase>;

template<class T, unsigned SIZE>
using AtomicQueueSpinlockHle = AtomicQueueSpinlock_<T, SIZE, AtomicQueueSpinlockHleBase>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
