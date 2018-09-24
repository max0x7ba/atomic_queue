/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
#define ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED

#include <system_error>
#include <mutex>

#include <pthread.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SpinLock {
    pthread_spinlock_t s_;
public:
    SpinLock() {
        if(int e = ::pthread_spin_init(&s_, 0))
            throw std::system_error(e, std::system_category(), "SpinLock::SpinLock");
    }

    ~SpinLock() noexcept {
        ::pthread_spin_destroy(&s_);
    }

    void lock() {
        if(int e = ::pthread_spin_lock(&s_))
            throw std::system_error(e, std::system_category(), "SpinLock::lock");
    }

    void unlock() {
        if(int e = ::pthread_spin_unlock(&s_))
            throw std::system_error(e, std::system_category(), "SpinLock::unlock");
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_SPIN_LOCK_H_INCLUDED
