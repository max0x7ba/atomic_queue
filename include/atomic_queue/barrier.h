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
        release_counter = counter_.fetch_add(1, AR) - release_counter;
        ++release_counter;
        if(ATOMIC_QUEUE_LIKELY(release_counter < 0))
            do
                spin_loop_pause();
            while(counter_.load(A));
        else
            counter_.store(release_counter, R);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Barrier2 {
    std::atomic<int> counter = {};

    ATOMIC_QUEUE_INLINE int countdown() noexcept {
        // All callers execute the same code path.
        int caller_idx = counter.fetch_sub(1, std::memory_order_seq_cst) - 1;
        do
            spin_loop_pause();
        while(ATOMIC_QUEUE_LIKELY(counter.load(std::memory_order_seq_cst)));
        return caller_idx;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // BARRIER_H_INCLUDED
