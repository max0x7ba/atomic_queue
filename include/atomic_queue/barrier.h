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
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Barrier2 {
    std::atomic<unsigned> counter = {};

    ATOMIC_QUEUE_INLINE unsigned countdown() noexcept {
        unsigned caller_idx = --counter; // std::memory_order_seq_cst

        // All callers execute the same code path.
        do spin_loop_pause();
        while(counter.load()); // std::memory_order_seq_cst

        return caller_idx;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // BARRIER_H_INCLUDED
