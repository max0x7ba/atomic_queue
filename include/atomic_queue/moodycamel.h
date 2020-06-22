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

template<class T, unsigned Capacity>
struct MoodyCamelQueue : moodycamel::ConcurrentQueue<T> {
    MoodyCamelQueue()
        : moodycamel::ConcurrentQueue<T>(Capacity) {
        ctok() = moodycamel::ConcurrentQueue<T>::consumer_token_t(*this);  // update instance
    }

    void push(T element) {
        // TODO: To be able to use try_enqueue properly, particularly with tokens,
        // requires the number of producer and consumer threads to be passed to the
        // constructor
        while (!this->try_enqueue(element))
            spin_loop_pause();
    }

    T pop() {
        T element;
        while (!this->try_dequeue(ctok(), element))
            spin_loop_pause();
        return element;
    }

private:
    moodycamel::ConcurrentQueue<T>::consumer_token_t& ctok() {
        // Assume just one instance is present at a time
        static thread_local moodycamel::ConcurrentQueue<T>::consumer_token_t ctok(*this);
        return ctok;
    }
};

template<class T, unsigned Capacity>
struct MoodyCamelReaderWriterQueue : moodycamel::ReaderWriterQueue<T> {
    MoodyCamelReaderWriterQueue()
        : moodycamel::ReaderWriterQueue<T>(Capacity) {}

    void push(T element) {
        while (!this->try_enqueue(element))
            spin_loop_pause();
    }

    T pop() {
        T element;
        while (!this->try_dequeue(element))
            spin_loop_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MOODYCAMEL_H_INCLUDED
