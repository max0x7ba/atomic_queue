/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

#include <atomic>
#include <cstdlib>
#include <mutex>

#include <pthread.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Spinlock {
    pthread_spinlock_t s_;

public:
    using scoped_lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept {
        if(ATOMIC_QUEUE_UNLIKELY(::pthread_spin_init(&s_, 0)))
            std::abort();
    }

    Spinlock(Spinlock const&) = delete;
    Spinlock& operator=(Spinlock const&) = delete;

    ~Spinlock() noexcept {
        ::pthread_spin_destroy(&s_);
    }

    void lock() noexcept {
        if(ATOMIC_QUEUE_UNLIKELY(::pthread_spin_lock(&s_)))
            std::abort();
    }

    void unlock() noexcept {
        if(ATOMIC_QUEUE_UNLIKELY(::pthread_spin_unlock(&s_)))
            std::abort();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TicketSpinlock {
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> ticket_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> next_{0};

public:
    class LockGuard {
        TicketSpinlock* const m_;
        unsigned const ticket_;
    public:
        LockGuard(TicketSpinlock& m) noexcept
            : m_(&m)
            , ticket_(m.lock())
        {}

        LockGuard(LockGuard const&) = delete;
        LockGuard& operator=(LockGuard const&) = delete;

        ~LockGuard() noexcept {
            m_->unlock(ticket_);
        }
    };

    using scoped_lock = LockGuard;

    TicketSpinlock() noexcept = default;
    TicketSpinlock(TicketSpinlock const&) = delete;
    TicketSpinlock& operator=(TicketSpinlock const&) = delete;

    ATOMIC_QUEUE_NOINLINE unsigned lock() noexcept {
        auto ticket = ticket_.fetch_add(1, std::memory_order_relaxed);
        for(;;) {
            auto position = ticket - next_.load(std::memory_order_acquire);
            if(ATOMIC_QUEUE_LIKELY(!position))
                break;
            do
                spin_loop_pause();
            while(--position);
        }
        return ticket;
    }

    void unlock() noexcept {
        unlock(next_.load(std::memory_order_relaxed) + 1);
    }

    void unlock(unsigned ticket) noexcept {
        next_.store(ticket + 1, std::memory_order_release);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class UnfairSpinlock {
    std::atomic<unsigned> lock_{0};

public:
    using scoped_lock = std::lock_guard<UnfairSpinlock>;

    UnfairSpinlock(UnfairSpinlock const&) = delete;
    UnfairSpinlock& operator=(UnfairSpinlock const&) = delete;

    void lock() noexcept {
        for(;;) {
            if(!lock_.load(std::memory_order_relaxed) && !lock_.exchange(1, std::memory_order_acquire))
                return;
            spin_loop_pause();
        }
    }

    void unlock() noexcept {
        lock_.store(0, std::memory_order_release);
    }
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
