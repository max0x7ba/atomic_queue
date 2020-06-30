/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef MOODYCAMEL_H_INCLUDED
#define MOODYCAMEL_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "benchmarks.h"

#include <concurrentqueue/concurrentqueue.h>
#include <readerwriterqueue/readerwriterqueue.h>

#include "atomic_queue/defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned Capacity>
struct MoodyCamelQueue : moodycamel::ConcurrentQueue<T> {
    using producer_token_t = typename moodycamel::ConcurrentQueue<T>::producer_token_t;
    using consumer_token_t = typename moodycamel::ConcurrentQueue<T>::consumer_token_t;

    using ContextType = Context;

    struct Producer {
        producer_token_t t_;
        Producer(MoodyCamelQueue& q) noexcept : t_(q) {}
        void push(MoodyCamelQueue& q, T element) { q.push(t_, element); }
    };

    struct Consumer {
        consumer_token_t t_;
        Consumer(MoodyCamelQueue& q) noexcept : t_(q) {}
        T pop(MoodyCamelQueue& q) { return q.pop(t_); }
    };

    MoodyCamelQueue(Context context)
        : moodycamel::ConcurrentQueue<T>(Capacity, context.producers, 0)
    {}

    void push(producer_token_t& tok, T element) noexcept {
        while(!this->try_enqueue(tok, element))
            spin_loop_pause();
    }

    T pop(consumer_token_t& tok) noexcept {
        T element;
        while(!this->try_dequeue(tok, element))
            spin_loop_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned Capacity>
struct MoodyCamelReaderWriterQueue : moodycamel::ReaderWriterQueue<T> {
    MoodyCamelReaderWriterQueue()
        : moodycamel::ReaderWriterQueue<T>(Capacity)
    {}

    void push(T element) noexcept {
        while(!this->try_enqueue(element))
            spin_loop_pause();
    }

    T pop() noexcept {
        T element;
        while(!this->try_dequeue(element))
            spin_loop_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MOODYCAMEL_H_INCLUDED
