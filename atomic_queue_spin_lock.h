/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

#include "spinlock.h"
#include "atomic_queue.h"

#include <cassert>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class AtomicQueueSpinLock {
    SpinLock s_;
    unsigned head_ = 0;
    unsigned tail_ = 0;
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

    using LockGuard = std::lock_guard<SpinLock>;

public:
    using value_type = T;

    template<class U>
    bool try_push(U&& element) {
        LockGuard lock(s_);
        if(head_ - tail_ < SIZE) {
            q_[head_ % SIZE] = std::forward<U>(element);
            ++head_;
            return true;
        }
        return false;
    }

    bool try_pop(T& element) {
        LockGuard lock(s_);
        if(head_ != tail_) {
            element = std::move(q_[tail_ % SIZE]);
            ++tail_;
            return true;
        }
        return false;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
