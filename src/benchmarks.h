/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED
#define ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED

#include <utility>

#include "atomic_queue/defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Context {
    unsigned producers;
    unsigned consumers;
};

struct NoContext {
    template<class... Args>
    ATOMIC_QUEUE_INLINE constexpr NoContext(Args&&...) noexcept {}
};

template<class T> typename T::ContextType context_of_(int);
template<class T> NoContext context_of_(long);
template<class T> using ContextOf = decltype(context_of_<T>(0));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NoToken {
    template<class... Args>
    ATOMIC_QUEUE_INLINE constexpr NoToken(Args&&...) noexcept {}

    template<class Queue, class T>
    ATOMIC_QUEUE_INLINE static void push(Queue& q, T&& element) noexcept {
        q.push(std::forward<T>(element));
    }

    template<class Queue>
    ATOMIC_QUEUE_INLINE static auto pop(Queue& q) noexcept {
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

template<class Queue, size_t Capacity>
struct CapacityArgAdaptor : Queue {
    ATOMIC_QUEUE_INLINE CapacityArgAdaptor()
        : Queue(Capacity)
    {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct RetryDecorator : Queue {
    using T = typename Queue::value_type;

    using Queue::Queue;

    ATOMIC_QUEUE_INLINE void push(T element) noexcept {
        while(!this->try_push(element))
            spin_loop_pause();
    }

    template<class InputIt>
    ATOMIC_QUEUE_INLINE InputIt push(InputIt first, InputIt const last) noexcept {
        while(last != (first = this->try_push(first, last))) {
            spin_loop_pause();
        }
        return first;
    }

    ATOMIC_QUEUE_INLINE T pop() noexcept {
        T element;
        while(!this->try_pop(element))
            spin_loop_pause();
        return element;
    }

    template<class OutputIt>
    ATOMIC_QUEUE_INLINE OutputIt pop(OutputIt first, int n) noexcept {
        while(n -= this->try_pop(first, n)) {
            spin_loop_pause();
        }
        return first;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_BENCHMARKS_H_INCLUDED
