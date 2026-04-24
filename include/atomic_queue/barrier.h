/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef BARRIER_H_INCLUDED
#define BARRIER_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Barrier {
    std::atomic<int> counter_ = {};

public:
    ATOMIC_QUEUE_INLINE void wait() noexcept {
        counter_.fetch_add(1, X);
        while(counter_.load(A))
            spin_loop_pause();
    }

    ATOMIC_QUEUE_INLINE void release(int expected_counter) noexcept {
        while(expected_counter != counter_.load(X))
            spin_loop_pause();
        counter_.store(0, R);
    }

    ATOMIC_QUEUE_INLINE void wait_or_release(int release_counter) noexcept {
#if ATOMIC_QUEUE_FULL_THROTTLE
        asm("":"+r"(release_counter)); // Disable constant propagation for release_counter.
#endif
        int c = (counter_.fetch_add(1, A) + 1) - release_counter;
        if(ATOMIC_QUEUE_LIKELY(c < 0))
            while(counter_.load(A))
                spin_loop_pause();
        else
            counter_.store(c, R);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // BARRIER_H_INCLUDED
