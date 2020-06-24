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

template<class Queue>
struct dummy_tok_t {
    dummy_tok_t(Queue&) { }
};

template<class T, unsigned Capacity>
struct MoodyCamelQueue : moodycamel::ConcurrentQueue<T> {
    using producer_token_t = typename moodycamel::ConcurrentQueue<T>::producer_token_t;
    using consumer_token_t = typename moodycamel::ConcurrentQueue<T>::consumer_token_t;

    MoodyCamelQueue(unsigned producerThreads) : moodycamel::ConcurrentQueue<T>(Capacity, producerThreads, 0) { }

    void push(producer_token_t& tok, T element) {
        while (!this->try_enqueue(tok, element))
            spin_loop_pause();
    }

    T pop(consumer_token_t& tok) {
        T element;
        while (!this->try_dequeue(tok, element))
            spin_loop_pause();
        return element;
    }
};

template<class T, unsigned Capacity>
struct MoodyCamelReaderWriterQueue : moodycamel::ReaderWriterQueue<T> {
    using producer_token_t = dummy_tok_t<MoodyCamelReaderWriterQueue<T, Capacity>>;
    using consumer_token_t = dummy_tok_t<MoodyCamelReaderWriterQueue<T, Capacity>>;

    MoodyCamelReaderWriterQueue(unsigned producerThreads) : moodycamel::ReaderWriterQueue<T>(Capacity) { }

    void push(producer_token_t, T element) {
        while (!this->try_enqueue(element))
            spin_loop_pause();
    }

    T pop(consumer_token_t) {
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
