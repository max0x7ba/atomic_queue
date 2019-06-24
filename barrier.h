/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef BARRIER_H_INCLUDED
#define BARRIER_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Barrier {
    std::atomic<unsigned> counter_ = {};

public:
    void wait() noexcept {
        counter_.fetch_add(1, std::memory_order_acquire);
        while(counter_.load(std::memory_order_relaxed))
            _mm_pause();
    }

    void release(unsigned expected_counter) noexcept {
        while(expected_counter != counter_.load(std::memory_order_relaxed))
            _mm_pause();
        counter_.store(0, std::memory_order_release);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // BARRIER_H_INCLUDED
