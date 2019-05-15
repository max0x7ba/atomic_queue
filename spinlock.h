/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

#include <system_error>

#include <emmintrin.h>
#include <pthread.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Spinlock {
    pthread_spinlock_t s_;
public:
    Spinlock() {
        if(int e = ::pthread_spin_init(&s_, 0))
            throw std::system_error(e, std::system_category(), "Spinlock::Spinlock");
    }

    ~Spinlock() noexcept {
        ::pthread_spin_destroy(&s_);
    }

    void lock() {
        if(int e = ::pthread_spin_lock(&s_))
            throw std::system_error(e, std::system_category(), "Spinlock::lock");
    }

    void unlock() {
        if(int e = ::pthread_spin_unlock(&s_))
            throw std::system_error(e, std::system_category(), "Spinlock::unlock");
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SpinlockHle {
    int lock_ = 0;

#ifdef __gcc__
    static constexpr int HLE_ACQUIRE = __ATOMIC_HLE_ACQUIRE;
    static constexpr int HLE_RELEASE = __ATOMIC_HLE_RELEASE;
#else
    static constexpr int HLE_ACQUIRE = 0;
    static constexpr int HLE_RELEASE = 0;
#endif

public:
    void lock() {
        for(int expected = 0; !__atomic_compare_exchange_n(&lock_, &expected, 1, false, __ATOMIC_ACQUIRE | HLE_ACQUIRE, __ATOMIC_RELAXED); expected = 0)
            _mm_pause();
    }

    void unlock() {
        __atomic_store_n(&lock_, 0, __ATOMIC_RELEASE | HLE_RELEASE);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
