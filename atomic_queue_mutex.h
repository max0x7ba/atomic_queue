/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

#include "atomic_queue.h"
#include "spinlock.h"

#include <cassert>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Mutex, unsigned SIZE, bool MinimizeContention>
class AtomicQueueMutexT {
    Mutex mutex_;
    unsigned head_ = 0;
    unsigned tail_ = 0;
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

    using Remap = typename details::GetIndexShuffleBits<MinimizeContention, SIZE, CACHE_LINE_SIZE / sizeof(T)>::type;
    using ScopedLock = typename Mutex::scoped_lock;

public:
    using value_type = T;

    template<class U>
    bool try_push(U&& element) noexcept {
        ScopedLock lock(this->mutex_);
        if(this->head_ - this->tail_ < SIZE) {
            q_[details::remap_index(this->head_ % SIZE, Remap{})] = std::forward<U>(element);
            ++this->head_;
            return true;
        }
        return false;
    }

    bool try_pop(T& element) noexcept {
        ScopedLock lock(this->mutex_);
        if(this->head_ != this->tail_) {
            element = std::move(q_[details::remap_index(this->tail_ % SIZE, Remap{})]);
            ++this->tail_;
            return true;
        }
        return false;
    }

    bool was_empty() const noexcept {
        return static_cast<int>(this->head_ - this->tail_) <= 0;
    }

    bool was_full() const noexcept {
        return static_cast<int>(this->head_ - this->tail_) >= static_cast<int>(SIZE);
    }
};

template<class T, unsigned SIZE, class Mutex, bool MinimizeContention = details::IsPowerOf2<SIZE>::value>
using AtomicQueueMutex = AtomicQueueMutexT<T, Mutex, SIZE, MinimizeContention>;

template<class T, unsigned SIZE, bool MinimizeContention = details::IsPowerOf2<SIZE>::value>
using AtomicQueueSpinlock = AtomicQueueMutexT<T, Spinlock, SIZE, MinimizeContention>;

template<class T, unsigned SIZE, bool MinimizeContention = details::IsPowerOf2<SIZE>::value>
using AtomicQueueSpinlockHle = AtomicQueueMutexT<T, SpinlockHle, SIZE, MinimizeContention>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
