/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED
#define ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED

#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Context {
    unsigned producers;
    unsigned consumers;
};

struct NoContext {
    template<class... Args>
    constexpr NoContext(Args&&...) noexcept {}
};

template<class T> typename T::ContextType context_of_(int);
template<class T> NoContext context_of_(long);
template<class T> using ContextOf = decltype(context_of_<T>(0));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NoToken {
    template<class... Args>
    constexpr NoToken(Args&&...) noexcept {}

    template<class Queue, class T>
    static void push(Queue& q, T&& element) noexcept {
        q.push(std::forward<T>(element));
    }

    template<class Queue>
    static auto pop(Queue& q) noexcept {
        return q.pop();
    }
};

template<class T> typename T::Producer producer_of_(int);
template<class T> NoToken producer_of_(long);
template<class T> using ProducerOf = decltype(producer_of_<T>(1));

template<class T> typename T::Consumer consumer_of_(int);
template<class T> NoToken consumer_of_(long);
template<class T> using ConsumerOf = decltype(consumer_of_<T>(1));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED
