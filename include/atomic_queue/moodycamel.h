/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef MOODYCAMEL_H_INCLUDED
#define MOODYCAMEL_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include <concurrentqueue/concurrentqueue.h>
#include <readerwriterqueue/readerwriterqueue.h>

#include "defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned Capacity, class Queue>
struct MoodyCamelAdapter : Queue {
    MoodyCamelAdapter()
        : Queue(Capacity) {}

    void push(T element) {
        while(!this->try_enqueue(element))
            spin_loop_pause();
    }

    T pop() {
        T element;
        while(!this->try_dequeue(element))
            spin_loop_pause();
        return element;
    }
};

template<class T, unsigned Capacity>
using MoodyCamelQueue = MoodyCamelAdapter<T, Capacity, moodycamel::ConcurrentQueue<T>>;

template<class T, unsigned Capacity>
using MoodyCamelReaderWriterQueue = MoodyCamelAdapter<T, Capacity, moodycamel::ReaderWriterQueue<T>>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MOODYCAMEL_H_INCLUDED
