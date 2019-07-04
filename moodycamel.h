/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef MOODYCAMEL_H_INCLUDED
#define MOODYCAMEL_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include <concurrentqueue/concurrentqueue.h>

#include <emmintrin.h> // _mm_pause

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned Capacity>
struct MoodyCamelQueue : moodycamel::ConcurrentQueue<T> {
    MoodyCamelQueue()
        : moodycamel::ConcurrentQueue<T>(Capacity) {}

    void push(T element) {
        while(!this->try_enqueue(element))
            _mm_pause();
    }

    T pop() {
        T element;
        while(!this->try_dequeue(element))
            _mm_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MOODYCAMEL_H_INCLUDED
